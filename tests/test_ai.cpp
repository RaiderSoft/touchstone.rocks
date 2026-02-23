#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>

#include <curl/curl.h>

#include "ai_provider.h"
#include "dotenv.h"

// ---------------------------------------------------------------------------
// dotenv tests
// ---------------------------------------------------------------------------

TEST(DotEnv, LoadsVariablesFromFile) {
  // Write a temporary .env file.
  {
    std::ofstream f("test_dotenv_tmp.env");
    f << "# comment line\n";
    f << "TEST_DOTENV_VAR_A=hello\n";
    f << "TEST_DOTENV_VAR_B=\"quoted value\"\n";
    f << "TEST_DOTENV_VAR_C='single quoted'\n";
    f << "  TEST_DOTENV_VAR_D = spaced  \n";
    f << "\n";
  }

  int loaded = dotenv::Load("test_dotenv_tmp.env");
  EXPECT_GE(loaded, 4);

  EXPECT_STREQ(std::getenv("TEST_DOTENV_VAR_A"), "hello");
  EXPECT_STREQ(std::getenv("TEST_DOTENV_VAR_B"), "quoted value");
  EXPECT_STREQ(std::getenv("TEST_DOTENV_VAR_C"), "single quoted");
  EXPECT_STREQ(std::getenv("TEST_DOTENV_VAR_D"), "spaced");

  // Clean up.
  std::remove("test_dotenv_tmp.env");
  unsetenv("TEST_DOTENV_VAR_A");
  unsetenv("TEST_DOTENV_VAR_B");
  unsetenv("TEST_DOTENV_VAR_C");
  unsetenv("TEST_DOTENV_VAR_D");
}

TEST(DotEnv, DoesNotOverrideExisting) {
  setenv("TEST_DOTENV_EXISTING", "original", 1);

  {
    std::ofstream f("test_dotenv_tmp2.env");
    f << "TEST_DOTENV_EXISTING=overwritten\n";
  }

  dotenv::Load("test_dotenv_tmp2.env");
  EXPECT_STREQ(std::getenv("TEST_DOTENV_EXISTING"), "original");

  std::remove("test_dotenv_tmp2.env");
  unsetenv("TEST_DOTENV_EXISTING");
}

TEST(DotEnv, MissingFileReturnsNegative) {
  int result = dotenv::Load("nonexistent_file_12345.env");
  EXPECT_EQ(result, -1);
}

// ---------------------------------------------------------------------------
// Provider creation tests
// ---------------------------------------------------------------------------

TEST(ProviderCreation, AnthropicProviderRequiresKey) {
  // Save and clear any existing key.
  const char* saved = std::getenv("CLAUDE_API_KEY");
  std::string saved_str = saved ? saved : "";
  unsetenv("CLAUDE_API_KEY");

  auto provider = ai::CreateAnthropicProvider();
  EXPECT_EQ(provider, nullptr);

  // Restore.
  if (!saved_str.empty()) {
    setenv("CLAUDE_API_KEY", saved_str.c_str(), 1);
  }
}

TEST(ProviderCreation, GeminiProviderRequiresKey) {
  const char* saved = std::getenv("GEMINI_API_KEY");
  std::string saved_str = saved ? saved : "";
  unsetenv("GEMINI_API_KEY");

  auto provider = ai::CreateGeminiProvider();
  EXPECT_EQ(provider, nullptr);

  if (!saved_str.empty()) {
    setenv("GEMINI_API_KEY", saved_str.c_str(), 1);
  }
}

TEST(ProviderCreation, AnthropicProviderCreatedWithKey) {
  setenv("CLAUDE_API_KEY", "test-key-not-real", 1);
  auto provider = ai::CreateAnthropicProvider();
  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->name(), "Anthropic");
  EXPECT_FALSE(provider->models().empty());
  unsetenv("CLAUDE_API_KEY");
}

TEST(ProviderCreation, GeminiProviderCreatedWithKey) {
  setenv("GEMINI_API_KEY", "test-key-not-real", 1);
  auto provider = ai::CreateGeminiProvider();
  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->name(), "Google");
  EXPECT_FALSE(provider->models().empty());
  unsetenv("GEMINI_API_KEY");
}

// ---------------------------------------------------------------------------
// Service tests
// ---------------------------------------------------------------------------

TEST(Service, EmptyServiceNotAvailable) {
  ai::Service svc;
  EXPECT_FALSE(svc.Available());
  EXPECT_TRUE(svc.AvailableModels().empty());
}

