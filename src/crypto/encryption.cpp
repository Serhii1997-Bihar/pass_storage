#include "pass-storage/crypto/encryption.hpp"

#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <span>

#include <botan/system_rng.h>
#include <botan/aead.h>
#include <botan/kdf.h>
#include <botan/secmem.h>

std::vector<unsigned char> Encryption::derive_path_key(std::span<const unsigned char> master_key, std::string_view path_context) {
    const auto kdf = Botan::KDF::create("HKDF(SHA-256)");
    if (!kdf) throw std::runtime_error("Failed to create HKDF");

    auto key = kdf->derive_key(
        KEY_SIZE,
        std::span<const uint8_t>(master_key.data(), master_key.size()),
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(path_context.data()), path_context.size()),
        std::span<const uint8_t>()
    );

    return {key.begin(), key.end()};
}

std::vector<unsigned char> Encryption::encrypt(
    std::span<const unsigned char> plaintext,
    std::span<const unsigned char> master_key,
    const std::string_view path_context)
{
    const auto derived_key = derive_path_key(master_key, path_context);

    Botan::System_RNG rng;
    std::vector<uint8_t> iv(IV_SIZE);
    rng.randomize(iv.data(), iv.size());

    const auto enc = Botan::AEAD_Mode::create("AES-256/GCM", Botan::Cipher_Dir::Encryption);
    if (!enc) throw std::runtime_error("Failed to create AES-256-GCM cipher");

    enc->set_key(derived_key.data(), derived_key.size());

    if (!path_context.empty()) {
        enc->set_associated_data(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(path_context.data()), path_context.size()));
    }

    enc->start(iv.data(), iv.size());

    Botan::secure_vector<uint8_t> buffer(plaintext.begin(), plaintext.end());
    enc->finish(buffer);

    if (buffer.size() < TAG_SIZE) throw std::runtime_error("Encryption failed");

    const auto tag_start = buffer.end() - TAG_SIZE;

    std::vector<unsigned char> result;
    result.reserve(iv.size() + TAG_SIZE + (buffer.size() - TAG_SIZE));
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), tag_start, buffer.end());
    result.insert(result.end(), buffer.begin(), tag_start);

    return result;
}

std::vector<unsigned char> Encryption::decrypt(
    const std::span<const unsigned char> ciphertext_with_header,
    const std::span<const unsigned char> master_key,
    const std::string_view path_context
) {
    if (ciphertext_with_header.size() < IV_SIZE + TAG_SIZE) {
        throw std::invalid_argument("Ciphertext payload is too short");
    }

    const auto iv = ciphertext_with_header.subspan(0, IV_SIZE);
    auto tag = ciphertext_with_header.subspan(IV_SIZE, TAG_SIZE);
    auto ciphertext = ciphertext_with_header.subspan(IV_SIZE + TAG_SIZE);

    const std::vector<unsigned char> derived_key = derive_path_key(master_key, path_context);

    const auto dec = Botan::AEAD_Mode::create("AES-256/GCM", Botan::Cipher_Dir::Decryption);
    if (!dec) throw std::runtime_error("Failed to create AES-256-GCM cipher");

    dec->set_key(derived_key.data(), derived_key.size());

    if (!path_context.empty()) {
        dec->set_associated_data(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(path_context.data()), path_context.size()));
    }

    dec->start(iv.data(), iv.size());

    Botan::secure_vector<uint8_t> buffer;
    buffer.reserve(ciphertext.size() + tag.size());
    buffer.insert(buffer.end(), ciphertext.begin(), ciphertext.end());
    buffer.insert(buffer.end(), tag.begin(), tag.end());

    try {
        dec->finish(buffer);
    } catch (const std::exception&) {
        throw std::runtime_error("Authentication tag verification failed");
    }

    return {buffer.begin(), buffer.end()};
}