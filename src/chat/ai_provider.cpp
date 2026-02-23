#include "chat/ai_provider.hpp"

#include "audio/audio_encoding.hpp"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <cstdlib>

namespace ai {

// ---------------------------------------------------------------------------
// libcurl helpers
// ---------------------------------------------------------------------------

static size_t WriteCallback(void* contents, size_t size, size_t nmemb,
                            void* userp) {
  auto* str = static_cast<std::string*>(userp);
  str->append(static_cast<char*>(contents), size * nmemb);
  return size * nmemb;
}

// ---------------------------------------------------------------------------
// Anthropic provider
// ---------------------------------------------------------------------------

class AnthropicProvider : public Provider {
 public:
  explicit AnthropicProvider(std::string api_key)
      : api_key_(std::move(api_key)) {}

  std::string name() const override { return "Anthropic"; }

  std::vector<std::string> models() const override {
    return {"claude-sonnet-4-6", "claude-haiku-4-5"};
  }

  Response Complete(const Request& request) override {
    Response result;

    if (request.messages.empty()) {
      result.error = "No messages provided";
      return result;
    }

    nlohmann::json messages_json = nlohmann::json::array();
    for (const auto& msg : request.messages) {
      messages_json.push_back({{"role", msg.role}, {"content", msg.content}});
    }

    nlohmann::json body = {{"model", request.model},
                           {"max_tokens", request.max_tokens},
                           {"messages", messages_json}};

    if (!request.system_prompt.empty()) {
      body["system"] = request.system_prompt;
    }

    std::string body_str = body.dump();

    CURL* curl = curl_easy_init();
    if (!curl) {
      result.error = "Failed to initialize libcurl";
      return result;
    }

    std::string response_body;

    struct curl_slist* headers = nullptr;
    std::string auth = "x-api-key: " + api_key_;
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      result.error =
          std::string("HTTP request failed: ") + curl_easy_strerror(res);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return result;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
      result.error =
          "API error " + std::to_string(http_code) + ": " + response_body;
      return result;
    }

    try {
      auto j = nlohmann::json::parse(response_body);
      if (j.contains("content") && j["content"].is_array() &&
          !j["content"].empty()) {
        auto& block = j["content"][0];
        if (block.contains("text")) {
          result.content = block["text"].get<std::string>();
          result.success = true;
        }
      }
      if (!result.success) {
        result.error = "Unexpected API response format";
      }
    } catch (const std::exception& e) {
      result.error = std::string("Failed to parse response: ") + e.what();
    }

    return result;
  }

 private:
  std::string api_key_;
};

// ---------------------------------------------------------------------------
// Gemini provider
// ---------------------------------------------------------------------------

class GeminiProvider : public Provider {
 public:
  explicit GeminiProvider(std::string api_key)
      : api_key_(std::move(api_key)) {}

  std::string name() const override { return "Google"; }

  std::vector<std::string> models() const override {
    return {"gemini-2.5-pro", "gemini-2.5-flash"};
  }

  Response Complete(const Request& request) override {
    Response result;

    if (request.messages.empty()) {
      result.error = "No messages provided";
      return result;
    }

    // Gemini uses "model" instead of "assistant".
    nlohmann::json contents = nlohmann::json::array();
    for (const auto& msg : request.messages) {
      std::string role = (msg.role == "assistant") ? "model" : msg.role;
      contents.push_back(
          {{"role", role}, {"parts", {{{"text", msg.content}}}}});
    }

    nlohmann::json gen_config = {{"maxOutputTokens", request.max_tokens}};

    nlohmann::json body = {{"contents", contents},
                           {"generationConfig", gen_config}};

    if (!request.system_prompt.empty()) {
      body["system_instruction"] = {
          {"parts", {{{"text", request.system_prompt}}}}};
    }

    std::string body_str = body.dump();

    // URL includes model name and API key as query param.
    std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/" +
        request.model + ":generateContent?key=" + api_key_;

    CURL* curl = curl_easy_init();
    if (!curl) {
      result.error = "Failed to initialize libcurl";
      return result;
    }

    std::string response_body;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      result.error =
          std::string("HTTP request failed: ") + curl_easy_strerror(res);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return result;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
      result.error =
          "API error " + std::to_string(http_code) + ": " + response_body;
      return result;
    }

    try {
      auto j = nlohmann::json::parse(response_body);
      if (j.contains("candidates") && j["candidates"].is_array() &&
          !j["candidates"].empty()) {
        auto& candidate = j["candidates"][0];
        if (candidate.contains("content") &&
            candidate["content"].contains("parts") &&
            candidate["content"]["parts"].is_array() &&
            !candidate["content"]["parts"].empty()) {
          result.content =
              candidate["content"]["parts"][0]["text"].get<std::string>();
          result.success = true;
        }
      }
      if (!result.success) {
        result.error = "Unexpected API response format";
      }
    } catch (const std::exception& e) {
      result.error = std::string("Failed to parse response: ") + e.what();
    }

