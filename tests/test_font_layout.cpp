#include "nector.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(float first, float second) {
    return std::fabs(first - second) < 0.001F;
}

uint16_t read16(const uint8_t* data) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

uint32_t read32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

void write32(std::vector<uint8_t>& data, size_t offset, uint32_t value) {
    data[offset] = static_cast<uint8_t>(value >> 24);
    data[offset + 1] = static_cast<uint8_t>(value >> 16);
    data[offset + 2] = static_cast<uint8_t>(value >> 8);
    data[offset + 3] = static_cast<uint8_t>(value);
}

void append16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value >> 8));
    data.push_back(static_cast<uint8_t>(value));
}

void append32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(static_cast<uint8_t>(value >> 24));
    data.push_back(static_cast<uint8_t>(value >> 16));
    data.push_back(static_cast<uint8_t>(value >> 8));
    data.push_back(static_cast<uint8_t>(value));
}

std::vector<uint8_t> readBytes(const std::string& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    require(stream.good(), "cannot open fixture font");
    const std::streamoff size = stream.tellg();
    require(size > 12, "fixture font is empty");
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), size);
    require(stream.good(), "cannot read fixture font");
    return bytes;
}

std::vector<uint8_t> addKerning(std::vector<uint8_t> bytes, uint16_t left, uint16_t right,
                                uint16_t& units_per_em) {
    const uint16_t count = read16(bytes.data() + 4);
    const size_t directory_size = 12 + static_cast<size_t>(count) * 16;
    require(directory_size < bytes.size(), "fixture font directory is invalid");
    units_per_em = 2048;
    size_t kern_record = directory_size;
    for (uint16_t index = 0; index < count; ++index) {
        const size_t record = 12 + static_cast<size_t>(index) * 16;
        const uint32_t tag = read32(bytes.data() + record);
        if (tag == 0x68656164u) {
            const uint32_t offset = read32(bytes.data() + record + 8);
            require(static_cast<size_t>(offset) + 20 <= bytes.size(), "fixture head table is invalid");
            units_per_em = read16(bytes.data() + offset + 18);
        }
        if (tag == 0x6b65726eu) kern_record = record;
    }
    require(kern_record != directory_size, "fixture has no kern table to replace");

    const uint32_t kern_offset = static_cast<uint32_t>(bytes.size());
    write32(bytes, kern_record + 4, 0);
    write32(bytes, kern_record + 8, kern_offset);
    write32(bytes, kern_record + 12, 44);
    append16(bytes, 0);
    append16(bytes, 2);
    for (int subtable = 0; subtable < 2; ++subtable) {
        append16(bytes, 0);
        append16(bytes, 20);
        append16(bytes, 1);
        append16(bytes, 1);
        append16(bytes, 6);
        append16(bytes, 0);
        append16(bytes, 0);
        append32(bytes, (static_cast<uint32_t>(left) << 16) | right);
        append16(bytes, subtable == 0 ? static_cast<uint16_t>(-80) : static_cast<uint16_t>(-40));
    }
    return bytes;
}

