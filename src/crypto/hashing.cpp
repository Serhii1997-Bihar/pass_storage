#include "pass-storage/crypto/hashing.hpp"

#include <charconv>
#include <format>
#include <memory>
#include <stdexcept>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

std::vector<unsigned char> Hashing::generate_salt() {
    std::vector<unsigned char> salt(16);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) throw std::runtime_error("Failed to generate random salt");

    return salt;
}

std::vector<unsigned char> Hashing::hash_data(std::string_view data, const std::vector<unsigned char>& salt) {
    using EVP_MD_CTX_ptr = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    EVP_MD_CTX_ptr ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);

    if (!ctx) throw std::runtime_error("Failed to create EVP_MD_CTX");
    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx.get(), salt.data(), salt.size()) != 1 ||
        EVP_DigestUpdate(ctx.get(), data.data(), data.size()) != 1) {
        throw std::runtime_error("Failed during digest computation");
    }

    std::vector<unsigned char> hash(EVP_MD_size(EVP_sha256()));
    unsigned int length = 0;

    if (EVP_DigestFinal_ex(ctx.get(), hash.data(), &length) != 1) throw std::runtime_error("Failed to finalize digest");
    hash.resize(length);

    return hash;
}

std::string Hashing::convert_bytes(const std::vector<unsigned char>& bytes) {
    std::string hex;
    hex.reserve(bytes.size() * 2);

    for (unsigned char b : bytes) {
        std::format_to(std::back_inserter(hex), "{:02x}", b);
    }

    return hex;
}

std::vector<unsigned char> Hashing::convert_hex(std::string_view hex) {
    if (hex.size() % 2 != 0) throw std::invalid_argument("Hex string length must be even");

    std::vector<unsigned char> bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned char byte_val = 0;
        auto [ptr, ec] = std::from_chars(hex.data() + i, hex.data() + i + 2, byte_val, 16);
        if (ec != std::errc{}) throw std::invalid_argument("Invalid hex character");

        bytes.push_back(byte_val);
    }

    return bytes;
}

bool Hashing::verify_data(std::string_view data, std::string_view expected_data, std::string_view salt_hex) {
    const std::vector<unsigned char> salt = convert_hex(salt_hex);
    const std::vector<unsigned char> computed_hash = hash_data(data, salt);
    const std::vector<unsigned char> expected_bytes = convert_hex(expected_data);

    if (computed_hash.size() != expected_bytes.size()) return false;

    return CRYPTO_memcmp(computed_hash.data(), expected_bytes.data(), computed_hash.size()) == 0;
}

std::pair<std::string, std::string> Hashing::get_data_and_salt(std::string_view password) {
    const std::vector<unsigned char> salt = generate_salt();
    const std::vector<unsigned char> password_hash = hash_data(password, salt);

    return {convert_bytes(password_hash), convert_bytes(salt)};
}