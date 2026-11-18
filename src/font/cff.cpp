#include "cff.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace nector_font_detail {
namespace {

constexpr size_t k_max_dict_operands = 48;
constexpr size_t k_max_type2_operands = 48;
constexpr int k_max_subroutine_depth = 10;
constexpr size_t k_max_instructions = 1000000;
constexpr size_t k_max_path_commands = 1000000;
constexpr int k_cid_ros = 1230;
constexpr int k_fd_array = 1236;
constexpr int k_fd_select = 1237;
constexpr int k_max_seac_depth = 10;
constexpr uint16_t k_invalid_glyph = 0xffffU;

constexpr uint16_t k_standard_encoding[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52,
    53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
    65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76,
    77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 93, 94, 95, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 96, 97, 98, 99, 100, 101, 102,
    103, 104, 105, 106, 107, 108, 109, 110, 0, 111, 112, 113,
    114, 0, 115, 116, 117, 118, 119, 120, 121, 122, 0, 123,
    0, 124, 125, 126, 127, 128, 129, 130, 131, 0, 132, 133,
    0, 134, 135, 136, 137, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 138, 0, 139,
    0, 0, 0, 0, 140, 141, 142, 143, 0, 0, 0, 0,
    0, 144, 0, 0, 0, 145, 0, 0, 146, 147, 148, 149,
    0, 0, 0, 0,
};

constexpr uint16_t k_expert_encoding[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 229, 230, 0,
    231, 232, 233, 234, 235, 236, 237, 238, 13, 14, 15, 99,
    239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 27, 28,
    249, 250, 251, 252, 0, 253, 254, 255, 256, 257, 0, 0,
    0, 258, 0, 0, 259, 260, 261, 262, 0, 0, 263, 264,
    265, 0, 266, 109, 110, 267, 268, 269, 0, 270, 271, 272,
    273, 274, 275, 276, 277, 278, 279, 280, 281, 282, 283, 284,
    285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296,
    297, 298, 299, 300, 301, 302, 303, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 304, 305, 306, 0, 0, 307, 308,
    309, 310, 311, 0, 312, 0, 0, 313, 0, 0, 314, 315,
    0, 0, 316, 317, 318, 0, 0, 0, 158, 155, 163, 319,
    320, 321, 322, 323, 324, 325, 0, 0, 326, 150, 164, 169,
    327, 328, 329, 330, 331, 332, 333, 334, 335, 336, 337, 338,
    339, 340, 341, 342, 343, 344, 345, 346, 347, 348, 349, 350,
    351, 352, 353, 354, 355, 356, 357, 358, 359, 360, 361, 362,
    363, 364, 365, 366, 367, 368, 369, 370, 371, 372, 373, 374,
    375, 376, 377, 378,
};

constexpr uint16_t k_expert_charset[166] = {
    0, 1, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238,
    13, 14, 15, 99, 239, 240, 241, 242, 243, 244, 245, 246,
    247, 248, 27, 28, 249, 250, 251, 252, 253, 254, 255, 256,
    257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 109, 110,
    267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278,
    279, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290,
    291, 292, 293, 294, 295, 296, 297, 298, 299, 300, 301, 302,
    303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313, 314,
    315, 316, 317, 318, 158, 155, 163, 319, 320, 321, 322, 323,
    324, 325, 326, 150, 164, 169, 327, 328, 329, 330, 331, 332,
    333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343, 344,
    345, 346, 347, 348, 349, 350, 351, 352, 353, 354, 355, 356,
    357, 358, 359, 360, 361, 362, 363, 364, 365, 366, 367, 368,
    369, 370, 371, 372, 373, 374, 375, 376, 377, 378,
};

constexpr uint16_t k_expert_subset_charset[87] = {
    0, 1, 231, 232, 235, 236, 237, 238, 13, 14, 15, 99,
    239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 27, 28,
    249, 250, 251, 253, 254, 255, 256, 257, 258, 259, 260, 261,
    262, 263, 264, 265, 266, 109, 110, 267, 268, 269, 270, 272,
    300, 301, 302, 305, 314, 315, 158, 155, 163, 320, 321, 322,
    323, 324, 325, 326, 150, 164, 169, 327, 328, 329, 330, 331,
    332, 333, 334, 335, 336, 337, 338, 339, 340, 341, 342, 343,
    344, 345, 346,
};

bool fail(std::string& error, const char* message) {
    error = message;
    return false;
}

bool readU8(const std::vector<uint8_t>& data, size_t& pos, uint8_t& value) {
    if (pos >= data.size()) {
        return false;
    }
    value = data[pos++];
    return true;
}

bool readU16(const std::vector<uint8_t>& data, size_t& pos, uint16_t& value) {
    if (pos > data.size() || data.size() - pos < 2) {
        return false;
    }
    value = static_cast<uint16_t>((static_cast<uint16_t>(data[pos]) << 8) |
                                  static_cast<uint16_t>(data[pos + 1]));
    pos += 2;
    return true;
}

bool readOffset(const std::vector<uint8_t>& data, size_t& pos, uint8_t width,
                uint32_t& value) {
    if (width < 1 || width > 4 || pos > data.size() ||
        data.size() - pos < width) {
        return false;
    }
    value = 0;
    for (uint8_t i = 0; i < width; ++i) {
        value = (value << 8) | static_cast<uint32_t>(data[pos++]);
    }
    return true;
}

struct Object {
    size_t offset = 0;
    size_t size = 0;
};

struct Index {
    uint32_t count = 0;
    uint8_t off_size = 0;
    size_t offsets_pos = 0;
    size_t data_pos = 0;
    size_t end = 0;
};

bool readIndex(const std::vector<uint8_t>& data, size_t& pos, Index& index,
               std::string& error) {
    uint16_t count = 0;
    if (!readU16(data, pos, count)) {
        return fail(error, "truncated CFF INDEX count");
    }
    index = Index();
    index.count = count;
    if (count == 0) {
        index.end = pos;
        return true;
    }
    if (!readU8(data, pos, index.off_size) || index.off_size < 1 ||
        index.off_size > 4) {
        return fail(error, "invalid CFF INDEX offset size");
    }
    index.offsets_pos = pos;
    const size_t offset_count = static_cast<size_t>(count) + 1;
    if (pos > data.size() ||
        offset_count > (data.size() - pos) / index.off_size) {
        return fail(error, "truncated CFF INDEX offsets");
    }
    size_t offset_pos = pos;
    uint32_t previous = 0;
    uint32_t last = 0;
    for (size_t i = 0; i < offset_count; ++i) {
        uint32_t current = 0;
        if (!readOffset(data, offset_pos, index.off_size, current)) {
            return fail(error, "truncated CFF INDEX offset");
        }
        if ((i == 0 && current != 1) || (i != 0 && current < previous)) {
            return fail(error, "invalid CFF INDEX offset ordering");
        }
        previous = current;
        last = current;
    }
    pos = offset_pos;
    index.data_pos = pos;
    const uint64_t data_size = static_cast<uint64_t>(last) - 1;
    if (data_size > static_cast<uint64_t>(data.size() - pos)) {
        return fail(error, "truncated CFF INDEX object data");
    }
    index.end = pos + static_cast<size_t>(data_size);
    pos = index.end;
    return true;
}

Object indexObject(const std::vector<uint8_t>& data, const Index& index,
                   uint32_t object_index) {
    size_t pos = index.offsets_pos + static_cast<size_t>(object_index) * index.off_size;
    uint32_t first = 0;
    uint32_t second = 0;
    readOffset(data, pos, index.off_size, first);
    readOffset(data, pos, index.off_size, second);
    Object object;
    object.offset = index.data_pos + static_cast<size_t>(first - 1);
    object.size = static_cast<size_t>(second - first);
    return object;
}

int subroutineBias(uint32_t count) {
    if (count < 1240) {
        return 107;
    }
    if (count < 33900) {
        return 1131;
    }
    return 32768;
}

struct DictEntry {
    int op = 0;
    std::vector<double> args;
};

struct Dict {
    std::vector<DictEntry> entries;
};

const DictEntry* dictEntry(const Dict& dict, int op) {
    for (auto it = dict.entries.rbegin(); it != dict.entries.rend(); ++it) {
        if (it->op == op) {
            return &*it;
        }
    }
    return nullptr;
}

bool dictInteger(const DictEntry* entry, size_t index, int32_t& value) {
    if (entry == nullptr || index >= entry->args.size()) {
        return false;
    }
    const double number = entry->args[index];
    if (!std::isfinite(number) || std::floor(number) != number ||
        number < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
        number > static_cast<double>(std::numeric_limits<int32_t>::max())) {
        return false;
    }
    value = static_cast<int32_t>(number);
    return true;
}

bool parseDictReal(const std::vector<uint8_t>& data, size_t& pos, size_t end,
                   double& result) {
    long double mantissa = 0.0L;
    int fractional_digits = 0;
    int exponent = 0;
    bool negative = false;
    bool after_decimal = false;
    bool decimal_seen = false;
    bool exponent_seen = false;
    bool exponent_negative = false;
    bool any_digit = false;
    bool finished = false;
    while (pos < end && !finished) {
        const uint8_t byte = data[pos++];
        for (int nibble_index = 0; nibble_index < 2 && !finished; ++nibble_index) {
            const int nibble = nibble_index == 0 ? (byte >> 4) : (byte & 15);
            if (nibble <= 9) {
                any_digit = true;
                if (exponent_seen) {
                    if (exponent < 10000) {
                        exponent = std::min(10000, exponent * 10 + nibble);
                    }
                } else {
                    mantissa = mantissa * 10.0L + static_cast<long double>(nibble);
                    if (!std::isfinite(mantissa)) {
                        return false;
                    }
                    if (after_decimal) {
                        if (fractional_digits < 10000) {
                            ++fractional_digits;
                        }
                    }
                }
            } else if (nibble == 10) {
                if (decimal_seen || exponent_seen) {
                    return false;
                }
                decimal_seen = true;
                after_decimal = true;
            } else if (nibble == 11 || nibble == 12) {
                if (exponent_seen) {
                    return false;
                }
                exponent_seen = true;
                exponent_negative = nibble == 12;
                after_decimal = false;
            } else if (nibble == 14) {
                if (any_digit || decimal_seen || exponent_seen || negative) {
                    return false;
                }
                negative = true;
            } else if (nibble == 15) {
                finished = true;
            } else {
                return false;
            }
        }
    }
    if (!finished || !any_digit) {
        return false;
    }
    int decimal_exponent = (exponent_negative ? -exponent : exponent) - fractional_digits;
    long double value = mantissa;
    if (decimal_exponent > 5000) {
        return false;
    }
    if (decimal_exponent < -5000) {
        value = 0.0L;
    } else if (decimal_exponent != 0) {
        value *= std::pow(10.0L, static_cast<long double>(decimal_exponent));
    }
    if (negative) {
        value = -value;
    }
    result = static_cast<double>(value);
    return std::isfinite(result);
}

bool pushDictOperand(std::vector<double>& operands, double value) {
    if (operands.size() >= k_max_dict_operands || !std::isfinite(value)) {
        return false;
    }
    operands.push_back(value);
    return true;
}

bool parseDict(const std::vector<uint8_t>& data, Object object, Dict& dict,
               std::string& error) {
    if (object.offset > data.size() || object.size > data.size() - object.offset) {
        return fail(error, "invalid CFF DICT bounds");
    }
    size_t pos = object.offset;
    const size_t end = object.offset + object.size;
    std::vector<double> operands;
    while (pos < end) {
        const uint8_t byte = data[pos++];
        double value = 0.0;
        if (byte >= 32 && byte <= 246) {
            value = static_cast<double>(static_cast<int>(byte) - 139);
        } else if (byte >= 247 && byte <= 250) {
            if (pos >= end) {
                return fail(error, "truncated CFF DICT integer");
            }
            value = static_cast<double>((static_cast<int>(byte) - 247) * 256 +
                                        static_cast<int>(data[pos++]) + 108);
        } else if (byte >= 251 && byte <= 254) {
            if (pos >= end) {
                return fail(error, "truncated CFF DICT integer");
            }
            value = static_cast<double>(-(static_cast<int>(byte) - 251) * 256 -
                                        static_cast<int>(data[pos++]) - 108);
        } else if (byte == 28) {
            if (end - pos < 2) {
                return fail(error, "truncated CFF DICT int16");
            }
            const int16_t number = static_cast<int16_t>(
                (static_cast<uint16_t>(data[pos]) << 8) | data[pos + 1]);
            pos += 2;
            value = number;
        } else if (byte == 29) {
            if (end - pos < 4) {
                return fail(error, "truncated CFF DICT int32");
            }
            const uint32_t bits = (static_cast<uint32_t>(data[pos]) << 24) |
                                  (static_cast<uint32_t>(data[pos + 1]) << 16) |
                                  (static_cast<uint32_t>(data[pos + 2]) << 8) |
                                  static_cast<uint32_t>(data[pos + 3]);
            pos += 4;
            value = static_cast<int32_t>(bits);
        } else if (byte == 30) {
            if (!parseDictReal(data, pos, end, value)) {
                return fail(error, "invalid CFF DICT real number");
            }
        } else if (byte == 255) {
            if (end - pos < 4) {
                return fail(error, "truncated CFF DICT fixed number");
            }
            const uint32_t bits = (static_cast<uint32_t>(data[pos]) << 24) |
                                  (static_cast<uint32_t>(data[pos + 1]) << 16) |
                                  (static_cast<uint32_t>(data[pos + 2]) << 8) |
                                  static_cast<uint32_t>(data[pos + 3]);
            pos += 4;
            value = static_cast<double>(static_cast<int32_t>(bits)) / 65536.0;
        } else {
            int op = byte;
            if (byte == 12) {
                if (pos >= end) {
                    return fail(error, "truncated CFF DICT escaped operator");
                }
                op = 1200 + data[pos++];
            }
            if (byte > 21 && byte != 12) {
                error = "reserved CFF DICT operator at offset " + std::to_string(pos - 1);
                return false;
            }
            DictEntry entry;
            entry.op = op;
            entry.args = std::move(operands);
            dict.entries.push_back(std::move(entry));
            operands.clear();
            continue;
        }
        if (!pushDictOperand(operands, value)) {
            return fail(error, "CFF DICT operand stack limit exceeded");
        }
    }
    if (!operands.empty()) {
        return fail(error, "unterminated CFF DICT operands");
    }
    return true;
}

struct PrivateInfo {
    double default_width = 0.0;
    double nominal_width = 0.0;
    bool has_subroutines = false;
    Index subroutines;
};

bool parsePrivate(const std::vector<uint8_t>& data, const Dict& font_dict,
                  PrivateInfo& info, bool required, std::string& error) {
    const DictEntry* private_entry = dictEntry(font_dict, 18);
    if (private_entry == nullptr) {
        return !required || fail(error, "CFF font DICT is missing Private");
    }
    int32_t private_size = 0;
    int32_t private_offset = 0;
    if (private_entry->args.size() != 2 ||
        !dictInteger(private_entry, 0, private_size) ||
        !dictInteger(private_entry, 1, private_offset) || private_size < 0 ||
        private_offset < 0) {
        return fail(error, "invalid CFF Private DICT location");
    }
    const size_t offset = static_cast<size_t>(private_offset);
    const size_t size = static_cast<size_t>(private_size);
    if (offset > data.size() || size > data.size() - offset) {
        return fail(error, "CFF Private DICT is out of bounds");
    }
    Dict private_dict;
    Object object;
    object.offset = offset;
    object.size = size;
    if (!parseDict(data, object, private_dict, error)) {
        return false;
    }
    const DictEntry* default_width = dictEntry(private_dict, 20);
    const DictEntry* nominal_width = dictEntry(private_dict, 21);
    if ((default_width != nullptr && default_width->args.size() != 1) ||
        (nominal_width != nullptr && nominal_width->args.size() != 1)) {
        return fail(error, "invalid CFF Private width value");
    }
    if (default_width != nullptr) {
        info.default_width = default_width->args[0];
    }
    if (nominal_width != nullptr) {
        info.nominal_width = nominal_width->args[0];
    }
    const DictEntry* subrs = dictEntry(private_dict, 19);
    if (subrs != nullptr) {
        int32_t relative_offset = 0;
        if (subrs->args.size() != 1 || !dictInteger(subrs, 0, relative_offset) ||
            relative_offset < 0) {
            return fail(error, "invalid CFF local Subrs offset");
        }
        const uint64_t absolute = static_cast<uint64_t>(offset) +
                                  static_cast<uint32_t>(relative_offset);
        if (absolute > data.size()) {
            return fail(error, "CFF local Subrs INDEX is out of bounds");
        }
        size_t subrs_pos = static_cast<size_t>(absolute);
        if (!readIndex(data, subrs_pos, info.subroutines, error)) {
            return false;
        }
        info.has_subroutines = true;
    }
    return true;
}

bool parseCharset(const std::vector<uint8_t>& data, int32_t offset,
                  uint32_t glyph_count, bool cid_keyed,
                  std::vector<uint16_t>& glyph_sids, std::string& error) {
    if (offset < 0 || glyph_count == 0 || (cid_keyed && offset <= 2)) {
        return fail(error, "invalid CFF charset");
    }
    if (!cid_keyed) {
        glyph_sids.assign(glyph_count, 0);
    }
    if (offset == 0) {
        if (glyph_count > 229) {
            return fail(error, "CFF ISOAdobe charset is too small for glyph count");
        }
        for (uint32_t gid = 1; gid < glyph_count; ++gid) {
            glyph_sids[gid] = static_cast<uint16_t>(gid);
        }
        return true;
    }
    if (offset == 1 || offset == 2) {
        const uint16_t* charset = offset == 1 ? k_expert_charset : k_expert_subset_charset;
        const size_t charset_count = offset == 1 ? 166U : 87U;
        if (glyph_count > charset_count) {
            return fail(error, "predefined CFF charset is too small for glyph count");
        }
        if (!cid_keyed) {
            std::copy(charset, charset + glyph_count, glyph_sids.begin());
        }
        return true;
    }
    size_t pos = static_cast<size_t>(offset);
    uint8_t format = 0;
    if (!readU8(data, pos, format)) {
        return fail(error, "truncated CFF charset");
    }
    const uint32_t required = glyph_count - 1;
    if (format == 0) {
        if (pos > data.size() || required > (data.size() - pos) / 2) {
            return fail(error, "truncated CFF format 0 charset");
        }
        for (uint32_t gid = 1; gid < glyph_count; ++gid) {
            uint16_t sid = 0;
            if (!readU16(data, pos, sid)) {
                return fail(error, "truncated CFF format 0 charset");
            }
            if (!cid_keyed) {
                glyph_sids[gid] = sid;
            }
        }
        return true;
    }
    if (format != 1 && format != 2) {
        return fail(error, "unsupported CFF charset format");
    }
    uint32_t covered = 0;
    while (covered < required) {
        uint16_t first_sid = 0;
        uint16_t range_count = 0;
        uint8_t small_count = 0;
        if (!readU16(data, pos, first_sid)) {
            return fail(error, "truncated CFF charset range");
        }
        if (format == 1) {
            if (!readU8(data, pos, small_count)) {
                return fail(error, "truncated CFF charset range");
            }
            range_count = small_count;
        } else if (!readU16(data, pos, range_count)) {
            return fail(error, "truncated CFF charset range");
        }
        const uint32_t range_size = static_cast<uint32_t>(range_count) + 1;
        if (range_size > required - covered ||
            static_cast<uint32_t>(first_sid) + range_count > 65535) {
            return fail(error, "invalid CFF charset range");
        }
        if (!cid_keyed) {
            for (uint32_t i = 0; i < range_size; ++i) {
                glyph_sids[1 + covered + i] = static_cast<uint16_t>(first_sid + i);
            }
        }
        covered += range_size;
    }
    return true;
}

} // namespace

