#pragma once
#include <cstdint>

inline std::uint8_t big_endian_to_host(std::uint8_t value) { return value; }

inline std::uint16_t swap_bytes(std::uint16_t v) {
    return static_cast<std::uint16_t>((v >> 8) | (v << 8));
}
inline std::uint32_t swap_bytes(std::uint32_t v) {
    return ((v & 0x000000ffu) << 24) | ((v & 0x0000ff00u) << 8) |
           ((v & 0x00ff0000u) >> 8) | ((v & 0xff000000u) >> 24);
}
inline std::uint64_t swap_bytes(std::uint64_t v) {
    return (static_cast<std::uint64_t>(swap_bytes(static_cast<std::uint32_t>(v))) << 32) |
           swap_bytes(static_cast<std::uint32_t>(v >> 32));
}
inline bool native_little_endian() {
    const std::uint16_t x = 1;
    return *reinterpret_cast<const std::uint8_t*>(&x) == 1;
}
inline std::uint16_t big_endian_to_host(std::uint16_t v) { return native_little_endian() ? swap_bytes(v) : v; }
inline std::uint32_t big_endian_to_host(std::uint32_t v) { return native_little_endian() ? swap_bytes(v) : v; }
inline std::uint64_t big_endian_to_host(std::uint64_t v) { return native_little_endian() ? swap_bytes(v) : v; }
inline std::uint8_t host_to_big_endian(std::uint8_t v) { return v; }
inline std::uint16_t host_to_big_endian(std::uint16_t v) { return big_endian_to_host(v); }
inline std::uint32_t host_to_big_endian(std::uint32_t v) { return big_endian_to_host(v); }
inline std::uint64_t host_to_big_endian(std::uint64_t v) { return big_endian_to_host(v); }