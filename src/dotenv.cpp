#include "dotenv.h"

#include <cstdlib>
#include <fstream>
#include <string>

namespace dotenv {

static std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

static std::string Unquote(const std::string& s) {
  if (s.size() >= 2) {
    char front = s.front();
    char back = s.back();
    if ((front == '"' && back == '"') || (front == '\'' && back == '\'')) {
      return s.substr(1, s.size() - 2);
    }
  }
  return s;
}

int Load(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) return -1;

  int count = 0;
  std::string line;
  while (std::getline(file, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#') continue;

    auto eq = line.find('=');
    if (eq == std::string::npos) continue;

    std::string key = Trim(line.substr(0, eq));
    std::string value = Trim(line.substr(eq + 1));
    value = Unquote(value);

    if (key.empty()) continue;

    // Don't override existing environment variables.
    if (std::getenv(key.c_str()) == nullptr) {
      setenv(key.c_str(), value.c_str(), 0);
      count++;
    }
  }

  return count;
}

}  // namespace dotenv
