#ifndef nector_cdsl_package_format_h
#define nector_cdsl_package_format_h

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace nector_cdsl_detail {

constexpr uint16_t package_version = 1;
constexpr size_t max_package_size = 64u * 1024u * 1024u;
constexpr size_t max_string_size = 16u * 1024u * 1024u;
constexpr size_t max_node_count = 100000;
constexpr size_t max_action_count = 10000;
constexpr size_t max_operation_count = 100000;

constexpr size_t package_version_offset = 6;
constexpr size_t package_flags_offset = 8;
constexpr size_t package_reserved_offset = 12;
constexpr size_t package_data_header_offset = 16;
constexpr size_t package_data_size_offset = 32;
constexpr size_t package_data_offset = 40;
constexpr size_t section_size_offset = 16;
constexpr size_t section_header_size = 24;
constexpr size_t end_total_size_offset = 16;
constexpr size_t end_data_crc_offset = 24;
constexpr size_t end_text_crc_offset = 28;
constexpr size_t end_reserved_offset = 32;
constexpr size_t end_section_size = 36;
constexpr size_t minimum_package_size = package_data_offset + section_header_size + end_section_size;
constexpr std::array<uint8_t, 6> package_magic{'N', 'E', 'C', 'T', 'O', 'R'};
constexpr std::array<uint8_t, 16> data_marker{'D', 'a', 't', 'a', ' ', 'S', 'e', 'c', 't', 'i', 'o', 'n', 0, 0, 0, 0};
constexpr std::array<uint8_t, 16> text_marker{'T', 'e', 'x', 't', ' ', 'S', 'e', 'c', 't', 'i', 'o', 'n', 0, 0, 0, 0};
constexpr std::array<uint8_t, 16> end_marker{'E', 'n', 'd', ' ', 'S', 'e', 'c', 't', 'i', 'o', 'n', 0, 0, 0, 0, 0};

enum class ValueType : uint16_t {
    string = 1,
    number = 2,
    boolean = 3,
    color = 4
};

enum class Operation : uint16_t {
    set_string = 1,
    set_number = 2,
    set_boolean = 3,
    set_color = 4,
    emit = 5
};

inline void append16(std::vector<uint8_t>& output, uint16_t value) {
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
}

inline void append32(std::vector<uint8_t>& output, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) output.push_back(static_cast<uint8_t>(value >> shift));
}

inline void append64(std::vector<uint8_t>& output, uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) output.push_back(static_cast<uint8_t>(value >> shift));
}

inline void appendBytes(std::vector<uint8_t>& output, const uint8_t* data, size_t size) {
    if (size != 0) output.insert(output.end(), data, data + size);
}

inline void appendString(std::vector<uint8_t>& output, const std::string& value) {
    append32(output, static_cast<uint32_t>(value.size()));
    appendBytes(output, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

inline uint16_t read16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0] | (static_cast<uint16_t>(data[1]) << 8));
}

inline uint32_t read32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

inline uint64_t read64(const uint8_t* data) {
    uint64_t value = 0;
    for (int index = 7; index >= 0; --index) value = (value << 8) | data[index];
    return value;
}

template <size_t size>
bool markerEquals(const uint8_t* data, const std::array<uint8_t, size>& marker) {
    return std::memcmp(data, marker.data(), marker.size()) == 0;
}

}
#endif