struct CffFont::Impl {
    std::vector<uint8_t> data;
    Index charstrings;
    Index global_subroutines;
    PrivateInfo private_info;
    std::vector<PrivateInfo> fd_private;
    std::vector<uint8_t> glyph_fd;
    std::vector<uint16_t> glyph_sids;
    std::vector<uint16_t> sid_to_glyph;
    std::vector<uint16_t> encoding_to_glyph;
    bool cid_keyed = false;

    bool parse(std::string& error) {
        if (data.size() < 4) {
            return fail(error, "truncated CFF header");
        }
        const uint8_t major = data[0];
        if (major == 2) {
            return fail(error, "CFF2 is not supported; CFF 1.0 is required");
        }
        if (major != 1 || data[2] < 4 || data[2] > data.size() ||
            data[3] < 1 || data[3] > 4) {
            return fail(error, "invalid CFF header");
        }
        size_t pos = data[2];
        Index names;
        Index top_dicts;
        Index strings;
        if (!readIndex(data, pos, names, error) ||
            !readIndex(data, pos, top_dicts, error) ||
            !readIndex(data, pos, strings, error) ||
            !readIndex(data, pos, global_subroutines, error)) {
            return false;
        }
        if (names.count != 1 || top_dicts.count != 1) {
            return fail(error, "OpenType CFF requires one Name and one Top DICT");
        }
        Dict top_dict;
        if (!parseDict(data, indexObject(data, top_dicts, 0), top_dict, error)) {
            return false;
        }
        int32_t charstrings_offset = 0;
        const DictEntry* charstrings_entry = dictEntry(top_dict, 17);
        if (charstrings_entry == nullptr || charstrings_entry->args.size() != 1 ||
            !dictInteger(charstrings_entry, 0, charstrings_offset) ||
            charstrings_offset < 0 || static_cast<size_t>(charstrings_offset) >= data.size()) {
            return fail(error, "invalid CFF CharStrings offset");
        }
        pos = static_cast<size_t>(charstrings_offset);
        if (!readIndex(data, pos, charstrings, error) || charstrings.count == 0) {
            if (error.empty()) {
                error = "CFF CharStrings INDEX is empty";
            }
            return false;
        }
        glyph_fd.assign(charstrings.count, 0);

        const DictEntry* ros = dictEntry(top_dict, k_cid_ros);
        cid_keyed = ros != nullptr;
        int32_t charset_offset = 0;
        const DictEntry* charset_entry = dictEntry(top_dict, 15);
        if (charset_entry != nullptr &&
            (charset_entry->args.size() != 1 ||
             !dictInteger(charset_entry, 0, charset_offset) || charset_offset < 0)) {
            return fail(error, "invalid CFF charset offset");
        }
        if (!parseCharset(data, charset_offset, charstrings.count, cid_keyed,
                          glyph_sids, error)) {
            return false;
        }

        if (cid_keyed) {
            if (ros->args.size() != 3) {
                return fail(error, "invalid CFF ROS operator");
            }
            const DictEntry* fd_array_entry = dictEntry(top_dict, k_fd_array);
            const DictEntry* fd_select_entry = dictEntry(top_dict, k_fd_select);
            int32_t fd_array_offset = 0;
            int32_t fd_select_offset = 0;
            if (fd_array_entry == nullptr || fd_select_entry == nullptr ||
                fd_array_entry->args.size() != 1 || fd_select_entry->args.size() != 1 ||
                !dictInteger(fd_array_entry, 0, fd_array_offset) ||
                !dictInteger(fd_select_entry, 0, fd_select_offset) ||
                fd_array_offset < 0 || fd_select_offset < 0) {
                return fail(error, "CID-keyed CFF requires FDArray and FDSelect");
            }
            pos = static_cast<size_t>(fd_array_offset);
            Index fd_array;
            if (!readIndex(data, pos, fd_array, error) || fd_array.count == 0) {
                if (error.empty()) {
                    error = "CFF FDArray is empty";
                }
                return false;
            }
            fd_private.resize(fd_array.count);
            for (uint32_t i = 0; i < fd_array.count; ++i) {
                Dict font_dict;
                if (!parseDict(data, indexObject(data, fd_array, i), font_dict, error) ||
                    !parsePrivate(data, font_dict, fd_private[i], true, error)) {
                    return false;
                }
            }
            if (!parseFdSelect(static_cast<size_t>(fd_select_offset), error)) {
                return false;
            }
        } else {
            if (!parsePrivate(data, top_dict, private_info, false, error) ||
                !parseEncoding(top_dict, error)) {
                return false;
            }
        }
        return true;
    }