std::string utf8(uint32_t codepoint) {
    std::string result;
    if (codepoint < 0x80) {
        result.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    return result;
}

void sameRun(const nector::GlyphRun& first, const nector::GlyphRun& second) {
    require(first.items.size() == second.items.size() && near(first.width, second.width) &&
            near(first.height, second.height), "repeated layout metadata differs");
    for (size_t index = 0; index < first.items.size(); ++index) {
        const nector::GlyphRunItem& a = first.items[index];
        const nector::GlyphRunItem& b = second.items[index];
        require(a.glyph_id == b.glyph_id && a.codepoint == b.codepoint && a.cluster == b.cluster &&
                a.line_index == b.line_index && near(a.advance, b.advance) && near(a.pen_x, b.pen_x) &&
                near(a.baseline_y, b.baseline_y) && a.has_tight_bounds == b.has_tight_bounds,
                "repeated layout glyph items differ");
        if (a.has_tight_bounds != 0) {
            require(near(a.tight_bounds.x, b.tight_bounds.x) && near(a.tight_bounds.y, b.tight_bounds.y) &&
                    near(a.tight_bounds.width, b.tight_bounds.width) &&
                    near(a.tight_bounds.height, b.tight_bounds.height), "repeated glyph bounds differ");
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        const std::string path = argc > 1 ? argv[1] : "third_party/fonts/deja_vu_sans.ttf";
        nector::Font font(path);
        const nector::FontMetrics metrics = font.metrics(64);
        const nector::GlyphRun av = font.layoutText(64, "AV");
        const nector::TextMetrics measured = font.measureText(64, "AV");
        require(av.items.size() == 2 && measured.glyph_count == 2 && near(av.width, measured.width) &&
                near(av.height, measured.height) && near(av.height, metrics.line_height),
                "layout and measurement disagree");
        require(av.items[0].cluster == 0 && av.items[1].cluster == 1 &&
                av.items[0].pen_x == 0 && av.items[1].pen_x > 0 &&
                av.items[1].pen_x <= av.items[0].advance,
                "AV byte clusters or kerned advances are incorrect");
        require(av.items[0].has_tight_bounds != 0 && av.items[0].tight_bounds.width > 0 &&
                av.items[0].tight_bounds.height > 0, "visible glyph has no tight bounds");

        uint16_t units_per_em = 0;
        const std::vector<uint8_t> kerned_bytes = addKerning(
            readBytes(path), static_cast<uint16_t>(av.items[0].glyph_id),
            static_cast<uint16_t>(av.items[1].glyph_id), units_per_em);
        nector::Font kerned = nector::Font::fromMemory(kerned_bytes);
        const nector::GlyphRun kerned_av = kerned.layoutText(64, "AV");
        const float expected_kerned_pen = av.items[0].advance - 120.0F * 64.0F / units_per_em;
        require(kerned_av.items.size() == 2 && near(kerned_av.items[1].pen_x, expected_kerned_pen),
                "multiple kern subtables were not accumulated");

        const std::string multiline = std::string("A\r\n") + "\xc3\xa9" + "\nA";
        const nector::GlyphRun lines = font.layoutText(64, multiline);
        const nector::TextMetrics lines_measured = font.measureText(64, multiline);
        require(lines.items.size() == 3 && lines_measured.glyph_count == 3 &&
                near(lines.width, lines_measured.width) && near(lines.height, lines_measured.height) &&
                near(lines.height, metrics.line_height * 3), "multiline layout dimensions are wrong");
        require(lines.items[0].cluster == 0 && lines.items[1].cluster == 3 &&
                lines.items[2].cluster == 6 && lines.items[1].codepoint == 0xe9,
                "UTF-8 byte clusters or CRLF handling are wrong");
        require(lines.items[0].line_index == 0 && lines.items[1].line_index == 1 &&
                lines.items[2].line_index == 2 && near(lines.items[0].baseline_y, 0) &&
                near(lines.items[1].baseline_y, metrics.line_height) &&
                near(lines.items[2].baseline_y, metrics.line_height * 2),
                "line indices or baseline positions are wrong");

        const nector::GlyphRun spaced = font.layoutText(64, "A A");
        require(spaced.items.size() == 3 && spaced.items[0].has_tight_bounds != 0 &&
                spaced.items[1].has_tight_bounds == 0 && spaced.items[2].has_tight_bounds != 0,
                "space glyph bounds are wrong");

        nector_glyph_run native{};
        nector::detail::check(nectorFontLayoutText(font.nativeHandle(), 64, "AV", &native));
        require(native.count == av.items.size() && native.items != nullptr &&
                native.items[1].cluster == av.items[1].cluster &&
                near(native.items[1].pen_x, av.items[1].pen_x), "C and C++ layout APIs disagree");

        for (uint32_t codepoint = 32; codepoint < 332; ++codepoint) {
            font.layoutText(18, utf8(codepoint));
        }
        const nector::GlyphRun fresh = font.layoutText(64, "AV");
        sameRun(av, fresh);
        require(av.items.size() == 2 && av.items[1].codepoint == 'V',
                "C++ GlyphRun did not retain ownership after later layouts");

        std::cout << "test_font_layout: PASS; layout/measure parity, kern, UTF-8 clusters, multiline, bounds and LRU reuse\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_font_layout: FAIL: " << error.what() << '\n';
        return 1;
    }
}