    return result;
  }

  TranscribeResponse Transcribe(const TranscribeRequest& req) override {
    TranscribeResponse result;

    if (req.audio_wav.empty()) {
      result.error = "No audio data provided";
      return result;
    }

    std::string audio_b64 = base64::Encode(req.audio_wav);

    nlohmann::json body = {
        {"contents",
         {{{"parts",
            {{{"text",
               "Transcribe this audio. Return only the transcription text, "
               "nothing else."}},
             {{"inlineData",
               {{"mimeType", "audio/wav"}, {"data", audio_b64}}}}}}}}}};

    std::string body_str = body.dump();

    std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-2.5-flash:generateContent?key=" +
        api_key_;

    CURL* curl = curl_easy_init();
    if (!curl) {
      result.error = "Failed to initialize libcurl";
      return result;
    }

    std::string response_body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      result.error =
          std::string("HTTP request failed: ") + curl_easy_strerror(res);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return result;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
      result.error =
          "STT API error " + std::to_string(http_code) + ": " + response_body;
      return result;
    }

    try {
      auto j = nlohmann::json::parse(response_body);
      if (j.contains("candidates") && j["candidates"].is_array() &&
          !j["candidates"].empty()) {
        auto& candidate = j["candidates"][0];
        if (candidate.contains("content") &&
            candidate["content"].contains("parts") &&
            candidate["content"]["parts"].is_array() &&
            !candidate["content"]["parts"].empty()) {
          result.text =
              candidate["content"]["parts"][0]["text"].get<std::string>();
          result.success = true;
        }
      }
      if (!result.success) {
        result.error = "Unexpected STT response format";
      }
    } catch (const std::exception& e) {
      result.error = std::string("Failed to parse STT response: ") + e.what();
    }

    return result;
  }

  SpeakResponse Speak(const SpeakRequest& req) override {
    SpeakResponse result;

    if (req.text.empty()) {
      result.error = "No text provided";
      return result;
    }

    nlohmann::json body = {
        {"contents", {{{"parts", {{{"text", req.text}}}}}}},
        {"generationConfig",
         {{"responseModalities", {"AUDIO"}},
          {"speechConfig",
           {{"voiceConfig",
             {{"prebuiltVoiceConfig", {{"voiceName", req.voice}}}}}}}}}};

    std::string body_str = body.dump();

    std::string url =
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-2.5-flash-preview-tts:generateContent?key=" +
        api_key_;

    CURL* curl = curl_easy_init();
    if (!curl) {
      result.error = "Failed to initialize libcurl";
      return result;
    }

    std::string response_body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "content-type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      result.error =
          std::string("HTTP request failed: ") + curl_easy_strerror(res);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return result;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
      result.error =
          "TTS API error " + std::to_string(http_code) + ": " + response_body;
      return result;
    }

    try {
      auto j = nlohmann::json::parse(response_body);
      if (j.contains("candidates") && j["candidates"].is_array() &&
          !j["candidates"].empty()) {
        auto& candidate = j["candidates"][0];
        if (candidate.contains("content") &&
            candidate["content"].contains("parts") &&
            candidate["content"]["parts"].is_array() &&
            !candidate["content"]["parts"].empty()) {
          auto& part = candidate["content"]["parts"][0];
          if (part.contains("inlineData") &&
              part["inlineData"].contains("data")) {
            result.audio_base64 =
                part["inlineData"]["data"].get<std::string>();
            result.success = true;
          }
        }
      }
      if (!result.success) {
        result.error = "Unexpected TTS response format";
      }
    } catch (const std::exception& e) {
      result.error = std::string("Failed to parse TTS response: ") + e.what();
    }

    return result;
  }

 private:
  std::string api_key_;
};

// ---------------------------------------------------------------------------
// Factory functions
// ---------------------------------------------------------------------------

std::unique_ptr<Provider> CreateAnthropicProvider() {
  const char* key = std::getenv("CLAUDE_API_KEY");
  if (key && key[0] != '\0') {
    return std::make_unique<AnthropicProvider>(key);
  }
  return nullptr;
}

std::unique_ptr<Provider> CreateGeminiProvider() {
  const char* key = std::getenv("GEMINI_API_KEY");
  if (key && key[0] != '\0') {
    return std::make_unique<GeminiProvider>(key);
  }
  return nullptr;
}

// ---------------------------------------------------------------------------
// Service
// ---------------------------------------------------------------------------

void Service::AddProvider(std::unique_ptr<Provider> provider) {
  if (current_model_.empty() && !provider->models().empty()) {
    current_model_ = provider->models().front();
  }
  providers_.push_back(std::move(provider));
}

std::vector<std::string> Service::AvailableModels() const {
  std::vector<std::string> all;
  for (const auto& p : providers_) {
    for (const auto& m : p->models()) {
      all.push_back(m);
    }
  }
  return all;
}

Provider* Service::ProviderForModel(const std::string& model) const {
  for (const auto& p : providers_) {
    for (const auto& m : p->models()) {
      if (m == model) return p.get();
    }
  }
  return nullptr;
}

Response Service::Complete(const Request& request) {
  std::string model = request.model.empty() ? current_model_ : request.model;
  if (model.empty()) {
    return {false, "", "No model configured"};
  }

  Provider* provider = ProviderForModel(model);
  if (!provider) {
    return {false, "", "No provider for model: " + model};
  }

  Request resolved = request;
  resolved.model = model;
  return provider->Complete(resolved);
}

TranscribeResponse Service::Transcribe(const TranscribeRequest& request) {
  // Route to first provider that supports STT (Gemini).
  for (const auto& p : providers_) {
    auto result = p->Transcribe(request);
    if (result.error != "Not supported by this provider") {
      return result;
    }
  }
  return {false, "", "No provider supports STT"};
}

SpeakResponse Service::Speak(const SpeakRequest& request) {
  // Route to first provider that supports TTS (Gemini).
  for (const auto& p : providers_) {
    auto result = p->Speak(request);
    if (result.error != "Not supported by this provider") {
      return result;
    }
  }
  return {false, "", "No provider supports TTS"};
}

std::string Service::NextModel() {
  auto all = AvailableModels();
  if (all.empty()) return "";

  // Find current index, advance to next.
  for (size_t i = 0; i < all.size(); i++) {
    if (all[i] == current_model_) {
      current_model_ = all[(i + 1) % all.size()];
      return current_model_;
    }
  }

  current_model_ = all.front();
  return current_model_;
}

}  // namespace ai