    bool parseEncoding(const Dict& top_dict, std::string& error) {
        encoding_to_glyph.assign(256, k_invalid_glyph);
        sid_to_glyph.assign(65536, k_invalid_glyph);
        for (uint32_t gid = 1; gid < charstrings.count; ++gid) {
            const uint16_t sid = glyph_sids[gid];
            if (sid != 0 && sid_to_glyph[sid] == k_invalid_glyph) {
                sid_to_glyph[sid] = static_cast<uint16_t>(gid);
            }
        }

        int32_t encoding_offset = 0;
        const DictEntry* encoding_entry = dictEntry(top_dict, 16);
        if (encoding_entry != nullptr &&
            (encoding_entry->args.size() != 1 ||
             !dictInteger(encoding_entry, 0, encoding_offset) || encoding_offset < 0)) {
            return fail(error, "invalid CFF Encoding offset");
        }
        if (encoding_offset <= 1) {
            const uint16_t* encoding = encoding_offset == 0 ? k_standard_encoding
                                                             : k_expert_encoding;
            for (size_t code = 0; code < 256; ++code) {
                const uint16_t sid = encoding[code];
                if (sid != 0) {
                    encoding_to_glyph[code] = sid_to_glyph[sid];
                }
            }
            return true;
        }

        size_t pos = static_cast<size_t>(encoding_offset);
        uint8_t raw_format = 0;
        if (!readU8(data, pos, raw_format)) {
            return fail(error, "truncated CFF Encoding");
        }
        const uint8_t format = raw_format & 0x7fU;
        const bool supplemented = (raw_format & 0x80U) != 0;
        uint32_t gid = 1;
        if (format == 0) {
            uint8_t count = 0;
            if (!readU8(data, pos, count) || count > charstrings.count - 1) {
                return fail(error, "invalid CFF format 0 Encoding size");
            }
            for (uint32_t i = 0; i < count; ++i, ++gid) {
                uint8_t code = 0;
                if (!readU8(data, pos, code)) {
                    return fail(error, "truncated CFF format 0 Encoding");
                }
                encoding_to_glyph[code] = static_cast<uint16_t>(gid);
            }
        } else if (format == 1) {
            uint8_t range_count = 0;
            if (!readU8(data, pos, range_count)) {
                return fail(error, "truncated CFF format 1 Encoding");
            }
            for (uint32_t i = 0; i < range_count; ++i) {
                uint8_t first = 0;
                uint8_t left = 0;
                if (!readU8(data, pos, first) || !readU8(data, pos, left)) {
                    return fail(error, "truncated CFF format 1 Encoding range");
                }
                const uint32_t range_size = static_cast<uint32_t>(left) + 1;
                if (static_cast<uint32_t>(first) + range_size > 256 ||
                    range_size > charstrings.count - gid) {
                    return fail(error, "invalid CFF format 1 Encoding range");
                }
                for (uint32_t j = 0; j < range_size; ++j, ++gid) {
                    encoding_to_glyph[first + j] = static_cast<uint16_t>(gid);
                }
            }
        } else {
            return fail(error, "unsupported CFF Encoding format");
        }

        if (supplemented) {
            uint8_t supplement_count = 0;
            if (!readU8(data, pos, supplement_count)) {
                return fail(error, "truncated CFF Encoding supplements");
            }
            for (uint32_t i = 0; i < supplement_count; ++i) {
                uint8_t code = 0;
                uint16_t sid = 0;
                if (!readU8(data, pos, code) || !readU16(data, pos, sid) ||
                    sid == 0 || sid_to_glyph[sid] == k_invalid_glyph) {
                    return fail(error, "invalid CFF Encoding supplement");
                }
                encoding_to_glyph[code] = sid_to_glyph[sid];
            }
        }
        return true;
    }

