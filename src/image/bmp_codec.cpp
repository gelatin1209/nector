#include "image_internal.h"

#include <algorithm>
#include <array>
#include <limits>

namespace nector_image_internal {
namespace {

uint16_t read16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1]) << 8;
}

uint32_t read32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) | static_cast<uint32_t>(data[1]) << 8 |
           static_cast<uint32_t>(data[2]) << 16 | static_cast<uint32_t>(data[3]) << 24;
}

struct ChannelMask {
    uint32_t mask = 0;
    unsigned int shift = 0;
    uint32_t maximum = 0;
};

bool makeChannel(uint32_t mask, ChannelMask* out_channel) {
    if (out_channel == nullptr || mask == 0) return false;
    unsigned int shift = 0;
    while ((mask & 1u) == 0) {
        mask >>= 1;
        ++shift;
    }
    if ((mask & (mask + 1u)) != 0) return false;
    out_channel->mask = mask;
    out_channel->shift = shift;
    out_channel->maximum = mask;
    return true;
}

uint8_t extractChannel(uint32_t pixel, const ChannelMask& channel) {
    if (channel.mask == 0) return 255;
    const uint32_t value = (pixel >> channel.shift) & channel.mask;
    return static_cast<uint8_t>((static_cast<uint64_t>(value) * 255u + channel.maximum / 2u) / channel.maximum);
}

unsigned int packedIndex(const uint8_t* row, int32_t x, uint16_t bits) {
    if (bits == 8) return row[x];
    const uint8_t value = row[static_cast<size_t>(x) * bits / 8];
    if (bits == 4) return (x & 1) == 0 ? value >> 4 : value & 0x0f;
    const unsigned int shift = 7u - static_cast<unsigned int>(x & 7);
    return (value >> shift) & 0x01u;
}

}