TEST(Service, RoutesToCorrectProvider) {
  setenv("CLAUDE_API_KEY", "test-key", 1);
  setenv("GEMINI_API_KEY", "test-key", 1);

  ai::Service svc;
  svc.AddProvider(ai::CreateAnthropicProvider());
  svc.AddProvider(ai::CreateGeminiProvider());

  EXPECT_TRUE(svc.Available());
  auto models = svc.AvailableModels();
  EXPECT_GE(models.size(), 4u);  // 2 Anthropic + 2 Gemini

  unsetenv("CLAUDE_API_KEY");
  unsetenv("GEMINI_API_KEY");
}

TEST(Service, NextModelCycles) {
  setenv("CLAUDE_API_KEY", "test-key", 1);
  setenv("GEMINI_API_KEY", "test-key", 1);

  ai::Service svc;
  svc.AddProvider(ai::CreateAnthropicProvider());
  svc.AddProvider(ai::CreateGeminiProvider());

  auto models = svc.AvailableModels();
  ASSERT_GE(models.size(), 2u);

  std::string first = svc.CurrentModel();
  std::string second = svc.NextModel();
  EXPECT_NE(first, second);

  // Cycle through all models and back to first.
  std::string current = second;
  for (size_t i = 1; i < models.size(); i++) {
    current = svc.NextModel();
  }
  EXPECT_EQ(current, first);

  unsetenv("CLAUDE_API_KEY");
  unsetenv("GEMINI_API_KEY");
}

TEST(Service, CompleteFailsWithNoProvider) {
  ai::Service svc;
  ai::Request req;
  req.messages = {{"user", "hello"}};
  auto resp = svc.Complete(req);
  EXPECT_FALSE(resp.success);
  EXPECT_FALSE(resp.error.empty());
}

// ---------------------------------------------------------------------------
// Live integration tests (only run when API keys are available)
// ---------------------------------------------------------------------------

class LiveAnthropicTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!std::getenv("CLAUDE_API_KEY")) {
      GTEST_SKIP() << "CLAUDE_API_KEY not set, skipping live test";
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }
  void TearDown() override { curl_global_cleanup(); }
};

class LiveGeminiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!std::getenv("GEMINI_API_KEY")) {
      GTEST_SKIP() << "GEMINI_API_KEY not set, skipping live test";
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);
  }
  void TearDown() override { curl_global_cleanup(); }
};

TEST_F(LiveAnthropicTest, CompletionReturnsResponse) {
  auto provider = ai::CreateAnthropicProvider();
  ASSERT_NE(provider, nullptr);

  ai::Request req;
  req.model = provider->models().front();
  req.system_prompt = "Reply with exactly: PONG";
  req.messages = {{"user", "PING"}};
  req.max_tokens = 32;

  auto resp = provider->Complete(req);
  EXPECT_TRUE(resp.success) << "Error: " << resp.error;
  EXPECT_FALSE(resp.content.empty());
}

TEST_F(LiveGeminiTest, CompletionReturnsResponse) {
  auto provider = ai::CreateGeminiProvider();
  ASSERT_NE(provider, nullptr);

  ai::Request req;
  req.model = provider->models().front();
  req.system_prompt = "Reply with exactly: PONG";
  req.messages = {{"user", "PING"}};
  req.max_tokens = 32;

  auto resp = provider->Complete(req);
  EXPECT_TRUE(resp.success) << "Error: " << resp.error;
  EXPECT_FALSE(resp.content.empty());
}

TEST_F(LiveAnthropicTest, ServiceRoutesAnthropicModel) {
  ai::Service svc;
  svc.AddProvider(ai::CreateAnthropicProvider());

  ai::Request req;
  req.system_prompt = "Reply with exactly: OK";
  req.messages = {{"user", "test"}};
  req.max_tokens = 16;

  auto resp = svc.Complete(req);
  EXPECT_TRUE(resp.success) << "Error: " << resp.error;
  EXPECT_FALSE(resp.content.empty());
}

TEST_F(LiveGeminiTest, ServiceRoutesGeminiModel) {
  ai::Service svc;
  svc.AddProvider(ai::CreateGeminiProvider());

  ai::Request req;
  req.system_prompt = "Reply with exactly: OK";
  req.messages = {{"user", "test"}};
  req.max_tokens = 16;

  auto resp = svc.Complete(req);
  EXPECT_TRUE(resp.success) << "Error: " << resp.error;
  EXPECT_FALSE(resp.content.empty());
}