    uint16_t glyphForEncoding(int32_t code) const {
        if (code < 0 || code >= 256 || encoding_to_glyph.empty()) {
            return k_invalid_glyph;
        }
        return encoding_to_glyph[static_cast<size_t>(code)];
    }

    bool parseFdSelect(size_t offset, std::string& error) {
        size_t pos = offset;
        uint8_t format = 0;
        if (!readU8(data, pos, format)) {
            return fail(error, "truncated CFF FDSelect");
        }
        if (format == 0) {
            for (uint32_t gid = 0; gid < charstrings.count; ++gid) {
                uint8_t fd = 0;
                if (!readU8(data, pos, fd)) {
                    return fail(error, "truncated CFF format 0 FDSelect");
                }
                if (fd >= fd_private.size()) {
                    return fail(error, "CFF FDSelect font index is out of range");
                }
                glyph_fd[gid] = fd;
            }
            return true;
        }
        if (format != 3) {
            return fail(error, "unsupported CFF FDSelect format");
        }
        uint16_t range_count = 0;
        if (!readU16(data, pos, range_count) || range_count == 0 ||
            range_count > charstrings.count) {
            return fail(error, "invalid CFF format 3 FDSelect range count");
        }
        std::vector<uint16_t> first_glyphs(range_count);
        std::vector<uint8_t> fds(range_count);
        for (uint16_t i = 0; i < range_count; ++i) {
            if (!readU16(data, pos, first_glyphs[i]) || !readU8(data, pos, fds[i]) ||
                fds[i] >= fd_private.size() ||
                (i == 0 && first_glyphs[i] != 0) ||
                (i != 0 && first_glyphs[i] <= first_glyphs[i - 1]) ||
                first_glyphs[i] >= charstrings.count) {
                return fail(error, "invalid CFF format 3 FDSelect range");
            }
        }
        uint16_t sentinel = 0;
        if (!readU16(data, pos, sentinel) || sentinel != charstrings.count) {
            return fail(error, "invalid CFF FDSelect sentinel");
        }
        for (uint16_t i = 0; i < range_count; ++i) {
            const uint32_t end = i + 1 < range_count ? first_glyphs[i + 1] : sentinel;
            for (uint32_t gid = first_glyphs[i]; gid < end; ++gid) {
                glyph_fd[gid] = fds[i];
            }
        }
        return true;
    }

    const PrivateInfo& privateForGlyph(uint16_t glyph_index) const {
        return cid_keyed ? fd_private[glyph_fd[glyph_index]] : private_info;
    }

    struct ExecutionBudget {
        size_t instructions = 0;
        size_t path_commands = 0;
    };

    struct Interpreter {
        const Impl& font;
        const PrivateInfo& private_info;
        OutlineSink& sink;
        std::string& error;
        std::vector<double> stack;
        double transient_array[32] = {};
        double x = 0.0;
        double y = 0.0;
        double contour_start_x = 0.0;
        double contour_start_y = 0.0;
        bool have_position = false;
        bool contour_open = false;
        bool width_done = false;
        double width = 0.0;
        uint32_t stem_count = 0;
        ExecutionBudget& budget;
        double offset_x = 0.0;
        double offset_y = 0.0;
        uint32_t random_state = 0x6d2b79f5U;
        double min_x = 0.0;
        double min_y = 0.0;
        double max_x = 0.0;
        double max_y = 0.0;
        bool has_bounds = false;
        bool has_seac = false;
        uint16_t seac_base = 0;
        uint16_t seac_accent = 0;
        double seac_x = 0.0;
        double seac_y = 0.0;

        Interpreter(const Impl& font, const PrivateInfo& private_info,
                    OutlineSink& sink, std::string& error, ExecutionBudget& budget,
                    double offset_x, double offset_y)
            : font(font), private_info(private_info), sink(sink), error(error),
              budget(budget), offset_x(offset_x), offset_y(offset_y) {
            stack.reserve(k_max_type2_operands);
        }

        bool push(double value) {
            if (stack.size() >= k_max_type2_operands || !std::isfinite(value)) {
                return fail(error, "Type2 operand stack overflow or invalid number");
            }
            stack.push_back(value);
            return true;
        }

        bool stackCount(size_t count) {
            if (stack.size() < count) {
                return fail(error, "Type2 operand stack underflow");
            }
            return true;
        }

