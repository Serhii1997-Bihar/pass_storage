#pragma once

#include <string>

void load_env(const std::string& path = ".env");
std::string get_env_var(const std::string& key, const std::string& default_val = "");