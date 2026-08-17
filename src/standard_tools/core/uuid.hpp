#pragma once

#include <array>
#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

namespace standard_tools::core {

/// Generates a random RFC-4122 version-4 UUID string.
inline std::string GenerateUuid() {
    static thread_local std::mt19937 generator{std::random_device{}()};
    std::uniform_int_distribution<std::uint32_t> dist(0, 255);

    std::array<std::uint8_t, 16> bytes{};
    for (auto& b : bytes) {
        b = static_cast<std::uint8_t>(dist(generator));
    }

    // Version 4: bits 12-15 of time_hi_and_version = 0b0100
    bytes[6] = static_cast<std::uint8_t>((bytes[6] & 0x0f) | 0x40);
    // Variant: bits 6-7 of clock_seq_hi_and_reserved = 0b10
    bytes[8] = static_cast<std::uint8_t>((bytes[8] & 0x3f) | 0x80);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

}  // namespace standard_tools::core