        bool integerOperand(double number, int32_t& value) {
            if (!std::isfinite(number) || std::floor(number) != number ||
                number < static_cast<double>(std::numeric_limits<int32_t>::min()) ||
                number > static_cast<double>(std::numeric_limits<int32_t>::max())) {
                return fail(error, "Type2 operator requires an integer operand");
            }
            value = static_cast<int32_t>(number);
            return true;
        }

        bool consumeWidth(size_t expected) {
            if (width_done) {
                return true;
            }
            width_done = true;
            width = private_info.default_width;
            if (stack.size() == expected + 1) {
                width = private_info.nominal_width + stack.front();
                if (!std::isfinite(width)) {
                    return fail(error, "invalid Type2 glyph width");
                }
                stack.erase(stack.begin());
            }
            return true;
        }

        bool consumeStemWidth() {
            if (width_done) {
                return true;
            }
            width_done = true;
            width = private_info.default_width;
            if ((stack.size() & 1U) != 0) {
                width = private_info.nominal_width + stack.front();
                if (!std::isfinite(width)) {
                    return fail(error, "invalid Type2 glyph width");
                }
                stack.erase(stack.begin());
            }
            return true;
        }

        bool stems() {
            if (!consumeStemWidth()) {
                return false;
            }
            if ((stack.size() & 1U) != 0) {
                return fail(error, "Type2 stem operator has an odd argument count");
            }
            const size_t new_stems = stack.size() / 2;
            if (new_stems > std::numeric_limits<uint32_t>::max() - stem_count) {
                return fail(error, "too many Type2 stems");
            }
            stem_count += static_cast<uint32_t>(new_stems);
            stack.clear();
            return true;
        }

        bool countInstruction() {
            if (budget.instructions >= k_max_instructions) {
                return fail(error, "Type2 instruction limit exceeded");
            }
            ++budget.instructions;
            return true;
        }

        bool countPathCommand() {
            if (budget.path_commands >= k_max_path_commands) {
                return fail(error, "Type2 path command limit exceeded");
            }
            ++budget.path_commands;
            return true;
        }

        bool validCoordinate(double value) const {
            return std::isfinite(value) &&
                   std::abs(value) <= static_cast<double>(std::numeric_limits<float>::max());
        }

        bool closeContour() {
            if (!contour_open) {
                return true;
            }
            if (!countPathCommand()) {
                return false;
            }
            contour_open = false;
            return sink.close() || fail(error, "outline sink rejected close");
        }

        bool moveTo(double dx, double dy) {
            if (!closeContour()) {
                return false;
            }
            x += dx;
            y += dy;
            const double mapped_x = x + offset_x;
            const double mapped_y = y + offset_y;
            if (!validCoordinate(x) || !validCoordinate(y) ||
                !validCoordinate(mapped_x) || !validCoordinate(mapped_y) ||
                !countPathCommand()) {
                return fail(error, "invalid Type2 moveto coordinate or path limit");
            }
            contour_start_x = x;
            contour_start_y = y;
            have_position = true;
            return sink.moveTo(static_cast<float>(mapped_x), static_cast<float>(mapped_y)) ||
                   fail(error, "outline sink rejected moveTo");
        }

        void includePoint(double px, double py) {
            if (!has_bounds) {
                min_x = max_x = px;
                min_y = max_y = py;
                has_bounds = true;
            } else {
                min_x = std::min(min_x, px);
                max_x = std::max(max_x, px);
                min_y = std::min(min_y, py);
                max_y = std::max(max_y, py);
            }
        }

        static double cubicValue(double p0, double p1, double p2, double p3, double t) {
            const double u = 1.0 - t;
            return u * u * u * p0 + 3.0 * u * u * t * p1 +
                   3.0 * u * t * t * p2 + t * t * t * p3;
        }

        void includeCubicAxis(double p0, double p1, double p2, double p3,
                              bool horizontal) {
            auto include = [&](double value) {
                if (horizontal) {
                    min_x = std::min(min_x, value);
                    max_x = std::max(max_x, value);
                } else {
                    min_y = std::min(min_y, value);
                    max_y = std::max(max_y, value);
                }
            };
            include(p0);
            include(p3);
            const double a = -p0 + 3.0 * p1 - 3.0 * p2 + p3;
            const double b = 2.0 * (p0 - 2.0 * p1 + p2);
            const double c = p1 - p0;
            auto include_root = [&](double t) {
                if (t > 0.0 && t < 1.0) {
                    include(cubicValue(p0, p1, p2, p3, t));
                }
            };
            if (std::abs(a) < 1e-14) {
                if (std::abs(b) >= 1e-14) {
                    include_root(-c / b);
                }
                return;
            }
            double discriminant = b * b - 4.0 * a * c;
            if (discriminant >= 0.0 && std::isfinite(discriminant)) {
                discriminant = std::sqrt(discriminant);
                include_root((-b + discriminant) / (2.0 * a));
                include_root((-b - discriminant) / (2.0 * a));
            }
        }

        bool lineTo(double dx, double dy) {
            if (!have_position) {
                return fail(error, "Type2 path draws before the first moveto");
            }
            const double old_x = x;
            const double old_y = y;
            x += dx;
            y += dy;
            const double mapped_old_x = old_x + offset_x;
            const double mapped_old_y = old_y + offset_y;
            const double mapped_x = x + offset_x;
            const double mapped_y = y + offset_y;
            if (!validCoordinate(x) || !validCoordinate(y) ||
                !validCoordinate(mapped_old_x) || !validCoordinate(mapped_old_y) ||
                !validCoordinate(mapped_x) || !validCoordinate(mapped_y) ||
                !countPathCommand()) {
                return fail(error, "invalid Type2 lineto coordinate or path limit");
            }
            includePoint(mapped_old_x, mapped_old_y);
            includePoint(mapped_x, mapped_y);
            contour_open = true;
            return sink.lineTo(static_cast<float>(mapped_x), static_cast<float>(mapped_y)) ||
                   fail(error, "outline sink rejected lineTo");
        }

        bool cubicTo(double c1x, double c1y, double c2x, double c2y,
                     double end_x, double end_y) {
            if (!have_position) {
                return fail(error, "Type2 path draws before the first moveto");
            }
            const double mapped_x = x + offset_x;
            const double mapped_y = y + offset_y;
            const double mapped_c1x = c1x + offset_x;
            const double mapped_c1y = c1y + offset_y;
            const double mapped_c2x = c2x + offset_x;
            const double mapped_c2y = c2y + offset_y;
            const double mapped_end_x = end_x + offset_x;
            const double mapped_end_y = end_y + offset_y;
            if (!validCoordinate(c1x) || !validCoordinate(c1y) ||
                !validCoordinate(c2x) || !validCoordinate(c2y) ||
                !validCoordinate(end_x) || !validCoordinate(end_y) ||
                !validCoordinate(mapped_x) || !validCoordinate(mapped_y) ||
                !validCoordinate(mapped_c1x) || !validCoordinate(mapped_c1y) ||
                !validCoordinate(mapped_c2x) || !validCoordinate(mapped_c2y) ||
                !validCoordinate(mapped_end_x) || !validCoordinate(mapped_end_y) ||
                !countPathCommand()) {
                return fail(error, "invalid Type2 curve coordinate or path limit");
            }
            includePoint(mapped_x, mapped_y);
            includeCubicAxis(mapped_x, mapped_c1x, mapped_c2x, mapped_end_x, true);
            includeCubicAxis(mapped_y, mapped_c1y, mapped_c2y, mapped_end_y, false);
            const bool accepted = sink.cubicTo(
                static_cast<float>(mapped_c1x), static_cast<float>(mapped_c1y),
                static_cast<float>(mapped_c2x), static_cast<float>(mapped_c2y),
                static_cast<float>(mapped_end_x), static_cast<float>(mapped_end_y));
            x = end_x;
            y = end_y;
            contour_open = true;
            return accepted || fail(error, "outline sink rejected cubicTo");
        }

        bool relativeCubic(double dx1, double dy1, double dx2, double dy2,
                          double dx3, double dy3) {
            const double c1x = x + dx1;
            const double c1y = y + dy1;
            const double c2x = c1x + dx2;
            const double c2y = c1y + dy2;
            return cubicTo(c1x, c1y, c2x, c2y, c2x + dx3, c2y + dy3);
        }

