#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <utility>

class Hashing {
public:
    static std::pair<std::string, std::string> get_data_and_salt(std::string_view password);
    static bool verify_data(std::string_view data, std::string_view expected_data, std::string_view salt_hex);

private:
    static std::vector<unsigned char> generate_salt();
    static std::vector<unsigned char> hash_data(std::string_view input, const std::vector<unsigned char>& salt);
    static std::string convert_bytes(const std::vector<unsigned char>& bytes);
    static std::vector<unsigned char> convert_hex(std::string_view hex);
};