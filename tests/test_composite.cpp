#include "font.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Table {
    std::array<char, 4> tag{};
    std::vector<std::uint8_t> bytes;
    std::uint32_t offset = 0;
};

void append16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

void append32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

void set16(std::vector<std::uint8_t>& output, std::size_t offset, std::uint16_t value) {
    output[offset] = static_cast<std::uint8_t>(value >> 8);
    output[offset + 1] = static_cast<std::uint8_t>(value);
}

void set32(std::vector<std::uint8_t>& output, std::size_t offset, std::uint32_t value) {
    output[offset] = static_cast<std::uint8_t>(value >> 24);
    output[offset + 1] = static_cast<std::uint8_t>(value >> 16);
    output[offset + 2] = static_cast<std::uint8_t>(value >> 8);
    output[offset + 3] = static_cast<std::uint8_t>(value);
}

std::uint32_t checksum(const std::uint8_t* data, std::size_t size) {
    std::uint32_t sum = 0;
    for (std::size_t offset = 0; offset < size; offset += 4) {
        std::uint32_t value = 0;
        for (std::size_t byte = 0; byte < 4; ++byte) {
            value <<= 8;
            if (offset + byte < size) value |= data[offset + byte];
        }
        sum += value;
    }
    return sum;
}

std::vector<std::uint8_t> simpleGlyph(const std::array<std::int16_t, 8>& coordinates) {
    std::vector<std::uint8_t> glyph(10, 0);
    set16(glyph, 0, 1);
    append16(glyph, 3);
    append16(glyph, 0);
    glyph.insert(glyph.end(), {1, 1, 1, 1});
    for (std::size_t index = 0; index < 4; ++index) append16(glyph, static_cast<std::uint16_t>(coordinates[index * 2]));
    for (std::size_t index = 0; index < 4; ++index) append16(glyph, static_cast<std::uint16_t>(coordinates[index * 2 + 1]));
    while (glyph.size() % 4 != 0) glyph.push_back(0);
    return glyph;
}

std::vector<std::uint8_t> compositeGlyph(std::size_t& point_argument_offset) {
    std::vector<std::uint8_t> glyph(10, 0);
    set16(glyph, 0, 0xffff);
    set16(glyph, 6, 200);
    set16(glyph, 8, 200);
    append16(glyph, 35);
    append16(glyph, 1);
    append16(glyph, 0);
    append16(glyph, 0);
    append16(glyph, 1);
    append16(glyph, 2);
    point_argument_offset = glyph.size();
    append16(glyph, 1);
    append16(glyph, 0);
    while (glyph.size() % 4 != 0) glyph.push_back(0);
    return glyph;
}

