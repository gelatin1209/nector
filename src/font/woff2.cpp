#include "woff2.h"

#include <brotli/decode.h>

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace nector_font_detail {
namespace {

constexpr size_t max_decoded_size = 512u * 1024u * 1024u;
constexpr size_t max_table_count = 4095u;
constexpr size_t max_composite_components = 65535u;
constexpr uint32_t tag_glyf = 0x676c7966u;
constexpr uint32_t tag_loca = 0x6c6f6361u;
constexpr uint32_t tag_hmtx = 0x686d7478u;
constexpr uint32_t tag_head = 0x68656164u;
constexpr uint32_t tag_hhea = 0x68686561u;
constexpr uint32_t tag_maxp = 0x6d617870u;
constexpr uint32_t tag_ttcf = 0x74746366u;
constexpr uint32_t no_table_index = std::numeric_limits<uint32_t>::max();

constexpr uint32_t known_tags[63] = {
    0x636d6170u, 0x68656164u, 0x68686561u, 0x686d7478u,
    0x6d617870u, 0x6e616d65u, 0x4f532f32u, 0x706f7374u,
    0x63767420u, 0x6670676du, 0x676c7966u, 0x6c6f6361u,
    0x70726570u, 0x43464620u, 0x564f5247u, 0x45424454u,
    0x45424c43u, 0x67617370u, 0x68646d78u, 0x6b65726eu,
    0x4c545348u, 0x50434c54u, 0x56444d58u, 0x76686561u,
    0x766d7478u, 0x42415345u, 0x47444546u, 0x47504f53u,
    0x47535542u, 0x45425343u, 0x4a535446u, 0x4d415448u,
    0x43424454u, 0x43424c43u, 0x434f4c52u, 0x4350414cu,
    0x53564720u, 0x73626978u, 0x61636e74u, 0x61766172u,
    0x62646174u, 0x626c6f63u, 0x62736c6eu, 0x63766172u,
    0x66647363u, 0x66656174u, 0x666d7478u, 0x66766172u,
    0x67766172u, 0x68737479u, 0x6a757374u, 0x6c636172u,
    0x6d6f7274u, 0x6d6f7278u, 0x6f706264u, 0x70726f70u,
    0x7472616bu, 0x5a617066u, 0x53696c66u, 0x476c6174u,
    0x476c6f63u, 0x46656174u, 0x53696c6cu
};

bool fail(std::string& error, const char* message) {
    error = message;
    return false;
}

uint16_t loadU16(const uint8_t* data) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                                 static_cast<uint16_t>(data[1]));
}

int16_t loadS16(const uint8_t* data) {
    const uint16_t value = loadU16(data);
    const int32_t signed_value = value < 0x8000u ? static_cast<int32_t>(value)
                                                 : static_cast<int32_t>(value) - 0x10000;
    return static_cast<int16_t>(signed_value);
}

uint32_t loadU32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

void storeU16(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value);
}

void storeU32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

