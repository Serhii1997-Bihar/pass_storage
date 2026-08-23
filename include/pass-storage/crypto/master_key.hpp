#pragma once

#include <string_view>
#include <span>
#include <cstdint>
#include <botan/secmem.h>

class Master_Key_Generator {
public:
    static Botan::secure_vector<uint8_t> derive_key(std::string_view password, std::span<const uint8_t> salt);
};