        bool finish(GlyphOutline& out) {
            if (!width_done) {
                width = private_info.default_width;
            }
            if (!closeContour() || !validCoordinate(width)) {
                return fail(error, "invalid Type2 final path or width");
            }
            GlyphOutline result;
            result.advance = static_cast<float>(width);
            if (has_bounds) {
                if (!validCoordinate(min_x) || !validCoordinate(min_y) ||
                    !validCoordinate(max_x) || !validCoordinate(max_y)) {
                    return fail(error, "Type2 glyph bounds exceed float range");
                }
                result.left = static_cast<float>(min_x);
                result.bottom = static_cast<float>(min_y);
                result.right = static_cast<float>(max_x);
                result.top = static_cast<float>(max_y);
                result.has_bounds = true;
            }
            out = result;
            return true;
        }

        enum class RunResult { returned, ended };

        bool callSubroutine(const Index& index, int depth, RunResult& result) {
            if (!stackCount(1)) {
                return false;
            }
            int32_t raw_index = 0;
            if (!integerOperand(stack.back(), raw_index)) {
                return false;
            }
            stack.pop_back();
            const int64_t biased = static_cast<int64_t>(raw_index) + subroutineBias(index.count);
            if (biased < 0 || biased >= index.count) {
                return fail(error, "Type2 subroutine index is out of range");
            }
            if (depth >= k_max_subroutine_depth) {
                return fail(error, "Type2 subroutine recursion limit exceeded");
            }
            const Object selected = indexObject(font.data, index, static_cast<uint32_t>(biased));
            return execute(selected, depth + 1, result);
        }

        bool arithmetic(int op) {
            if (op == 20) {
                if (!stackCount(2)) {
                    return false;
                }
                int32_t index = 0;
                const double value = stack[stack.size() - 2];
                if (!integerOperand(stack.back(), index) || index < 0 || index >= 32) {
                    return fail(error, "invalid Type2 transient array index");
                }
                transient_array[index] = value;
                stack.resize(stack.size() - 2);
                return true;
            }
            if (op == 21) {
                if (!stackCount(1)) {
                    return false;
                }
                int32_t index = 0;
                if (!integerOperand(stack.back(), index) || index < 0 || index >= 32) {
                    return fail(error, "invalid Type2 transient array index");
                }
                stack.back() = transient_array[index];
                return true;
            }
            if (op == 23) {
                random_state ^= random_state << 13;
                random_state ^= random_state >> 17;
                random_state ^= random_state << 5;
                return push((static_cast<double>(random_state) + 1.0) / 4294967297.0);
            }
            if (op == 18) {
                if (!stackCount(1)) {
                    return false;
                }
                stack.pop_back();
                return true;
            }
            if (op == 27) {
                if (!stackCount(1)) {
                    return false;
                }
                return push(stack.back());
            }
            if (op == 28) {
                if (!stackCount(2)) {
                    return false;
                }
                std::swap(stack[stack.size() - 1], stack[stack.size() - 2]);
                return true;
            }
            if (op == 29) {
                if (!stackCount(1)) {
                    return false;
                }
                int32_t index = 0;
                if (!integerOperand(stack.back(), index)) {
                    return false;
                }
                stack.pop_back();
                if (index < 0 || static_cast<size_t>(index) >= stack.size()) {
                    return fail(error, "Type2 index operand is out of range");
                }
                return push(stack[stack.size() - 1 - static_cast<size_t>(index)]);
            }
            if (op == 30) {
                if (!stackCount(2)) {
                    return false;
                }
                int32_t count = 0;
                int32_t shift = 0;
                if (!integerOperand(stack[stack.size() - 2], count) ||
                    !integerOperand(stack.back(), shift)) {
                    return false;
                }
                stack.resize(stack.size() - 2);
                if (count < 0 || static_cast<size_t>(count) > stack.size()) {
                    return fail(error, "Type2 roll count is out of range");
                }
                if (count > 1) {
                    const size_t n = static_cast<size_t>(count);
                    int64_t normalized = static_cast<int64_t>(shift) % static_cast<int64_t>(n);
                    if (normalized < 0) {
                        normalized += n;
                    }
                    const size_t amount = static_cast<size_t>(normalized);
                    std::rotate(stack.end() - static_cast<std::ptrdiff_t>(n),
                                stack.end() - static_cast<std::ptrdiff_t>(amount),
                                stack.end());
                }
                return true;
            }
            if (op == 22) {
                if (!stackCount(4)) {
                    return false;
                }
                const double v1 = stack[stack.size() - 4];
                const double v2 = stack[stack.size() - 3];
                const double s1 = stack[stack.size() - 2];
                const double s2 = stack.back();
                stack.resize(stack.size() - 4);
                return push(v1 <= v2 ? s1 : s2);
            }
            if (op == 3 || op == 4 || op == 10 || op == 11 || op == 12 ||
                op == 15 || op == 24) {
                if (!stackCount(2)) {
                    return false;
                }
                const double a = stack[stack.size() - 2];
                const double b = stack.back();
                stack.resize(stack.size() - 2);
                switch (op) {
                case 3:
                    return push(a != 0.0 && b != 0.0 ? 1.0 : 0.0);
                case 4:
                    return push(a != 0.0 || b != 0.0 ? 1.0 : 0.0);
                case 10:
                    return push(a + b);
                case 11:
                    return push(a - b);
                case 12:
                    if (b == 0.0) {
                        return fail(error, "Type2 division by zero");
                    }
                    return push(a / b);
                case 15:
                    return push(a == b ? 1.0 : 0.0);
                default:
                    return push(a * b);
                }
            }
            if (!stackCount(1)) {
                return false;
            }
            const double value = stack.back();
            switch (op) {
            case 5:
                stack.back() = value == 0.0 ? 1.0 : 0.0;
                return true;
            case 9:
                stack.back() = std::abs(value);
                return true;
            case 14:
                stack.back() = -value;
                return true;
            case 26:
                if (value < 0.0) {
                    return fail(error, "Type2 square root of a negative number");
                }
                stack.back() = std::sqrt(value);
                return true;
            default:
                return fail(error, "unsupported Type2 escaped arithmetic operator");
            }
        }

        bool escapedOperator(int op, int depth, RunResult& result) {
            if (op == 0) {
                return stack.empty() || fail(error, "Type2 dotsection takes no operands");
            }
            if (op >= 3 && op <= 30 && op != 6 && op != 7 && op != 8 && op != 13 &&
                op != 16 && op != 17 && op != 19 && op != 25) {
                return arithmetic(op);
            }
            if (op == 34) {
                if (stack.size() != 7) {
                    return fail(error, "Type2 hflex requires 7 operands");
                }
                const double dy2 = stack[2];
                bool ok = relativeCubic(stack[0], 0.0, stack[1], dy2, stack[3], 0.0);
                ok = ok && relativeCubic(stack[4], 0.0, stack[5], -dy2, stack[6], 0.0);
                stack.clear();
                return ok;
            }
            if (op == 35) {
                if (stack.size() != 13) {
                    return fail(error, "Type2 flex requires 13 operands");
                }
                bool ok = relativeCubic(stack[0], stack[1], stack[2], stack[3],
                                        stack[4], stack[5]);
                ok = ok && relativeCubic(stack[6], stack[7], stack[8], stack[9],
                                         stack[10], stack[11]);
                stack.clear();
                return ok;
            }
            if (op == 36) {
                if (stack.size() != 9) {
                    return fail(error, "Type2 hflex1 requires 9 operands");
                }
                const double start_y = y;
                bool ok = relativeCubic(stack[0], stack[1], stack[2], stack[3],
                                        stack[4], 0.0);
                ok = ok && relativeCubic(stack[5], 0.0, stack[6], stack[7],
                                         stack[8], start_y - (y + stack[7]));
                stack.clear();
                return ok;
            }
            if (op == 37) {
                if (stack.size() != 11) {
                    return fail(error, "Type2 flex1 requires 11 operands");
                }
                const double sum_x = stack[0] + stack[2] + stack[4] + stack[6] + stack[8];
                const double sum_y = stack[1] + stack[3] + stack[5] + stack[7] + stack[9];
                double dx6 = 0.0;
                double dy6 = 0.0;
                if (std::abs(sum_x) > std::abs(sum_y)) {
                    dx6 = stack[10];
                    dy6 = -sum_y;
                } else {
                    dx6 = -sum_x;
                    dy6 = stack[10];
                }
                bool ok = relativeCubic(stack[0], stack[1], stack[2], stack[3],
                                        stack[4], stack[5]);
                ok = ok && relativeCubic(stack[6], stack[7], stack[8], stack[9], dx6, dy6);
                stack.clear();
                return ok;
            }
            (void)depth;
            (void)result;
            return fail(error, "unsupported or reserved Type2 escaped operator");
        }

