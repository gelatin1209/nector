#ifndef nector_cdsl_package_reader_h
#define nector_cdsl_package_reader_h

#include "package_format.h"
#include "core.h"

#include <cmath>
#include <cstring>
#include <string>

namespace nector_cdsl_detail {

class Reader {
public:
    Reader(const uint8_t* data, size_t size, size_t absolute_offset = 0)
        : data_(data), size_(size), absolute_offset_(absolute_offset) {}

    size_t remaining() const { return size_ - cursor_; }
    size_t position() const { return cursor_; }
    size_t offset() const { return absolute_offset_ + cursor_; }
    bool empty() const { return cursor_ == size_; }

    bool read8(uint8_t& value) {
        if (remaining() < 1) return false;
        value = data_[cursor_++];
        return true;
    }
    bool read16(uint16_t& value) {
        if (remaining() < 2) return false;
        value = nector_cdsl_detail::read16(data_ + cursor_);
        cursor_ += 2;
        return true;
    }
    bool read32(uint32_t& value) {
        if (remaining() < 4) return false;
        value = nector_cdsl_detail::read32(data_ + cursor_);
        cursor_ += 4;
        return true;
    }
    bool read64(uint64_t& value) {
        if (remaining() < 8) return false;
        value = nector_cdsl_detail::read64(data_ + cursor_);
        cursor_ += 8;
        return true;
    }
    bool readDouble(double& value) {
        uint64_t bits = 0;
        if (!read64(bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value);
    }
    bool readFloat(float& value) {
        uint32_t bits = 0;
        if (!read32(bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value);
    }
    bool readString(std::string& value) {
        uint32_t size = 0;
        if (!read32(size) || size > remaining() || size > max_string_size) return false;
        if (size == 0) {
            value.clear();
        } else {
            value.assign(reinterpret_cast<const char*>(data_ + cursor_), size);
        }
        cursor_ += size;
        return nectorCoreValidateUtf8(value.data(), value.size()) == nector_ok;
    }
    bool readBytes(size_t size, const uint8_t*& data) {
        if (size > remaining()) return false;
        data = data_ == nullptr ? nullptr : data_ + cursor_;
        cursor_ += size;
        return true;
    }
    Reader subreader(size_t size, bool& valid) {
        valid = size <= remaining();
        if (!valid) return Reader(nullptr, 0, offset());
        Reader result(data_ + cursor_, size, offset());
        cursor_ += size;
        return result;
    }

private:
    const uint8_t* data_;
    size_t size_;
    size_t absolute_offset_;
    size_t cursor_ = 0;
};

struct PackageLayout {
    size_t package_size = 0;
    uint16_t version = 0;
    uint32_t flags = 0;
    uint32_t reserved = 0;
    size_t data_header_offset = package_data_header_offset;
    size_t data_offset = package_data_offset;
    size_t data_size = 0;
    size_t text_header_offset = 0;
    size_t text_offset = 0;
    size_t text_size = 0;
    size_t end_offset = 0;
    uint64_t declared_package_size = 0;
    uint32_t data_crc32 = 0;
    uint32_t text_crc32 = 0;
    uint32_t actual_data_crc32 = 0;
    uint32_t actual_text_crc32 = 0;
    bool magic_valid = false;
    bool data_crc_valid = false;
    bool text_crc_valid = false;
    bool envelope_valid = false;
};

inline bool readPackageLayout(const uint8_t* data, size_t size, PackageLayout& layout, std::string& error) {
    layout = PackageLayout{};
    layout.package_size = size;
    error.clear();
    if (data == nullptr || size < minimum_package_size || size > max_package_size) {
        error = "package is too small, too large, or has bad magic";
        return false;
    }
    layout.magic_valid = markerEquals(data, package_magic);
    if (!layout.magic_valid) {
        error = "package is too small, too large, or has bad magic";
        return false;
    }
    layout.version = nector_cdsl_detail::read16(data + package_version_offset);
    layout.flags = nector_cdsl_detail::read32(data + package_flags_offset);
    layout.reserved = nector_cdsl_detail::read32(data + package_reserved_offset);
    if (layout.version != package_version || layout.flags != 0 || layout.reserved != 0 ||
        !markerEquals(data + package_data_header_offset, data_marker)) {
        error = "unsupported package version or invalid header";
        return false;
    }
    const uint64_t data_size = nector_cdsl_detail::read64(data + package_data_size_offset);
    if (data_size > size - package_data_offset || data_size > max_package_size) {
        error = "Data Section size is invalid";
        return false;
    }
    layout.data_size = static_cast<size_t>(data_size);
    layout.text_header_offset = package_data_offset + layout.data_size;
    if (size - layout.text_header_offset < section_header_size + end_section_size ||
        !markerEquals(data + layout.text_header_offset, text_marker)) {
        error = "Text Section marker is missing";
        return false;
    }
    layout.text_offset = layout.text_header_offset + section_header_size;
    const uint64_t text_size = nector_cdsl_detail::read64(data + layout.text_header_offset + section_size_offset);
    if (text_size > max_package_size || text_size > size - layout.text_offset) {
        error = "Text Section size is invalid";
        return false;
    }
    layout.text_size = static_cast<size_t>(text_size);
    layout.end_offset = layout.text_offset + layout.text_size;
    if (size - layout.end_offset != end_section_size || !markerEquals(data + layout.end_offset, end_marker)) {
        error = "End Section or total package length is invalid";
        return false;
    }
    layout.declared_package_size = nector_cdsl_detail::read64(data + layout.end_offset + end_total_size_offset);
    layout.data_crc32 = nector_cdsl_detail::read32(data + layout.end_offset + end_data_crc_offset);
    layout.text_crc32 = nector_cdsl_detail::read32(data + layout.end_offset + end_text_crc_offset);
    if (layout.declared_package_size != size ||
        nector_cdsl_detail::read32(data + layout.end_offset + end_reserved_offset) != 0) {
        error = "End Section or total package length is invalid";
        return false;
    }
    layout.actual_data_crc32 = nectorCoreCrc32(data + layout.data_offset, layout.data_size);
    layout.actual_text_crc32 = nectorCoreCrc32(data + layout.text_offset, layout.text_size);
    layout.data_crc_valid = layout.actual_data_crc32 == layout.data_crc32;
    layout.text_crc_valid = layout.actual_text_crc32 == layout.text_crc32;
    if (!layout.data_crc_valid || !layout.text_crc_valid) {
        error = "section CRC32 mismatch";
        return false;
    }
    layout.envelope_valid = true;
    return true;
}

}
#endif
