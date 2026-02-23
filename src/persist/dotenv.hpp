#pragma once

#include <string>

namespace dotenv {

// Reads a .env file and sets environment variables for any key=value pairs
// found. Does NOT override variables already set in the environment.
// Searches for .env in the current working directory.
// Returns the number of variables loaded, or -1 on file-not-found.
int Load(const std::string& path = ".env");

}  // namespace dotenv
