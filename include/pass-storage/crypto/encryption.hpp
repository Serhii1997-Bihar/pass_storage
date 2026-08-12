#pragma once

#include <string_view>
#include <vector>
#include <span>

class Encryption {
public:
    static constexpr size_t KEY_SIZE = 32;
    static constexpr size_t IV_SIZE = 12;
    static constexpr size_t TAG_SIZE = 16;

    static std::vector<unsigned char> derive_path_key(
        std::span<const unsigned char> master_key,
        std::string_view path_context
    );

    static std::vector<unsigned char> encrypt(
        std::span<const unsigned char> plaintext,
        std::span<const unsigned char> master_key,
        std::string_view path_context
    );

    static std::vector<unsigned char> decrypt(
        std::span<const unsigned char> ciphertext_with_header,
        std::span<const unsigned char> master_key,
        std::string_view path_context
    );
};