std::vector<std::uint8_t> makeFont(std::size_t& point_argument_offset) {
    std::vector<Table> tables(7);
    tables[0].tag = {'c', 'm', 'a', 'p'};
    append16(tables[0].bytes, 0);
    append16(tables[0].bytes, 1);
    append16(tables[0].bytes, 0);
    append16(tables[0].bytes, 0);
    append32(tables[0].bytes, 12);
    append16(tables[0].bytes, 0);
    append16(tables[0].bytes, 262);
    append16(tables[0].bytes, 0);
    tables[0].bytes.resize(274, 0);
    tables[0].bytes[12 + 6 + 65] = 3;

    const std::vector<std::uint8_t> first = simpleGlyph({0, 0, 100, 0, 0, 100, -100, 0});
    const std::vector<std::uint8_t> second = simpleGlyph({100, 100, 100, 0, 0, 100, -200, 0});
    std::size_t composite_argument = 0;
    const std::vector<std::uint8_t> composite = compositeGlyph(composite_argument);
    tables[1].tag = {'g', 'l', 'y', 'f'};
    tables[1].bytes.insert(tables[1].bytes.end(), first.begin(), first.end());
    tables[1].bytes.insert(tables[1].bytes.end(), second.begin(), second.end());
    const std::size_t composite_offset = tables[1].bytes.size();
    tables[1].bytes.insert(tables[1].bytes.end(), composite.begin(), composite.end());

    tables[2].tag = {'h', 'e', 'a', 'd'};
    tables[2].bytes.resize(54, 0);
    set32(tables[2].bytes, 0, 0x00010000);
    set32(tables[2].bytes, 4, 0x00010000);
    set32(tables[2].bytes, 12, 0x5f0f3cf5);
    set16(tables[2].bytes, 18, 1000);
    set16(tables[2].bytes, 36, 0);
    set16(tables[2].bytes, 38, 0);
    set16(tables[2].bytes, 40, 200);
    set16(tables[2].bytes, 42, 200);
    set16(tables[2].bytes, 50, 1);

    tables[3].tag = {'h', 'h', 'e', 'a'};
    tables[3].bytes.resize(36, 0);
    set32(tables[3].bytes, 0, 0x00010000);
    set16(tables[3].bytes, 4, 800);
    set16(tables[3].bytes, 6, static_cast<std::uint16_t>(-200));
    set16(tables[3].bytes, 10, 1000);
    set16(tables[3].bytes, 34, 4);

    tables[4].tag = {'h', 'm', 't', 'x'};
    for (int glyph = 0; glyph < 4; ++glyph) {
        append16(tables[4].bytes, 1000);
        append16(tables[4].bytes, 0);
    }

    tables[5].tag = {'l', 'o', 'c', 'a'};
    append32(tables[5].bytes, 0);
    append32(tables[5].bytes, 0);
    append32(tables[5].bytes, static_cast<std::uint32_t>(first.size()));
    append32(tables[5].bytes, static_cast<std::uint32_t>(first.size() + second.size()));
    append32(tables[5].bytes, static_cast<std::uint32_t>(tables[1].bytes.size()));

    tables[6].tag = {'m', 'a', 'x', 'p'};
    tables[6].bytes.resize(32, 0);
    set32(tables[6].bytes, 0, 0x00010000);
    set16(tables[6].bytes, 4, 4);

    std::sort(tables.begin(), tables.end(), [](const Table& left, const Table& right) { return left.tag < right.tag; });
    const std::uint16_t count = static_cast<std::uint16_t>(tables.size());
    std::vector<std::uint8_t> font(12 + tables.size() * 16, 0);
    set32(font, 0, 0x00010000);
    set16(font, 4, count);
    std::uint16_t search_range = 1;
    std::uint16_t entry_selector = 0;
    while (search_range * 2 <= count) {
        search_range *= 2;
        ++entry_selector;
    }
    search_range *= 16;
    set16(font, 6, search_range);
    set16(font, 8, entry_selector);
    set16(font, 10, static_cast<std::uint16_t>(count * 16 - search_range));
    std::size_t head_index = 0;
    for (std::size_t index = 0; index < tables.size(); ++index) {
        Table& table = tables[index];
        while (font.size() % 4 != 0) font.push_back(0);
        table.offset = static_cast<std::uint32_t>(font.size());
        const std::size_t directory = 12 + index * 16;
        std::copy(table.tag.begin(), table.tag.end(), font.begin() + static_cast<std::ptrdiff_t>(directory));
        set32(font, directory + 4, checksum(table.bytes.data(), table.bytes.size()));
        set32(font, directory + 8, table.offset);
        set32(font, directory + 12, static_cast<std::uint32_t>(table.bytes.size()));
        font.insert(font.end(), table.bytes.begin(), table.bytes.end());
        if (table.tag == std::array<char, 4>{'h', 'e', 'a', 'd'}) head_index = index;
        if (table.tag == std::array<char, 4>{'g', 'l', 'y', 'f'}) {
            point_argument_offset = table.offset + composite_offset + composite_argument;
        }
    }
    const std::size_t head_offset = tables[head_index].offset;
    set32(font, head_offset + 8, 0xb1b0afbaU - checksum(font.data(), font.size()));
    return font;
}

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        std::size_t point_argument_offset = 0;
        std::vector<std::uint8_t> data = makeFont(point_argument_offset);
        require(point_argument_offset != 0, "test font did not contain a point-matched component");
        nector_font_handle font = nullptr;
        check(nectorLoadFontMemory(data.data(), data.size(), 0, &font));
        std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> font_guard(font, nectorDestroyFont);
        nector_path_handle path = nullptr;
        check(nectorCreatePath(&path));
        std::unique_ptr<nector_path, decltype(&nectorDestroyPath)> path_guard(path, nectorDestroyPath);
        check(nectorFontGetGlyphPath(font, 'A', 1000, path));

        nector_surface_handle surface = nullptr;
        check(nectorCreateSurface(220, 180, &surface));
        std::unique_ptr<nector_surface, decltype(&nectorDestroySurface)> surface_guard(surface, nectorDestroySurface);
        nector_painter_handle painter = nullptr;
        check(nectorCreatePainter(surface, &painter));
        std::unique_ptr<nector_painter, decltype(&nectorDestroyPainter)> painter_guard(painter, nectorDestroyPainter);
        check(nectorFontDrawText(painter, font, 1000, 0, 150, "A", {1, 0, 0, 1}));
        std::vector<std::uint8_t> pixels(220U * 180U * 4U);
        check(nectorSurfaceReadPixels(surface, pixels.data(), pixels.size()));
        const std::size_t matched_pixel = (75U * 220U + 150U) * 4U;
        const std::size_t unmatched_pixel = (25U * 220U + 150U) * 4U;
        require(pixels[matched_pixel + 3] == 255 && pixels[matched_pixel] == 255,
                "point-matched component was not positioned next to the parent");
        require(pixels[unmatched_pixel + 3] == 0, "component retained an incorrect unmatched offset");

        data[point_argument_offset] = 0xff;
        data[point_argument_offset + 1] = 0xff;
        nector_font_handle invalid_font = nullptr;
        check(nectorLoadFontMemory(data.data(), data.size(), 0, &invalid_font));
        std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> invalid_guard(invalid_font, nectorDestroyFont);
        require(nectorFontGetGlyphPath(invalid_font, 'A', 1000, path) != nector_ok,
                "out-of-range composite parent point was accepted");
        std::cout << "test_composite: PASS; point-matched outline rendered; invalid point index rejected\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_composite: FAIL: " << error.what() << '\n';
        return 1;
    }
}
