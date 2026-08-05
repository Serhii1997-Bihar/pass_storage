#include "pass-storage/helpers/load_env.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace {
    std::unordered_map<std::string, std::string> env_cache;

    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }
}

void load_env(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        file.open("../.env");
        if (!file.is_open()) {
            return;
        }
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed_line = trim(line);
        if (trimmed_line.empty() || trimmed_line[0] == '#') {
            continue;
        }

        size_t delimiter_pos = trimmed_line.find('=');
        if (delimiter_pos != std::string::npos) {
            std::string key = trim(trimmed_line.substr(0, delimiter_pos));
            std::string value = trim(trimmed_line.substr(delimiter_pos + 1));

            if (!key.empty()) {
                env_cache[key] = value;
            }
        }
    }
}

std::string get_env_var(const std::string& key, const std::string& default_val) {
    auto it = env_cache.find(key);
    if (it != env_cache.end()) {
        return it->second;
    }
    return default_val;
}