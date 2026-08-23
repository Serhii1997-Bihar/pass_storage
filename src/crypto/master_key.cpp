#include "pass-storage/crypto/master_key.hpp"
#include <botan/pwdhash.h>
#include <stdexcept>

Botan::secure_vector<uint8_t> Master_Key_Generator::derive_key(std::string_view password, std::span<const uint8_t> salt) {
    auto family = Botan::PasswordHashFamily::create("Argon2id");
    if (!family) throw std::runtime_error("Argon2id unsupported");

    auto kdf = family->default_params();
    Botan::secure_vector<uint8_t> master_key(32);

    kdf->derive_key(master_key.data(), master_key.size(),
                    password.data(), password.size(),
                    salt.data(), salt.size());

    return master_key;
}