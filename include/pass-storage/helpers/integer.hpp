#pragma once

#include <string>
#include <algorithm>
#include <random>

inline std::string generate_password() {
    constexpr std::string_view lower = "abcdefghijklmnopqrstuvwxyz";
    constexpr std::string_view upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    constexpr std::string_view digits = "0123456789";
    constexpr std::string_view symbols = "!@#$%^&*()_+-=[]{}|;:,.<>?";
    constexpr std::string_view all_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{}|;:,.<>?";

    thread_local std::mt19937 gen(std::random_device{}());

    auto get_random_char = [](std::string_view charset) {
        std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);
        return charset[dist(gen)];
    };

    std::string password;
    password.reserve(12);

    password += get_random_char(lower);
    password += get_random_char(upper);
    password += get_random_char(digits);
    password += get_random_char(symbols);

    std::uniform_int_distribution<size_t> dist_all(0, all_chars.size() - 1);
    for (size_t i = 0; i < 8; ++i) {
        password += all_chars[dist_all(gen)];
    }

    std::ranges::shuffle(password, gen);

    return password;
}