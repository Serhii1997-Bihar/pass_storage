#include "pass-storage/helpers/load_env.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {
    std::unordered_map<std::string, std::string> env_cache;
}

void load_env(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream line_stream(line);
        std::string key, value;
        if (std::getline(line_stream, key, '=') && std::getline(line_stream, value)) {
            env_cache[key] = value;
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