        bool pathOperator(int op) {
            if (op == 21 || op == 22 || op == 4) {
                const size_t expected = op == 21 ? 2 : 1;
                if (!consumeWidth(expected)) {
                    return false;
                }
                if (stack.size() != expected) {
                    return fail(error, "invalid Type2 moveto argument count");
                }
                bool ok = false;
                if (op == 21) {
                    ok = moveTo(stack[0], stack[1]);
                } else if (op == 22) {
                    ok = moveTo(stack[0], 0.0);
                } else {
                    ok = moveTo(0.0, stack[0]);
                }
                stack.clear();
                return ok;
            }
            if (op == 5) {
                if (stack.empty() || (stack.size() & 1U) != 0) {
                    return fail(error, "Type2 rlineto requires coordinate pairs");
                }
                for (size_t i = 0; i < stack.size(); i += 2) {
                    if (!lineTo(stack[i], stack[i + 1])) {
                        return false;
                    }
                }
                stack.clear();
                return true;
            }
            if (op == 6 || op == 7) {
                if (stack.empty()) {
                    return fail(error, "Type2 hlineto/vlineto requires coordinates");
                }
                bool horizontal = op == 6;
                for (double value : stack) {
                    if (!(horizontal ? lineTo(value, 0.0) : lineTo(0.0, value))) {
                        return false;
                    }
                    horizontal = !horizontal;
                }
                stack.clear();
                return true;
            }
            if (op == 8) {
                if (stack.empty() || stack.size() % 6 != 0) {
                    return fail(error, "Type2 rrcurveto requires groups of 6 operands");
                }
                for (size_t i = 0; i < stack.size(); i += 6) {
                    if (!relativeCubic(stack[i], stack[i + 1], stack[i + 2],
                                       stack[i + 3], stack[i + 4], stack[i + 5])) {
                        return false;
                    }
                }
                stack.clear();
                return true;
            }
            if (op == 24 || op == 25) {
                const size_t tail = op == 24 ? 2 : 6;
                const size_t group = op == 24 ? 6 : 2;
                if (stack.size() < tail + group || (stack.size() - tail) % group != 0) {
                    return fail(error, "invalid Type2 mixed line/curve argument count");
                }
                const size_t split = stack.size() - tail;
                for (size_t i = 0; i < split; i += group) {
                    const bool ok = op == 24
                        ? relativeCubic(stack[i], stack[i + 1], stack[i + 2],
                                        stack[i + 3], stack[i + 4], stack[i + 5])
                        : lineTo(stack[i], stack[i + 1]);
                    if (!ok) {
                        return false;
                    }
                }
                bool ok = false;
                if (op == 24) {
                    ok = lineTo(stack[split], stack[split + 1]);
                } else {
                    ok = relativeCubic(stack[split], stack[split + 1], stack[split + 2],
                                       stack[split + 3], stack[split + 4], stack[split + 5]);
                }
                stack.clear();
                return ok;
            }
            if (op == 26 || op == 27) {
                if (stack.size() < 4 || (stack.size() % 4 != 0 && stack.size() % 4 != 1)) {
                    return fail(error, "invalid Type2 hhcurveto/vvcurveto argument count");
                }
                size_t pos = 0;
                double initial = 0.0;
                if ((stack.size() & 1U) != 0) {
                    initial = stack[pos++];
                }
                bool first = true;
                while (pos < stack.size()) {
                    double dx1 = 0.0;
                    double dy1 = 0.0;
                    if (op == 26) {
                        dy1 = stack[pos];
                        if (first) {
                            dx1 = initial;
                        }
                    } else {
                        dx1 = stack[pos];
                        if (first) {
                            dy1 = initial;
                        }
                    }
                    if (!relativeCubic(dx1, dy1, stack[pos + 1], stack[pos + 2],
                                       op == 26 ? 0.0 : stack[pos + 3],
                                       op == 26 ? stack[pos + 3] : 0.0)) {
                        return false;
                    }
                    first = false;
                    pos += 4;
                }
                stack.clear();
                return true;
            }
            if (op == 30 || op == 31) {
                if (stack.size() < 4 || (stack.size() % 4 != 0 && stack.size() % 4 != 1)) {
                    return fail(error, "invalid Type2 hvcurveto/vhcurveto argument count");
                }
                bool horizontal = op == 31;
                size_t pos = 0;
                while (pos + 4 <= stack.size()) {
                    const bool last = pos + 4 == stack.size() || pos + 5 == stack.size();
                    const double extra = last && pos + 5 == stack.size() ? stack[pos + 4] : 0.0;
                    const double c1x = horizontal ? x + stack[pos] : x;
                    const double c1y = horizontal ? y : y + stack[pos];
                    const double c2x = c1x + stack[pos + 1];
                    const double c2y = c1y + stack[pos + 2];
                    const double end_x = horizontal ? c2x + extra : c2x + stack[pos + 3];
                    const double end_y = horizontal ? c2y + stack[pos + 3] : c2y + extra;
                    if (!cubicTo(c1x, c1y, c2x, c2y, end_x, end_y)) {
                        return false;
                    }
                    horizontal = !horizontal;
                    pos += 4;
                }
                stack.clear();
                return true;
            }
            return fail(error, "unknown Type2 path operator");
        }

        bool endChar(RunResult& result) {
            if (!width_done) {
                if (stack.size() == 1 || stack.size() == 5) {
                    width = private_info.nominal_width + stack.front();
                    stack.erase(stack.begin());
                } else {
                    width = private_info.default_width;
                }
                width_done = true;
            }
            if (!validCoordinate(width)) {
                return fail(error, "invalid Type2 glyph width");
            }
            if (!stack.empty()) {
                if (stack.size() != 4) {
                    return fail(error, "invalid Type2 endchar argument count");
                }
                if (font.cid_keyed) {
                    return fail(error, "Type2 seac endchar is invalid in CID-keyed CFF");
                }
                if (have_position || has_bounds) {
                    return fail(error, "Type2 seac endchar cannot follow a path");
                }
                if (!validCoordinate(stack[0]) || !validCoordinate(stack[1])) {
                    return fail(error, "invalid Type2 seac accent offset");
                }
                int32_t base_code = 0;
                int32_t accent_code = 0;
                if (!integerOperand(stack[2], base_code) ||
                    !integerOperand(stack[3], accent_code) ||
                    base_code < 0 || base_code > 255 ||
                    accent_code < 0 || accent_code > 255) {
                    return fail(error, "invalid Type2 seac character code");
                }
                const uint16_t base = font.glyphForEncoding(base_code);
                const uint16_t accent = font.glyphForEncoding(accent_code);
                if (base == k_invalid_glyph || accent == k_invalid_glyph) {
                    return fail(error, "Type2 seac character has no glyph");
                }
                seac_x = stack[0];
                seac_y = stack[1];
                seac_base = base;
                seac_accent = accent;
                has_seac = true;
                stack.clear();
            }
            result = RunResult::ended;
            return true;
        }