bool checkedAdd(size_t left, size_t right, size_t& result) {
    if (right > std::numeric_limits<size_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

class Reader {
public:
    Reader() = default;
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool readU8(uint8_t& value) {
        if (position_ == size_) {
            return false;
        }
        value = data_[position_++];
        return true;
    }

    bool readU16(uint16_t& value) {
        const uint8_t* bytes = nullptr;
        if (!readBytes(2, bytes)) {
            return false;
        }
        value = loadU16(bytes);
        return true;
    }

    bool readS16(int16_t& value) {
        const uint8_t* bytes = nullptr;
        if (!readBytes(2, bytes)) {
            return false;
        }
        value = loadS16(bytes);
        return true;
    }

    bool readU32(uint32_t& value) {
        const uint8_t* bytes = nullptr;
        if (!readBytes(4, bytes)) {
            return false;
        }
        value = loadU32(bytes);
        return true;
    }

    bool readBytes(size_t count, const uint8_t*& bytes) {
        if (count > size_ - position_) {
            return false;
        }
        bytes = data_ + position_;
        position_ += count;
        return true;
    }

    size_t position() const { return position_; }
    size_t remaining() const { return size_ - position_; }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t position_ = 0;
};

bool readUIntBase128(Reader& reader, uint32_t& value) {
    uint32_t accumulated = 0;
    for (size_t index = 0; index < 5; ++index) {
        uint8_t byte = 0;
        if (!reader.readU8(byte) || (index == 0 && byte == 0x80u) ||
            (accumulated & 0xfe000000u) != 0) {
            return false;
        }
        accumulated = (accumulated << 7) | static_cast<uint32_t>(byte & 0x7fu);
        if ((byte & 0x80u) == 0) {
            value = accumulated;
            return true;
        }
    }
    return false;
}

bool read255UInt16(Reader& reader, uint16_t& value) {
    uint8_t code = 0;
    if (!reader.readU8(code)) {
        return false;
    }
    if (code < 253u) {
        value = code;
        return true;
    }
    if (code == 253u) {
        return reader.readU16(value);
    }
    uint8_t extra = 0;
    if (!reader.readU8(extra)) {
        return false;
    }
    value = static_cast<uint16_t>(extra + (code == 255u ? 253u : 506u));
    return true;
}

void appendU16(std::vector<uint8_t>& output, uint16_t value) {
    output.push_back(static_cast<uint8_t>(value >> 8));
    output.push_back(static_cast<uint8_t>(value));
}

void appendS16(std::vector<uint8_t>& output, int16_t value) {
    appendU16(output, static_cast<uint16_t>(value));
}

void appendBytes(std::vector<uint8_t>& output, const uint8_t* bytes, size_t size) {
    if (size != 0) {
        output.insert(output.end(), bytes, bytes + size);
    }
}

struct TableEntry {
    uint32_t tag = 0;
    uint32_t orig_length = 0;
    uint32_t transform_length = 0;
    uint8_t version = 0;
    bool transformed = false;
    size_t source_offset = 0;
    size_t source_length = 0;
    std::vector<uint8_t> data;
};

struct BoundingBox {
    int16_t x_min = 0;
    int16_t y_min = 0;
    int16_t x_max = 0;
    int16_t y_max = 0;
};

bool readBoundingBox(Reader& reader, BoundingBox& box) {
    if (!reader.readS16(box.x_min) || !reader.readS16(box.y_min) ||
        !reader.readS16(box.x_max) || !reader.readS16(box.y_max)) {
        return false;
    }
    return box.x_min <= box.x_max && box.y_min <= box.y_max;
}

bool decodeTriplet(uint8_t flag, Reader& glyph_stream, int32_t& delta_x, int32_t& delta_y) {
    const uint8_t code = flag & 0x7fu;
    uint8_t byte0 = 0;
    uint8_t byte1 = 0;
    uint8_t byte2 = 0;
    uint8_t byte3 = 0;
    uint32_t sign_code = 0;

    if (code < 10u) {
        if (!glyph_stream.readU8(byte0)) {
            return false;
        }
        delta_x = 0;
        delta_y = static_cast<int32_t>(byte0) + 256 * (code / 2);
        if ((code & 1u) == 0) {
            delta_y = -delta_y;
        }
        return true;
    }
    if (code < 20u) {
        if (!glyph_stream.readU8(byte0)) {
            return false;
        }
        const uint8_t local_code = static_cast<uint8_t>(code - 10u);
        delta_x = static_cast<int32_t>(byte0) + 256 * (local_code / 2);
        delta_y = 0;
        if ((local_code & 1u) == 0) {
            delta_x = -delta_x;
        }
        return true;
    }
    if (code < 84u) {
        if (!glyph_stream.readU8(byte0)) {
            return false;
        }
        const uint32_t local_code = static_cast<uint32_t>(code - 20u);
        const uint32_t group = local_code / 16u;
        const uint32_t within_group = local_code % 16u;
        delta_x = static_cast<int32_t>(1u + 16u * group + (byte0 >> 4));
        delta_y = static_cast<int32_t>(1u + 16u * (within_group / 4u) + (byte0 & 0x0fu));
        sign_code = within_group % 4u;
    } else if (code < 120u) {
        if (!glyph_stream.readU8(byte0) || !glyph_stream.readU8(byte1)) {
            return false;
        }
        const uint32_t local_code = static_cast<uint32_t>(code - 84u);
        const uint32_t group = local_code / 12u;
        const uint32_t within_group = local_code % 12u;
        delta_x = static_cast<int32_t>(1u + 256u * group + byte0);
        delta_y = static_cast<int32_t>(1u + 256u * (within_group / 4u) + byte1);
        sign_code = within_group % 4u;
    } else if (code < 124u) {
        if (!glyph_stream.readU8(byte0) || !glyph_stream.readU8(byte1) ||
            !glyph_stream.readU8(byte2)) {
            return false;
        }
        delta_x = static_cast<int32_t>((static_cast<uint32_t>(byte0) << 4) | (byte1 >> 4));
        delta_y = static_cast<int32_t>(((static_cast<uint32_t>(byte1) & 0x0fu) << 8) | byte2);
        sign_code = static_cast<uint32_t>(code - 120u);
    } else {
        if (!glyph_stream.readU8(byte0) || !glyph_stream.readU8(byte1) ||
            !glyph_stream.readU8(byte2) || !glyph_stream.readU8(byte3)) {
            return false;
        }
        delta_x = static_cast<int32_t>((static_cast<uint32_t>(byte0) << 8) | byte1);
        delta_y = static_cast<int32_t>((static_cast<uint32_t>(byte2) << 8) | byte3);
        sign_code = static_cast<uint32_t>(code - 124u);
    }

    if ((sign_code & 1u) == 0) {
        delta_x = -delta_x;
    }
    if ((sign_code & 2u) == 0) {
        delta_y = -delta_y;
    }
    return true;
}

bool appendGlyphRecord(std::vector<uint8_t>& glyf, const std::vector<uint8_t>& record,
                       std::string& error) {
    const size_t padding = (4u - (record.size() & 3u)) & 3u;
    size_t required = 0;
    if (!checkedAdd(record.size(), padding, required) || glyf.size() > max_decoded_size ||
        required > max_decoded_size - glyf.size()) {
        return fail(error, "reconstructed glyf table is too large");
    }
    glyf.insert(glyf.end(), record.begin(), record.end());
    glyf.insert(glyf.end(), padding, 0);
    return true;
}

bool decodeTransformedGlyf(const uint8_t* data, size_t size, uint32_t loca_orig_length,
                           std::vector<uint8_t>& glyf, std::vector<uint8_t>& loca,
                           std::vector<int16_t>& x_mins, uint16_t& output_num_glyphs,
                           uint16_t& output_index_format, std::string& error) {
    Reader reader(data, size);
    uint16_t reserved = 0;
    uint16_t option_flags = 0;
    uint16_t num_glyphs = 0;
    uint16_t index_format = 0;
    uint32_t stream_sizes[7] = {};
    if (!reader.readU16(reserved) || !reader.readU16(option_flags) ||
        !reader.readU16(num_glyphs) || !reader.readU16(index_format)) {
        return fail(error, "truncated transformed glyf header");
    }
    for (uint32_t& stream_size : stream_sizes) {
        if (!reader.readU32(stream_size)) {
            return fail(error, "truncated transformed glyf header");
        }
    }
    if (reserved != 0 || (option_flags & 0xfffeu) != 0 || index_format > 1u) {
        return fail(error, "invalid transformed glyf header flags");
    }
    if (stream_sizes[0] != static_cast<uint32_t>(num_glyphs) * 2u) {
        return fail(error, "invalid nContour stream size");
    }

    Reader streams[7];
    for (size_t index = 0; index < 7; ++index) {
        const uint8_t* stream_data = nullptr;
        if (!reader.readBytes(stream_sizes[index], stream_data)) {
            return fail(error, "transformed glyf substreams exceed table size");
        }
        streams[index] = Reader(stream_data, stream_sizes[index]);
    }

    const size_t bitmap_size = ((static_cast<size_t>(num_glyphs) + 31u) / 32u) * 4u;
    if (bitmap_size > streams[5].remaining()) {
        return fail(error, "truncated glyf bounding box bitmap");
    }
    const uint8_t* bbox_bitmap = nullptr;
    if (!streams[5].readBytes(bitmap_size, bbox_bitmap)) {
        return fail(error, "truncated glyf bounding box bitmap");
    }
    Reader& n_contour_stream = streams[0];
    Reader& n_points_stream = streams[1];
    Reader& flag_stream = streams[2];
    Reader& glyph_stream = streams[3];
    Reader& composite_stream = streams[4];
    Reader& bbox_stream = streams[5];
    Reader& instruction_stream = streams[6];

    const uint8_t* overlap_bitmap = nullptr;
    if ((option_flags & 1u) != 0 && !reader.readBytes(bitmap_size, overlap_bitmap)) {
        return fail(error, "truncated overlapSimpleBitmap");
    }
    if (reader.remaining() != 0) {
        return fail(error, "extraneous data in transformed glyf table");
    }

    const uint64_t expected_loca_size =
        (static_cast<uint64_t>(num_glyphs) + 1u) * (index_format == 0 ? 2u : 4u);
    if (loca_orig_length != expected_loca_size) {
        return fail(error, "invalid transformed loca original length");
    }

    std::vector<uint32_t> offsets;
    offsets.reserve(static_cast<size_t>(num_glyphs) + 1u);
    x_mins.clear();
    x_mins.reserve(num_glyphs);
    glyf.clear();

    for (uint32_t glyph_index = 0; glyph_index < num_glyphs; ++glyph_index) {
        offsets.push_back(static_cast<uint32_t>(glyf.size()));
        int16_t number_of_contours = 0;
        if (!n_contour_stream.readS16(number_of_contours) || number_of_contours < -1) {
            return fail(error, "invalid glyph contour count");
        }
        const bool has_explicit_bbox =
            (bbox_bitmap[glyph_index / 8u] & (0x80u >> (glyph_index % 8u))) != 0;

        if (number_of_contours == 0) {
            if (has_explicit_bbox) {
                return fail(error, "empty glyph has an explicit bounding box");
            }
            x_mins.push_back(0);
            continue;
        }

        std::vector<uint8_t> record;
        BoundingBox box;
        if (number_of_contours > 0) {
            const size_t contour_count = static_cast<size_t>(number_of_contours);
            std::vector<uint16_t> end_points;
            end_points.reserve(contour_count);
            uint32_t total_points = 0;
            for (size_t contour = 0; contour < contour_count; ++contour) {
                uint16_t point_count = 0;
                if (!read255UInt16(n_points_stream, point_count) || point_count == 0 ||
                    total_points > 65536u - point_count) {
                    return fail(error, "invalid simple glyph point count");
                }
                total_points += point_count;
                end_points.push_back(static_cast<uint16_t>(total_points - 1u));
            }

            std::vector<uint8_t> point_flags;
            std::vector<int16_t> x_deltas;
            std::vector<int16_t> y_deltas;
            point_flags.reserve(total_points);
            x_deltas.reserve(total_points);
            y_deltas.reserve(total_points);
            int32_t current_x = 0;
            int32_t current_y = 0;
            int32_t calculated_x_min = 0;
            int32_t calculated_y_min = 0;
            int32_t calculated_x_max = 0;
            int32_t calculated_y_max = 0;

            for (uint32_t point = 0; point < total_points; ++point) {
                uint8_t flag = 0;
                int32_t delta_x = 0;
                int32_t delta_y = 0;
                if (!flag_stream.readU8(flag) || !decodeTriplet(flag, glyph_stream, delta_x, delta_y) ||
                    delta_x < -32768 || delta_x > 32767 || delta_y < -32768 || delta_y > 32767) {
                    return fail(error, "invalid simple glyph coordinate triplet");
                }
                current_x += delta_x;
                current_y += delta_y;
                if (current_x < -32768 || current_x > 32767 ||
                    current_y < -32768 || current_y > 32767) {
                    return fail(error, "simple glyph coordinate is out of range");
                }
                if (point == 0) {
                    calculated_x_min = calculated_x_max = current_x;
                    calculated_y_min = calculated_y_max = current_y;
                } else {
                    calculated_x_min = std::min(calculated_x_min, current_x);
                    calculated_y_min = std::min(calculated_y_min, current_y);
                    calculated_x_max = std::max(calculated_x_max, current_x);
                    calculated_y_max = std::max(calculated_y_max, current_y);
                }
                point_flags.push_back(flag);
                x_deltas.push_back(static_cast<int16_t>(delta_x));
                y_deltas.push_back(static_cast<int16_t>(delta_y));
            }

            uint16_t instruction_length = 0;
            if (!read255UInt16(glyph_stream, instruction_length)) {
                return fail(error, "missing simple glyph instruction length");
            }
            const uint8_t* instructions = nullptr;
            if (!instruction_stream.readBytes(instruction_length, instructions)) {
                return fail(error, "truncated simple glyph instructions");
            }
            if (has_explicit_bbox) {
                if (!readBoundingBox(bbox_stream, box)) {
                    return fail(error, "invalid explicit simple glyph bounding box");
                }
            } else {
                box.x_min = static_cast<int16_t>(calculated_x_min);
                box.y_min = static_cast<int16_t>(calculated_y_min);
                box.x_max = static_cast<int16_t>(calculated_x_max);
                box.y_max = static_cast<int16_t>(calculated_y_max);
            }

            const size_t record_size = 12u + contour_count * 2u + instruction_length +
                                       static_cast<size_t>(total_points) * 5u;
            record.reserve(record_size);
            appendS16(record, number_of_contours);
            appendS16(record, box.x_min);
            appendS16(record, box.y_min);
            appendS16(record, box.x_max);
            appendS16(record, box.y_max);
            for (uint16_t end_point : end_points) {
                appendU16(record, end_point);
            }
            appendU16(record, instruction_length);
            appendBytes(record, instructions, instruction_length);
            for (size_t point = 0; point < point_flags.size(); ++point) {
                uint8_t output_flag = (point_flags[point] & 0x80u) == 0 ? 0x01u : 0x00u;
                if (point == 0 && overlap_bitmap != nullptr &&
                    (overlap_bitmap[glyph_index / 8u] & (0x80u >> (glyph_index % 8u))) != 0) {
                    output_flag |= 0x40u;
                }
                record.push_back(output_flag);
            }
            for (int16_t delta : x_deltas) {
                appendS16(record, delta);
            }
            for (int16_t delta : y_deltas) {
                appendS16(record, delta);
            }
        } else {
            if (!has_explicit_bbox || !readBoundingBox(bbox_stream, box)) {
                return fail(error, "composite glyph requires a valid explicit bounding box");
            }
            appendS16(record, -1);
            appendS16(record, box.x_min);
            appendS16(record, box.y_min);
            appendS16(record, box.x_max);
            appendS16(record, box.y_max);

            bool more_components = true;
            bool have_instructions = false;
            size_t component_count = 0;
            while (more_components) {
                if (++component_count > max_composite_components) {
                    return fail(error, "too many composite glyph components");
                }
                uint16_t component_flags = 0;
                uint16_t component_glyph = 0;
                if (!composite_stream.readU16(component_flags) ||
                    !composite_stream.readU16(component_glyph) || component_glyph >= num_glyphs) {
                    return fail(error, "invalid composite glyph component");
                }
                const unsigned scale_flags = component_flags & 0x00c8u;
                if (scale_flags != 0 && scale_flags != 0x0008u &&
                    scale_flags != 0x0040u && scale_flags != 0x0080u) {
                    return fail(error, "invalid composite glyph scale flags");
                }
                size_t argument_bytes = (component_flags & 0x0001u) != 0 ? 4u : 2u;
                if ((component_flags & 0x0008u) != 0) {
                    argument_bytes += 2u;
                } else if ((component_flags & 0x0040u) != 0) {
                    argument_bytes += 4u;
                } else if ((component_flags & 0x0080u) != 0) {
                    argument_bytes += 8u;
                }
                const uint8_t* arguments = nullptr;
                if (!composite_stream.readBytes(argument_bytes, arguments)) {
                    return fail(error, "truncated composite glyph component");
                }
                appendU16(record, component_flags);
                appendU16(record, component_glyph);
                appendBytes(record, arguments, argument_bytes);
                more_components = (component_flags & 0x0020u) != 0;
                have_instructions = have_instructions || (component_flags & 0x0100u) != 0;
            }
            if (have_instructions) {
                uint16_t instruction_length = 0;
                if (!read255UInt16(glyph_stream, instruction_length)) {
                    return fail(error, "missing composite glyph instruction length");
                }
                const uint8_t* instructions = nullptr;
                if (!instruction_stream.readBytes(instruction_length, instructions)) {
                    return fail(error, "truncated composite glyph instructions");
                }
                appendU16(record, instruction_length);
                appendBytes(record, instructions, instruction_length);
            }
        }

        x_mins.push_back(box.x_min);
        if (!appendGlyphRecord(glyf, record, error)) {
            return false;
        }
    }
    offsets.push_back(static_cast<uint32_t>(glyf.size()));

    for (const Reader& stream : streams) {
        if (stream.remaining() != 0) {
            return fail(error, "extraneous data in a transformed glyf substream");
        }
    }
    if (index_format == 0 && offsets.back() / 2u > 65535u) {
        return fail(error, "reconstructed glyf table exceeds short loca range");
    }

    loca.clear();
    loca.reserve(static_cast<size_t>(loca_orig_length));
    for (uint32_t offset : offsets) {
        if (index_format == 0) {
            appendU16(loca, static_cast<uint16_t>(offset / 2u));
        } else {
            loca.push_back(static_cast<uint8_t>(offset >> 24));
            loca.push_back(static_cast<uint8_t>(offset >> 16));
            loca.push_back(static_cast<uint8_t>(offset >> 8));
            loca.push_back(static_cast<uint8_t>(offset));
        }
    }
    output_num_glyphs = num_glyphs;
    output_index_format = index_format;
    return true;
}

bool analyzeNullGlyf(const TableEntry& glyf, const TableEntry& loca, const TableEntry& maxp,
                     const TableEntry& head, std::vector<int16_t>& x_mins,
                     uint16_t& num_glyphs, std::string& error) {
    if (maxp.data.size() < 6 || head.data.size() < 54) {
        return fail(error, "maxp or head table is too short for glyf reconstruction");
    }
    num_glyphs = loadU16(maxp.data.data() + 4);
    const int16_t index_format = loadS16(head.data.data() + 50);
    if (index_format < 0 || index_format > 1) {
        return fail(error, "invalid head indexToLocFormat");
    }
    const uint64_t expected_loca_size =
        (static_cast<uint64_t>(num_glyphs) + 1u) * (index_format == 0 ? 2u : 4u);
    if (loca.data.size() != expected_loca_size) {
        return fail(error, "invalid untransformed loca table length");
    }

    x_mins.clear();
    x_mins.reserve(num_glyphs);
    uint32_t previous_offset = 0;
    for (uint32_t glyph_index = 0; glyph_index <= num_glyphs; ++glyph_index) {
        const size_t loca_position = static_cast<size_t>(glyph_index) * (index_format == 0 ? 2u : 4u);
        uint32_t offset = 0;
        if (index_format == 0) {
            offset = static_cast<uint32_t>(loadU16(loca.data.data() + loca_position)) * 2u;
        } else {
            offset = loadU32(loca.data.data() + loca_position);
        }
        if ((glyph_index == 0 && offset != 0) || offset < previous_offset ||
            offset > glyf.data.size()) {
            return fail(error, "invalid untransformed loca offset");
        }
        if (glyph_index != 0) {
            if (offset == previous_offset) {
                x_mins.push_back(0);
            } else {
                if (offset - previous_offset < 10u) {
                    return fail(error, "untransformed glyph record is too short");
                }
                x_mins.push_back(loadS16(glyf.data.data() + previous_offset + 2));
            }
        }
        previous_offset = offset;
    }
    return true;
}

bool reconstructHmtx(const TableEntry& entry, const uint8_t* transformed_data,
                     const std::vector<int16_t>& x_mins, const TableEntry& hhea,
                     std::vector<uint8_t>& output, std::string& error) {
    if (hhea.data.size() < 36 || x_mins.empty()) {
        return fail(error, "hmtx transform requires hhea and glyf data");
    }
    const uint16_t num_glyphs = static_cast<uint16_t>(x_mins.size());
    const uint16_t num_h_metrics = loadU16(hhea.data.data() + 34);
    if (num_h_metrics == 0 || num_h_metrics > num_glyphs) {
        return fail(error, "invalid hhea numberOfHMetrics");
    }
    const uint32_t expected_original_size =
        2u * (static_cast<uint32_t>(num_glyphs) + num_h_metrics);
    if (entry.orig_length != expected_original_size) {
        return fail(error, "invalid transformed hmtx original length");
    }

    Reader reader(transformed_data, entry.transform_length);
    uint8_t flags = 0;
    if (!reader.readU8(flags) || (flags & 0xfcu) != 0 || (flags & 0x03u) == 0) {
        return fail(error, "invalid transformed hmtx flags");
    }
    std::vector<uint16_t> advance_widths;
    std::vector<int16_t> proportional_lsbs;
    advance_widths.reserve(num_h_metrics);
    proportional_lsbs.reserve(num_h_metrics);
    for (uint32_t index = 0; index < num_h_metrics; ++index) {
        uint16_t advance = 0;
        if (!reader.readU16(advance)) {
            return fail(error, "truncated hmtx advance widths");
        }
        advance_widths.push_back(advance);
    }
    if ((flags & 1u) == 0) {
        for (uint32_t index = 0; index < num_h_metrics; ++index) {
            int16_t lsb = 0;
            if (!reader.readS16(lsb)) {
                return fail(error, "truncated hmtx proportional bearings");
            }
            proportional_lsbs.push_back(lsb);
        }
    }

    output.clear();
    output.reserve(entry.orig_length);
    for (uint32_t index = 0; index < num_h_metrics; ++index) {
        appendU16(output, advance_widths[index]);
        appendS16(output, (flags & 1u) != 0 ? x_mins[index] : proportional_lsbs[index]);
    }
    for (uint32_t index = num_h_metrics; index < num_glyphs; ++index) {
        int16_t lsb = x_mins[index];
        if ((flags & 2u) == 0 && !reader.readS16(lsb)) {
            return fail(error, "truncated hmtx monospaced bearings");
        }
        appendS16(output, lsb);
    }
    if (reader.remaining() != 0) {
        return fail(error, "extraneous data in transformed hmtx table");
    }
    return true;
}

uint32_t calculateChecksum(const uint8_t* data, size_t size) {
    uint32_t checksum = 0;
    size_t position = 0;
    while (size - position >= 4) {
        checksum += loadU32(data + position);
        position += 4;
    }
    if (position != size) {
        uint8_t final_word[4] = {};
        for (size_t index = position; index < size; ++index) {
            final_word[index - position] = data[index];
        }
        checksum += loadU32(final_word);
    }
    return checksum;
}

bool zeroPadding(const uint8_t* data, size_t begin, size_t end) {
    for (size_t position = begin; position < end; ++position) {
        if (data[position] != 0) {
            return false;
        }
    }
    return true;
}

bool decodeWoff2Impl(const uint8_t* data, size_t size, std::vector<uint8_t>& out_sfnt,
                     std::string& error) {
    if (size < 48 || data == nullptr) {
        return fail(error, "truncated WOFF2 header");
    }
    if (loadU32(data) != 0x774f4632u) {
        return fail(error, "invalid WOFF2 signature");
    }
    if (size > std::numeric_limits<uint32_t>::max() || loadU32(data + 8) != size) {
        return fail(error, "WOFF2 length does not match input size");
    }
    const uint32_t flavor = loadU32(data + 4);
    if (flavor == tag_ttcf) {
        return fail(error, "unsupported WOFF2 font collection");
    }
    const uint16_t num_tables = loadU16(data + 12);
    if (num_tables == 0 || num_tables > max_table_count) {
        return fail(error, "invalid WOFF2 table count");
    }
    const uint32_t compressed_size = loadU32(data + 20);
    const uint32_t meta_offset = loadU32(data + 28);
    const uint32_t meta_length = loadU32(data + 32);
    const uint32_t meta_orig_length = loadU32(data + 36);
    const uint32_t priv_offset = loadU32(data + 40);
    const uint32_t priv_length = loadU32(data + 44);

    Reader directory(data + 48, size - 48);
    std::vector<TableEntry> tables;
    tables.reserve(num_tables);
    size_t transformed_data_size = 0;
    for (uint32_t table_index = 0; table_index < num_tables; ++table_index) {
        uint8_t flags = 0;
        if (!directory.readU8(flags)) {
            return fail(error, "truncated WOFF2 table directory");
        }
        TableEntry entry;
        const uint8_t known_index = flags & 0x3fu;
        if (known_index == 63u) {
            if (!directory.readU32(entry.tag)) {
                return fail(error, "truncated custom table tag");
            }
        } else {
            entry.tag = known_tags[known_index];
        }
        entry.version = static_cast<uint8_t>(flags >> 6);
        if (entry.tag == tag_glyf || entry.tag == tag_loca) {
            if (entry.version != 0 && entry.version != 3) {
                return fail(error, "unknown glyf or loca transform version");
            }
            entry.transformed = entry.version == 0;
        } else if (entry.tag == tag_hmtx) {
            if (entry.version > 1) {
                return fail(error, "unknown hmtx transform version");
            }
            entry.transformed = entry.version == 1;
        } else if (entry.version != 0) {
            return fail(error, "unknown table transform version");
        }
        if (!readUIntBase128(directory, entry.orig_length)) {
            return fail(error, "invalid table original length");
        }
        entry.source_length = entry.orig_length;
        if (entry.transformed) {
            if (!readUIntBase128(directory, entry.transform_length)) {
                return fail(error, "invalid table transform length");
            }
            entry.source_length = entry.transform_length;
        }
        if (entry.transformed && entry.tag == tag_loca && entry.transform_length != 0) {
            return fail(error, "transformed loca must have zero transform length");
        }
        for (const TableEntry& previous : tables) {
            if (previous.tag == entry.tag) {
                return fail(error, "duplicate table tag in WOFF2 directory");
            }
        }
        entry.source_offset = transformed_data_size;
        if (!checkedAdd(transformed_data_size, entry.source_length, transformed_data_size) ||
            transformed_data_size > max_decoded_size) {
            return fail(error, "decompressed WOFF2 data is too large");
        }
        tables.push_back(std::move(entry));
    }

    const size_t compressed_offset = 48u + directory.position();
    if (compressed_offset > size || compressed_size > size - compressed_offset) {
        return fail(error, "compressed font data exceeds WOFF2 file");
    }
    const size_t compressed_end = compressed_offset + compressed_size;
    size_t previous_block_end = compressed_end;
    if (meta_offset == 0) {
        if (meta_length != 0 || meta_orig_length != 0) {
            return fail(error, "metadata lengths require a metadata offset");
        }
    } else {
        const size_t aligned_offset = (compressed_end + 3u) & ~size_t(3u);
        if (meta_length == 0 || meta_orig_length == 0 || (meta_offset & 3u) != 0 ||
            meta_offset != aligned_offset || meta_offset > size ||
            meta_length > size - meta_offset ||
            !zeroPadding(data, compressed_end, meta_offset)) {
            return fail(error, "invalid WOFF2 metadata block offset or length");
        }
        previous_block_end = static_cast<size_t>(meta_offset) + meta_length;
    }
    if (priv_offset == 0) {
        if (priv_length != 0) {
            return fail(error, "private length requires a private offset");
        }
    } else {
        const size_t aligned_offset = (previous_block_end + 3u) & ~size_t(3u);
        if (priv_length == 0 || (priv_offset & 3u) != 0 || priv_offset != aligned_offset ||
            priv_offset > size || priv_length > size - priv_offset ||
            static_cast<size_t>(priv_offset) + priv_length != size ||
            !zeroPadding(data, previous_block_end, priv_offset)) {
            return fail(error, "invalid WOFF2 private block offset or length");
        }
    }

    uint32_t glyf_index = no_table_index;
    uint32_t loca_index = no_table_index;
    uint32_t hmtx_index = no_table_index;
    uint32_t head_index = no_table_index;
    uint32_t hhea_index = no_table_index;
    uint32_t maxp_index = no_table_index;
    for (uint32_t index = 0; index < tables.size(); ++index) {
        const uint32_t tag = tables[index].tag;
        if (tag == tag_glyf) glyf_index = index;
        if (tag == tag_loca) loca_index = index;
        if (tag == tag_hmtx) hmtx_index = index;
        if (tag == tag_head) head_index = index;
        if (tag == tag_hhea) hhea_index = index;
        if (tag == tag_maxp) maxp_index = index;
    }
    if ((glyf_index == no_table_index) != (loca_index == no_table_index)) {
        return fail(error, "glyf and loca tables must appear together");
    }
    if (glyf_index != no_table_index) {
        if (loca_index < glyf_index ||
            tables[glyf_index].transformed != tables[loca_index].transformed) {
            return fail(error, "invalid glyf/loca table order or transform pairing");
        }
    }
    if (head_index == no_table_index || tables[head_index].orig_length < 54) {
        return fail(error, "a valid head table is required");
    }

    std::vector<uint8_t> decompressed(transformed_data_size);
    size_t decoded_size = transformed_data_size;
    uint8_t empty_output = 0;
    uint8_t* output_pointer = decompressed.empty() ? &empty_output : decompressed.data();
    const BrotliDecoderResult brotli_result = BrotliDecoderDecompress(
        compressed_size, data + compressed_offset, &decoded_size, output_pointer);
    if (brotli_result != BROTLI_DECODER_RESULT_SUCCESS || decoded_size != transformed_data_size) {
        return fail(error, "Brotli decompression failed or produced an invalid size");
    }

    for (TableEntry& table : tables) {
        if (!table.transformed) {
            const uint8_t* source = decompressed.data() + table.source_offset;
            table.data.insert(table.data.end(), source, source + table.source_length);
        }
    }

    std::vector<int16_t> x_mins;
    uint16_t num_glyphs = 0;
    if (glyf_index != no_table_index) {
        TableEntry& glyf = tables[glyf_index];
        TableEntry& loca = tables[loca_index];
        if (glyf.transformed) {
            uint16_t index_format = 0;
            if (!decodeTransformedGlyf(decompressed.data() + glyf.source_offset,
                                       glyf.source_length, loca.orig_length, glyf.data, loca.data,
                                       x_mins, num_glyphs, index_format, error)) {
                return false;
            }
            if (loadS16(tables[head_index].data.data() + 50) != index_format) {
                return fail(error, "glyf indexFormat disagrees with head table");
            }
            if (maxp_index == no_table_index || tables[maxp_index].data.size() < 6 ||
                loadU16(tables[maxp_index].data.data() + 4) != num_glyphs) {
                return fail(error, "glyf glyph count disagrees with maxp table");
            }
        } else {
            if (maxp_index == no_table_index ||
                !analyzeNullGlyf(glyf, loca, tables[maxp_index], tables[head_index],
                                 x_mins, num_glyphs, error)) {
                if (maxp_index == no_table_index) {
                    return fail(error, "untransformed glyf data requires maxp table");
                }
                return false;
            }
        }
    }

    if (hmtx_index != no_table_index && tables[hmtx_index].transformed) {
        if (glyf_index == no_table_index || hhea_index == no_table_index ||
            x_mins.size() != num_glyphs) {
            return fail(error, "hmtx transform requires complete glyf and hhea tables");
        }
        if (!reconstructHmtx(tables[hmtx_index],
                             decompressed.data() + tables[hmtx_index].source_offset,
                             x_mins, tables[hhea_index], tables[hmtx_index].data, error)) {
            return false;
        }
    }

    std::vector<uint8_t>& head_data = tables[head_index].data;
    std::fill(head_data.begin() + 8, head_data.begin() + 12, 0);
    std::vector<uint32_t> order(tables.size());
    for (uint32_t index = 0; index < order.size(); ++index) {
        order[index] = index;
    }
    std::sort(order.begin(), order.end(), [&](uint32_t left, uint32_t right) {
        return tables[left].tag < tables[right].tag;
    });

    size_t output_size = 12u + tables.size() * 16u;
    for (const TableEntry& table : tables) {
        const size_t padded_size = (table.data.size() + 3u) & ~size_t(3u);
        if (!checkedAdd(output_size, padded_size, output_size) ||
            output_size > max_decoded_size || output_size > std::numeric_limits<uint32_t>::max()) {
            return fail(error, "reconstructed sfnt font is too large");
        }
    }
    out_sfnt.assign(output_size, 0);
    storeU32(out_sfnt.data(), flavor);
    storeU16(out_sfnt.data() + 4, num_tables);
    uint16_t entry_selector = 0;
    uint32_t table_power = 1;
    while (table_power * 2u <= num_tables) {
        table_power *= 2u;
        ++entry_selector;
    }
    const uint32_t search_range = table_power * 16u;
    storeU16(out_sfnt.data() + 6, static_cast<uint16_t>(search_range));
    storeU16(out_sfnt.data() + 8, entry_selector);
    storeU16(out_sfnt.data() + 10, static_cast<uint16_t>(num_tables * 16u - search_range));

    size_t table_offset = 12u + tables.size() * 16u;
    size_t head_output_offset = 0;
    for (size_t order_index = 0; order_index < order.size(); ++order_index) {
        TableEntry& table = tables[order[order_index]];
        uint8_t* directory_entry = out_sfnt.data() + 12u + order_index * 16u;
        storeU32(directory_entry, table.tag);
        storeU32(directory_entry + 4, calculateChecksum(table.data.data(), table.data.size()));
        storeU32(directory_entry + 8, static_cast<uint32_t>(table_offset));
        storeU32(directory_entry + 12, static_cast<uint32_t>(table.data.size()));
        if (!table.data.empty()) {
            std::copy(table.data.begin(), table.data.end(), out_sfnt.begin() + table_offset);
        }
        if (table.tag == tag_head) {
            head_output_offset = table_offset;
        }
        table_offset += (table.data.size() + 3u) & ~size_t(3u);
    }

    const uint32_t font_checksum = calculateChecksum(out_sfnt.data(), out_sfnt.size());
    storeU32(out_sfnt.data() + head_output_offset + 8, 0xb1b0afbau - font_checksum);
    return true;
}

} // namespace

bool decodeWoff2(const uint8_t* data, size_t size, std::vector<uint8_t>& out_sfnt,
                 std::string& error) {
    out_sfnt.clear();
    error.clear();
    try {
        return decodeWoff2Impl(data, size, out_sfnt, error);
    } catch (const std::bad_alloc&) {
        out_sfnt.clear();
        return fail(error, "not enough memory to decode WOFF2 font");
    } catch (const std::length_error&) {
        out_sfnt.clear();
        return fail(error, "WOFF2 decoded size exceeds container limits");
    }
}

} // namespace nector_font_detail
