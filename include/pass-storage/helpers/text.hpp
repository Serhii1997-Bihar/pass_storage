#pragma once

#include <string>
#include <algorithm>
#include <cctype>

inline std::string convert_lower(std::string str) {
    std::ranges::transform(str, str.begin(), [](unsigned char result) { return std::tolower(result); });
    return str;
}

inline std::string convert_upper(std::string str) {
    std::ranges::transform(str, str.begin(), [](unsigned char result) { return std::toupper(result); });
    return str;
}