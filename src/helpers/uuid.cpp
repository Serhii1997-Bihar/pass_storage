#include "pass-storage/helpers/uuid.hpp"
#include <random>
#include <format>
#include <cstdint>

std::string generate_uuid_v4() {
    static std::random_device rd;
    static std::mt19937 generator(rd());
    std::uniform_int_distribution<uint32_t> dis;

    uint32_t data[4];
    for (auto& val : data) {
        val = dis(generator);
    }

    data[1] = (data[1] & 0xFFFF0FFF) | 0x00004000;
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;

    return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:012x}",
        data[0],
        data[1] >> 16,
        data[1] & 0xFFFF,
        data[2] >> 16,
        ((uint64_t)(data[2] & 0xFFFF) << 32) | data[3]
    );
}