        bool execute(Object object, int depth, RunResult& result) {
            if (object.offset > font.data.size() ||
                object.size > font.data.size() - object.offset) {
                return fail(error, "Type2 charstring is out of bounds");
            }
            size_t pos = object.offset;
            const size_t end = object.offset + object.size;
            while (pos < end) {
                if (!countInstruction()) {
                    return false;
                }
                const uint8_t byte = font.data[pos++];
                if (byte >= 32 || byte == 28 || byte == 255) {
                    double value = 0.0;
                    if (byte >= 32 && byte <= 246) {
                        value = static_cast<double>(static_cast<int>(byte) - 139);
                    } else if (byte >= 247 && byte <= 250) {
                        if (pos >= end) {
                            return fail(error, "truncated Type2 integer");
                        }
                        value = static_cast<double>((static_cast<int>(byte) - 247) * 256 +
                                                    font.data[pos++] + 108);
                    } else if (byte >= 251 && byte <= 254) {
                        if (pos >= end) {
                            return fail(error, "truncated Type2 integer");
                        }
                        value = static_cast<double>(-(static_cast<int>(byte) - 251) * 256 -
                                                    font.data[pos++] - 108);
                    } else if (byte == 28) {
                        if (end - pos < 2) {
                            return fail(error, "truncated Type2 int16");
                        }
                        const int16_t number = static_cast<int16_t>(
                            (static_cast<uint16_t>(font.data[pos]) << 8) | font.data[pos + 1]);
                        pos += 2;
                        value = number;
                    } else {
                        if (end - pos < 4) {
                            return fail(error, "truncated Type2 fixed number");
                        }
                        const uint32_t bits = (static_cast<uint32_t>(font.data[pos]) << 24) |
                                              (static_cast<uint32_t>(font.data[pos + 1]) << 16) |
                                              (static_cast<uint32_t>(font.data[pos + 2]) << 8) |
                                              font.data[pos + 3];
                        pos += 4;
                        value = static_cast<double>(static_cast<int32_t>(bits)) / 65536.0;
                    }
                    if (!push(value)) {
                        return false;
                    }
                    continue;
                }
                if (byte == 12) {
                    if (pos >= end || !countInstruction()) {
                        return fail(error, "truncated Type2 escaped operator");
                    }
                    const int escaped = font.data[pos++];
                    if (!escapedOperator(escaped, depth, result)) {
                        return false;
                    }
                    continue;
                }
                if (byte == 1 || byte == 3 || byte == 18 || byte == 23) {
                    if (!stems()) {
                        return false;
                    }
                    continue;
                }
                if (byte == 19 || byte == 20) {
                    if ((!stack.empty() || !width_done) && !stems()) {
                        return false;
                    }
                    const size_t mask_bytes = (static_cast<size_t>(stem_count) + 7) / 8;
                    if (pos > end || mask_bytes > end - pos) {
                        return fail(error, "truncated Type2 hint mask");
                    }
                    pos += mask_bytes;
                    if (mask_bytes > k_max_instructions - budget.instructions) {
                        return fail(error, "Type2 instruction limit exceeded");
                    }
                    budget.instructions += mask_bytes;
                    continue;
                }
                if (byte == 10 || byte == 29) {
                    const Index& subrs = byte == 10 ? private_info.subroutines
                                                    : font.global_subroutines;
                    if (byte == 10 && !private_info.has_subroutines) {
                        return fail(error, "Type2 local subroutine call without Subrs");
                    }
                    RunResult subroutine_result = RunResult::returned;
                    if (!callSubroutine(subrs, depth, subroutine_result)) {
                        return false;
                    }
                    if (subroutine_result == RunResult::ended) {
                        result = RunResult::ended;
                        return true;
                    }
                    continue;
                }
                if (byte == 11) {
                    if (depth == 0) {
                        return fail(error, "Type2 return outside a subroutine");
                    }
                    result = RunResult::returned;
                    return true;
                }
                if (byte == 14) {
                    return endChar(result);
                }
                if (byte == 4 || byte == 5 || byte == 6 || byte == 7 || byte == 8 ||
                    byte == 21 || byte == 22 || byte == 24 || byte == 25 ||
                    byte == 26 || byte == 27 || byte == 30 || byte == 31) {
                    if (!pathOperator(byte)) {
                        return false;
                    }
                    continue;
                }
                return fail(error, "reserved or unsupported Type2 operator");
            }
            if (depth != 0) {
                result = RunResult::returned;
                return true;
            }
            return endChar(result);
        }

        bool run(uint16_t glyph_index, GlyphOutline& out) {
            const Object object = indexObject(font.data, font.charstrings, glyph_index);
            RunResult result = RunResult::returned;
            return execute(object, 0, result) && result == RunResult::ended && finish(out);
        }
    };

    bool interpretGlyph(uint16_t glyph_index, OutlineSink& sink, GlyphOutline& out,
                        std::string& error, ExecutionBudget& budget, int seac_depth,
                        double offset_x, double offset_y) const {
        Interpreter interpreter(*this, privateForGlyph(glyph_index), sink, error,
                                budget, offset_x, offset_y);
        GlyphOutline own;
        if (!interpreter.run(glyph_index, own)) {
            return false;
        }
        if (!interpreter.has_seac) {
            out = own;
            return true;
        }
        if (seac_depth >= k_max_seac_depth) {
            return fail(error, "Type2 seac recursion limit exceeded");
        }
        const double accent_offset_x = offset_x + interpreter.seac_x;
        const double accent_offset_y = offset_y + interpreter.seac_y;
        if (!interpreter.validCoordinate(accent_offset_x) ||
            !interpreter.validCoordinate(accent_offset_y)) {
            return fail(error, "invalid Type2 seac accumulated accent offset");
        }
        GlyphOutline base;
        GlyphOutline accent;
        if (!interpretGlyph(interpreter.seac_base, sink, base, error, budget,
                            seac_depth + 1, offset_x, offset_y) ||
            !interpretGlyph(interpreter.seac_accent, sink, accent, error, budget,
                            seac_depth + 1, accent_offset_x, accent_offset_y)) {
            return false;
        }
        auto mergeBounds = [](GlyphOutline& target, const GlyphOutline& source) {
            if (!source.has_bounds) {
                return;
            }
            if (!target.has_bounds) {
                target.left = source.left;
                target.bottom = source.bottom;
                target.right = source.right;
                target.top = source.top;
                target.has_bounds = true;
                return;
            }
            target.left = std::min(target.left, source.left);
            target.bottom = std::min(target.bottom, source.bottom);
            target.right = std::max(target.right, source.right);
            target.top = std::max(target.top, source.top);
        };
        mergeBounds(own, base);
        mergeBounds(own, accent);
        out = own;
        return true;
    }

    bool glyphOutline(uint16_t glyph_index, OutlineSink& sink, GlyphOutline& out,
                      std::string& error) const {
        if (glyph_index >= charstrings.count) {
            return fail(error, "CFF glyph index is out of range");
        }
        ExecutionBudget budget;
        return interpretGlyph(glyph_index, sink, out, error, budget, 0, 0.0, 0.0);
    }
};

CffFont::CffFont() noexcept = default;
CffFont::~CffFont() = default;
CffFont::CffFont(CffFont&&) noexcept = default;
CffFont& CffFont::operator=(CffFont&&) noexcept = default;

bool CffFont::parse(const uint8_t* data, size_t size, std::string& error) {
    error.clear();
    impl_.reset();
    try {
        if (size != 0 && data == nullptr) {
            return fail(error, "null CFF data pointer");
        }
        std::unique_ptr<Impl> candidate(new Impl());
        if (size != 0) {
            candidate->data.assign(data, data + size);
        }
        if (!candidate->parse(error)) {
            return false;
        }
        impl_ = std::move(candidate);
        return true;
    } catch (const std::bad_alloc&) {
        return fail(error, "not enough memory to parse CFF");
    } catch (const std::exception&) {
        return fail(error, "failed to parse CFF data");
    } catch (...) {
        return fail(error, "failed to parse CFF data");
    }
}

uint16_t CffFont::glyphCount() const {
    return impl_ ? static_cast<uint16_t>(impl_->charstrings.count) : 0;
}

bool CffFont::glyphOutline(uint16_t glyph_index, OutlineSink& sink, GlyphOutline& out,
                           std::string& error) const {
    error.clear();
    try {
        if (!impl_) {
            return fail(error, "CFF font has not been parsed");
        }
        return impl_->glyphOutline(glyph_index, sink, out, error);
    } catch (const std::bad_alloc&) {
        return fail(error, "not enough memory to interpret CFF glyph");
    } catch (const std::exception&) {
        return fail(error, "failed to interpret CFF glyph");
    } catch (...) {
        return fail(error, "failed to interpret CFF glyph");
    }
}

} // namespace nector_font_detail