nector_status decodeBmp(const uint8_t* data, size_t size, DecodedImage* out_image) {
    if (data == nullptr || out_image == nullptr) return nectorFail(nector_invalid_argument, "BMP input is null");
    if (size < 26) return nectorFail(nector_invalid_format, "BMP header is truncated");
    const uint32_t pixel_offset = read32(data + 10);
    const uint32_t dib_size = read32(data + 14);
    if (dib_size != 12 && dib_size < 40) return nectorFail(nector_unsupported, "BMP DIB header is unsupported");
    if (dib_size > size - 14) return nectorFail(nector_invalid_format, "BMP DIB header is truncated");
    const bool core = dib_size == 12;
    const int64_t width64 = core ? read16(data + 18) : static_cast<int32_t>(read32(data + 18));
    const int64_t signed_height = core ? read16(data + 20) : static_cast<int32_t>(read32(data + 22));
    if (width64 <= 0 || width64 > std::numeric_limits<int32_t>::max() || signed_height == 0 ||
        signed_height == std::numeric_limits<int32_t>::min()) {
        return nectorFail(nector_invalid_format, "BMP dimensions are invalid");
    }
    const bool top_down = signed_height < 0;
    const int64_t height64 = top_down ? -signed_height : signed_height;
    const int32_t width = static_cast<int32_t>(width64);
    const int32_t height = static_cast<int32_t>(height64);
    size_t output_size = 0;
    if (!pixelBufferSize(width, height, &output_size)) return nectorFail(nector_limit_exceeded, "BMP image is too large");
    const uint16_t planes = read16(data + 26);
    const uint16_t bits = read16(data + 28);
    const uint32_t compression = core ? 0 : read32(data + 30);
    if (planes != 1 || (compression != 0 && compression != 3)) {
        return nectorFail(nector_unsupported, "BMP compression or plane count is unsupported");
    }
    if (bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32) {
        return nectorFail(nector_unsupported, "BMP bit depth is unsupported");
    }
    if (compression == 3 && bits != 16 && bits != 32) {
        return nectorFail(nector_invalid_format, "BMP bitfields require 16-bit or 32-bit pixels");
    }
    if (bits <= 8 && compression != 0) return nectorFail(nector_invalid_format, "indexed BMP uses invalid compression");

    const uint64_t row_bits = static_cast<uint64_t>(static_cast<uint32_t>(width)) * bits;
    if (row_bits > (std::numeric_limits<uint64_t>::max() - 31u)) return nectorFail(nector_limit_exceeded, "BMP row is too large");
    const uint64_t stride64 = ((row_bits + 31u) / 32u) * 4u;
    const uint64_t data_bytes = stride64 * static_cast<uint64_t>(height);
    if (stride64 > std::numeric_limits<size_t>::max() || data_bytes > size ||
        pixel_offset > size || data_bytes > size - pixel_offset) {
        return nectorFail(nector_invalid_format, "BMP pixel array is truncated or overflows");
    }
    const size_t stride = static_cast<size_t>(stride64);
    const size_t palette_entry_size = core ? 3 : 4;
    const size_t palette_offset = 14u + dib_size + (compression == 3 ? (dib_size >= 52 ? 0u : 12u) : 0u);
    if (palette_offset > size || palette_offset > pixel_offset) {
        return nectorFail(nector_invalid_format, "BMP color table offset is invalid");
    }
    uint32_t color_count = 0;
    if (bits <= 8) {
        color_count = (!core && dib_size >= 36) ? read32(data + 46) : 0;
        if (color_count == 0) color_count = 1u << bits;
        if (color_count > (1u << bits)) return nectorFail(nector_invalid_format, "BMP palette is too large");
        const uint64_t palette_bytes = static_cast<uint64_t>(color_count) * palette_entry_size;
        if (palette_bytes > pixel_offset - palette_offset) {
            return nectorFail(nector_invalid_format, "BMP palette is truncated");
        }
    }

    ChannelMask red;
    ChannelMask green;
    ChannelMask blue;
    ChannelMask alpha;
    if (bits == 16 || bits == 32) {
        uint32_t red_mask = bits == 16 ? 0x7c00u : 0x00ff0000u;
        uint32_t green_mask = bits == 16 ? 0x03e0u : 0x0000ff00u;
        uint32_t blue_mask = bits == 16 ? 0x001fu : 0x000000ffu;
        uint32_t alpha_mask = 0;
        if (compression == 3) {
            size_t mask_offset = 0;
            if (dib_size >= 52) {
                mask_offset = 54;
            } else {
                mask_offset = 14u + dib_size;
                if (mask_offset > size || size - mask_offset < 12) {
                    return nectorFail(nector_invalid_format, "BMP channel masks are truncated");
                }
            }
            red_mask = read32(data + mask_offset);
            green_mask = read32(data + mask_offset + 4);
            blue_mask = read32(data + mask_offset + 8);
            if (dib_size >= 56) alpha_mask = read32(data + 66);
        } else if (dib_size >= 52) {
            red_mask = read32(data + 54);
            green_mask = read32(data + 58);
            blue_mask = read32(data + 62);
            if (dib_size >= 56) alpha_mask = read32(data + 66);
        }
        if (!makeChannel(red_mask, &red) || !makeChannel(green_mask, &green) || !makeChannel(blue_mask, &blue) ||
            (alpha_mask != 0 && !makeChannel(alpha_mask, &alpha))) {
            return nectorFail(nector_invalid_format, "BMP channel masks are invalid");
        }
        const uint32_t all_masks = red_mask | green_mask | blue_mask | alpha_mask;
        const uint32_t allowed = bits == 16 ? 0xffffu : 0xffffffffu;
        if ((red_mask & green_mask) != 0 || (red_mask & blue_mask) != 0 || (green_mask & blue_mask) != 0 ||
            (alpha_mask & (red_mask | green_mask | blue_mask)) != 0 || (all_masks & ~allowed) != 0) {
            return nectorFail(nector_invalid_format, "BMP channel masks overlap or exceed pixel size");
        }
    }

    std::array<std::array<uint8_t, 4>, 256> palette{};
    for (uint32_t index = 0; index < color_count; ++index) {
        const uint8_t* entry = data + palette_offset + static_cast<size_t>(index) * palette_entry_size;
        palette[index] = {entry[2], entry[1], entry[0], 255};
    }
    DecodedImage decoded;
    decoded.width = width;
    decoded.height = height;
    decoded.pixels.resize(output_size);
    bool any_alpha = false;
    for (int32_t y = 0; y < height; ++y) {
        const int32_t source_y = top_down ? y : height - 1 - y;
        const uint8_t* row = data + pixel_offset + static_cast<size_t>(source_y) * stride;
        uint8_t* destination = decoded.pixels.data() + static_cast<size_t>(y) * width * 4;
        for (int32_t x = 0; x < width; ++x) {
            if (bits <= 8) {
                const unsigned int index = packedIndex(row, x, bits);
                if (index >= color_count) return nectorFail(nector_invalid_format, "BMP palette index is out of range");
                const auto& color = palette[index];
                destination[0] = color[0];
                destination[1] = color[1];
                destination[2] = color[2];
                destination[3] = 255;
            } else if (bits == 24) {
                const uint8_t* source = row + static_cast<size_t>(x) * 3;
                destination[0] = source[2];
                destination[1] = source[1];
                destination[2] = source[0];
                destination[3] = 255;
            } else {
                const size_t offset = static_cast<size_t>(x) * (bits / 8);
                const uint32_t pixel = bits == 16 ? read16(row + offset) : read32(row + offset);
                destination[0] = extractChannel(pixel, red);
                destination[1] = extractChannel(pixel, green);
                destination[2] = extractChannel(pixel, blue);
                if (bits == 32 && alpha.mask == 0 && compression == 0) destination[3] = static_cast<uint8_t>(pixel >> 24);
                else destination[3] = alpha.mask == 0 ? 255 : extractChannel(pixel, alpha);
                any_alpha = any_alpha || destination[3] != 0;
            }
            destination += 4;
        }
    }
    if (bits == 32 && alpha.mask == 0 && compression == 0 && !any_alpha) {
        for (size_t offset = 3; offset < decoded.pixels.size(); offset += 4) decoded.pixels[offset] = 255;
    }
    *out_image = std::move(decoded);
    return nector_ok;
}

}
