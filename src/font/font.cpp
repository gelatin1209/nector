#include "font.h"
#include "../core/internal.h"
#include "outline.h"
#include "cff.h"
#include "woff2.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using nector_font_detail::CffFont;
using nector_font_detail::GlyphOutline;
using nector_font_detail::OutlineSink;

constexpr size_t max_font_size = 512u * 1024u * 1024u;
constexpr size_t max_text_size = 16u * 1024u * 1024u;
constexpr uint32_t tag_cmap = 0x636d6170u;
constexpr uint32_t tag_glyf = 0x676c7966u;
constexpr uint32_t tag_head = 0x68656164u;
constexpr uint32_t tag_hhea = 0x68686561u;
constexpr uint32_t tag_hmtx = 0x686d7478u;
constexpr uint32_t tag_loca = 0x6c6f6361u;
constexpr uint32_t tag_maxp = 0x6d617870u;
constexpr uint32_t tag_cff = 0x43464620u;
constexpr uint32_t tag_kern = 0x6b65726eu;

uint16_t read16(const uint8_t* data) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

int16_t readSigned16(const uint8_t* data) {
    return static_cast<int16_t>(read16(data));
}

uint32_t read32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
}

void write16(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value >> 8);
    data[1] = static_cast<uint8_t>(value);
}

void write32(uint8_t* data, uint32_t value) {
    data[0] = static_cast<uint8_t>(value >> 24);
    data[1] = static_cast<uint8_t>(value >> 16);
    data[2] = static_cast<uint8_t>(value >> 8);
    data[3] = static_cast<uint8_t>(value);
}

bool rangeValid(size_t offset, size_t length, size_t size) {
    return offset <= size && length <= size - offset;
}

uint32_t tableChecksum(const uint8_t* data, size_t size) {
    uint32_t sum = 0;
    for (size_t offset = 0; offset < size; offset += 4) {
        uint32_t value = 0;
        for (size_t byte = 0; byte < 4; ++byte) {
            value <<= 8;
            if (offset + byte < size) value |= data[offset + byte];
        }
        sum += value;
    }
    return sum;
}

struct TableData {
    uint32_t tag = 0;
    std::vector<uint8_t> bytes;
};

bool buildSfnt(uint32_t flavor, std::vector<TableData>& tables, std::vector<uint8_t>& output, std::string& error) {
    if (tables.empty() || tables.size() > 4096) {
        error = "invalid sfnt table count";
        return false;
    }
    std::sort(tables.begin(), tables.end(), [](const TableData& a, const TableData& b) { return a.tag < b.tag; });
    for (size_t index = 1; index < tables.size(); ++index) {
        if (tables[index - 1].tag == tables[index].tag) {
            error = "duplicate sfnt table";
            return false;
        }
    }
    const size_t count = tables.size();
    const size_t directory_size = 12 + count * 16;
    size_t total = directory_size;
    for (const auto& table : tables) {
        if (table.bytes.size() > max_font_size || total > max_font_size - ((table.bytes.size() + 3) & ~size_t(3))) {
            error = "reconstructed font is too large";
            return false;
        }
        total += (table.bytes.size() + 3) & ~size_t(3);
    }
    output.assign(total, 0);
    write32(output.data(), flavor);
    write16(output.data() + 4, static_cast<uint16_t>(count));
    uint16_t search_range = 1;
    uint16_t entry_selector = 0;
    while (static_cast<uint32_t>(search_range) * 2 <= count) {
        search_range = static_cast<uint16_t>(search_range * 2);
        ++entry_selector;
    }
    search_range = static_cast<uint16_t>(search_range * 16);
    write16(output.data() + 6, search_range);
    write16(output.data() + 8, entry_selector);
    write16(output.data() + 10, static_cast<uint16_t>(count * 16 - search_range));
    size_t offset = directory_size;
    size_t head_offset = std::numeric_limits<size_t>::max();
    for (size_t index = 0; index < count; ++index) {
        auto& table = tables[index];
        uint8_t* entry = output.data() + 12 + index * 16;
        write32(entry, table.tag);
        write32(entry + 4, tableChecksum(table.bytes.data(), table.bytes.size()));
        write32(entry + 8, static_cast<uint32_t>(offset));
        write32(entry + 12, static_cast<uint32_t>(table.bytes.size()));
        if (!table.bytes.empty()) std::memcpy(output.data() + offset, table.bytes.data(), table.bytes.size());
        if (table.tag == tag_head) head_offset = offset;
        offset += (table.bytes.size() + 3) & ~size_t(3);
    }
    if (head_offset != std::numeric_limits<size_t>::max() && rangeValid(head_offset + 8, 4, output.size())) {
        std::memset(output.data() + head_offset + 8, 0, 4);
        const uint32_t adjustment = 0xb1b0afbau - tableChecksum(output.data(), output.size());
        write32(output.data() + head_offset + 8, adjustment);
        for (size_t index = 0; index < count; ++index) {
            auto& table = tables[index];
            if (table.tag == tag_head) {
                write32(output.data() + 12 + index * 16 + 4, tableChecksum(output.data() + head_offset, table.bytes.size()));
                break;
            }
        }
    }
    return true;
}

bool decodeWoff(const uint8_t* data, size_t size, std::vector<uint8_t>& output, std::string& error) {
    if (size < 44 || read32(data) != 0x774f4646u) {
        error = "truncated WOFF header";
        return false;
    }
    const uint32_t flavor = read32(data + 4);
    const uint32_t declared_size = read32(data + 8);
    const uint16_t count = read16(data + 12);
    const uint32_t sfnt_size = read32(data + 20);
    if (declared_size != size || count == 0 || !rangeValid(44, static_cast<size_t>(count) * 20, size)) {
        error = "invalid WOFF directory";
        return false;
    }
    if (sfnt_size > max_font_size) {
        error = "WOFF output exceeds font size limit";
        return false;
    }
    std::vector<TableData> tables(count);
    for (size_t index = 0; index < count; ++index) {
        const uint8_t* entry = data + 44 + index * 20;
        const uint32_t offset = read32(entry + 4);
        const uint32_t compressed_size = read32(entry + 8);
        const uint32_t original_size = read32(entry + 12);
        if (!rangeValid(offset, compressed_size, size) || original_size > max_font_size || compressed_size > original_size) {
            error = "invalid WOFF table range";
            return false;
        }
        tables[index].tag = read32(entry);
        tables[index].bytes.resize(original_size);
        if (compressed_size == original_size) {
            if (original_size != 0) std::memcpy(tables[index].bytes.data(), data + offset, original_size);
        } else {
            uLongf destination_size = original_size;
            const int result = uncompress(tables[index].bytes.data(), &destination_size, data + offset, compressed_size);
            if (result != Z_OK || destination_size != original_size) {
                error = "invalid compressed WOFF table";
                return false;
            }
        }
    }
    return buildSfnt(flavor, tables, output, error);
}

struct TableRef {
    const uint8_t* data = nullptr;
    size_t size = 0;
    size_t offset = 0;
};

struct FaceTables {
    size_t face_offset = 0;
    std::vector<std::pair<uint32_t, TableRef>> tables;

    const TableRef* find(uint32_t tag) const {
        for (const auto& item : tables) {
            if (item.first == tag) return &item.second;
        }
        return nullptr;
    }
};

bool parseFaceTables(const std::vector<uint8_t>& bytes, int32_t face_index, FaceTables& face, std::string& error) {
    if (bytes.size() < 12) {
        error = "truncated sfnt header";
        return false;
    }
    const uint8_t* data = bytes.data();
    uint32_t face_count = 1;
    size_t directory = 0;
    if (read32(data) == 0x74746366u) {
        if (bytes.size() < 16) {
            error = "truncated sfnt collection";
            return false;
        }
        face_count = read32(data + 8);
        if (face_count == 0 || face_count > 1024 || !rangeValid(12, static_cast<size_t>(face_count) * 4, bytes.size())) {
            error = "invalid sfnt collection directory";
            return false;
        }
        if (face_index < 0 || static_cast<uint32_t>(face_index) >= face_count) {
            error = "font face index is out of range";
            return false;
        }
        directory = read32(data + 12 + static_cast<size_t>(face_index) * 4);
    } else if (face_index != 0) {
        error = "font face index is out of range";
        return false;
    }
    if (!rangeValid(directory, 12, bytes.size())) {
        error = "invalid face offset";
        return false;
    }
    const uint32_t flavor = read32(data + directory);
    if (flavor != 0x00010000u && flavor != 0x4f54544fu && flavor != 0x74727565u) {
        error = "unsupported sfnt flavor";
        return false;
    }
    const uint16_t count = read16(data + directory + 4);
    if (count == 0 || !rangeValid(directory + 12, static_cast<size_t>(count) * 16, bytes.size())) {
        error = "invalid sfnt table directory";
        return false;
    }
    face.face_offset = directory;
    face.tables.clear();
    face.tables.reserve(count);
    for (size_t index = 0; index < count; ++index) {
        const uint8_t* entry = data + directory + 12 + index * 16;
        const uint32_t offset = read32(entry + 8);
        const uint32_t length = read32(entry + 12);
        if (!rangeValid(offset, length, bytes.size())) {
            error = "sfnt table lies outside the font";
            return false;
        }
        const uint32_t tag = read32(entry);
        for (const auto& existing : face.tables) {
            if (existing.first == tag) {
                error = "duplicate sfnt table";
                return false;
            }
        }
        face.tables.push_back({tag, {data + offset, length, offset}});
    }
    return true;
}

bool sfntFaceCount(const uint8_t* data, size_t size, int32_t& count, std::string& error) {
    count = 0;
    if (data == nullptr || size < 4) {
        error = "font buffer is too small";
        return false;
    }
    if (read32(data) == 0x74746366u) {
        if (size < 16) {
            error = "truncated sfnt collection";
            return false;
        }
        const uint32_t value = read32(data + 8);
        if (value == 0 || value > 1024 || value > static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) ||
            !rangeValid(12, static_cast<size_t>(value) * 4, size)) {
            error = "invalid sfnt collection directory";
            return false;
        }
        count = static_cast<int32_t>(value);
        return true;
    }
    std::vector<uint8_t> decoded;
    if (read32(data) == 0x774f4646u) {
        if (!decodeWoff(data, size, decoded, error)) return false;
        data = decoded.data();
        size = decoded.size();
    } else if (read32(data) == 0x774f4632u) {
        if (!nector_font_detail::decodeWoff2(data, size, decoded, error)) return false;
        data = decoded.data();
        size = decoded.size();
    }
    if (size < 12) {
        error = "truncated sfnt header";
        return false;
    }
    const uint32_t flavor = read32(data);
    if (flavor != 0x00010000u && flavor != 0x4f54544fu && flavor != 0x74727565u) {
        error = "unsupported font format";
        return false;
    }
    count = 1;
    return true;
}

class PathSink final : public OutlineSink {
public:
    PathSink(nector_path_handle path, float scale, float offset_x, float offset_y)
        : path_(path), scale_(scale), offset_x_(offset_x), offset_y_(offset_y) {}

    bool moveTo(float x, float y) override { return record(nectorPathMoveTo(path_, mapX(x), mapY(y))); }
    bool lineTo(float x, float y) override { return record(nectorPathLineTo(path_, mapX(x), mapY(y))); }
    bool quadraticTo(float cx, float cy, float x, float y) override {
        return record(nectorPathQuadraticTo(path_, mapX(cx), mapY(cy), mapX(x), mapY(y)));
    }
    bool cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) override {
        return record(nectorPathCubicTo(path_, mapX(c1x), mapY(c1y), mapX(c2x), mapY(c2y), mapX(x), mapY(y)));
    }
    bool close() override { return record(nectorPathClose(path_)); }

private:
    float mapX(float value) const { return offset_x_ + value * scale_; }
    float mapY(float value) const { return offset_y_ - value * scale_; }
    bool record(nector_status value) {
        if (value != nector_ok) status_ = value;
        return value == nector_ok;
    }

    nector_path_handle path_;
    float scale_;
    float offset_x_;
    float offset_y_;
public:
    nector_status status_ = nector_ok;
};

struct GlyphTransform {
    float a = 1;
    float b = 0;
    float c = 0;
    float d = 1;
    float dx = 0;
    float dy = 0;

    float mapX(float x, float y) const { return a * x + c * y + dx; }
    float mapY(float x, float y) const { return b * x + d * y + dy; }
};

class NullSink final : public OutlineSink {
public:
    bool moveTo(float, float) override { return true; }
    bool lineTo(float, float) override { return true; }
    bool quadraticTo(float, float, float, float) override { return true; }
    bool cubicTo(float, float, float, float, float, float) override { return true; }
    bool close() override { return true; }
};

class TransformSink final : public OutlineSink {
public:
    TransformSink(OutlineSink& sink, float a, float b, float c, float d, float dx, float dy)
        : sink_(sink), a_(a), b_(b), c_(c), d_(d), dx_(dx), dy_(dy) {}
    bool moveTo(float x, float y) override { return sink_.moveTo(mapX(x, y), mapY(x, y)); }
    bool lineTo(float x, float y) override { return sink_.lineTo(mapX(x, y), mapY(x, y)); }
    bool quadraticTo(float cx, float cy, float x, float y) override {
        return sink_.quadraticTo(mapX(cx, cy), mapY(cx, cy), mapX(x, y), mapY(x, y));
    }
    bool cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) override {
        return sink_.cubicTo(mapX(c1x, c1y), mapY(c1x, c1y), mapX(c2x, c2y), mapY(c2x, c2y), mapX(x, y), mapY(x, y));
    }
    bool close() override { return sink_.close(); }
private:
    float mapX(float x, float y) const { return a_ * x + c_ * y + dx_; }
    float mapY(float x, float y) const { return b_ * x + d_ * y + dy_; }
    OutlineSink& sink_;
    float a_, b_, c_, d_, dx_, dy_;
};

struct ContourPoint {
    float x = 0;
    float y = 0;
    bool on_curve = false;
};

constexpr size_t max_captured_glyph_points = 2u * 1024u * 1024u;

bool captureGlyphPoints(std::vector<ContourPoint>& output, const std::vector<ContourPoint>& points,
                        const GlyphTransform& transform, std::string& error) {
    if (points.size() > max_captured_glyph_points ||
        output.size() > max_captured_glyph_points - points.size()) {
        error = "composite glyph point limit exceeded";
        return false;
    }
    output.reserve(output.size() + points.size());
    for (const auto& point : points) {
        output.push_back({transform.mapX(point.x, point.y), transform.mapY(point.x, point.y), point.on_curve});
    }
    return true;
}

bool emitTrueTypeContour(const std::vector<ContourPoint>& contour, OutlineSink& sink) {
    if (contour.size() < 2) return true;
    std::vector<ContourPoint> points;
    points.reserve(contour.size() * 2);
    for (size_t index = 0; index < contour.size(); ++index) {
        const auto& point = contour[index];
        const auto& next = contour[(index + 1) % contour.size()];
        points.push_back(point);
        if (!point.on_curve && !next.on_curve) {
            points.push_back({(point.x + next.x) * 0.5f, (point.y + next.y) * 0.5f, true});
        }
    }
    size_t start = 0;
    while (start < points.size() && !points[start].on_curve) ++start;
    if (start == points.size()) return true;
    std::rotate(points.begin(), points.begin() + static_cast<std::ptrdiff_t>(start), points.end());
    points.push_back(points.front());
    if (!sink.moveTo(points[0].x, points[0].y)) return false;
    for (size_t index = 1; index < points.size(); ++index) {
        const auto& point = points[index];
        if (point.on_curve) {
            if (!sink.lineTo(point.x, point.y)) return false;
        } else {
            if (index + 1 >= points.size() || !points[index + 1].on_curve) return false;
            const auto& end = points[index + 1];
            if (!sink.quadraticTo(point.x, point.y, end.x, end.y)) return false;
            ++index;
        }
    }
    return sink.close();
}

} // namespace

struct GlyphCommand {
    enum class Type { move, line, quadratic, cubic, close };
    Type type = Type::move;
    float x = 0;
    float y = 0;
    float c1x = 0;
    float c1y = 0;
    float c2x = 0;
    float c2y = 0;
};

struct CachedGlyph {
    std::vector<GlyphCommand> commands;
    GlyphOutline outline;
};

class RecordingSink final : public OutlineSink {
public:
    explicit RecordingSink(CachedGlyph& glyph) : glyph_(glyph) {}

    bool moveTo(float x, float y) override { return append({GlyphCommand::Type::move, x, y}); }
    bool lineTo(float x, float y) override { return append({GlyphCommand::Type::line, x, y}); }
    bool quadraticTo(float cx, float cy, float x, float y) override {
        return append({GlyphCommand::Type::quadratic, x, y, cx, cy, 0, 0});
    }
    bool cubicTo(float c1x, float c1y, float c2x, float c2y, float x, float y) override {
        return append({GlyphCommand::Type::cubic, x, y, c1x, c1y, c2x, c2y});
    }
    bool close() override { return append({GlyphCommand::Type::close, 0, 0}); }

private:
    bool append(GlyphCommand command) {
        if (!std::isfinite(command.x) || !std::isfinite(command.y) ||
            !std::isfinite(command.c1x) || !std::isfinite(command.c1y) ||
            !std::isfinite(command.c2x) || !std::isfinite(command.c2y) || glyph_.commands.size() >= 1000000) {
            return false;
        }
        if (command.type != GlyphCommand::Type::close) {
            include(command.x, command.y);
            if (command.type == GlyphCommand::Type::quadratic || command.type == GlyphCommand::Type::cubic) {
                include(command.c1x, command.c1y);
            }
            if (command.type == GlyphCommand::Type::cubic) include(command.c2x, command.c2y);
        }
        glyph_.commands.push_back(command);
        return true;
    }
    void include(float x, float y) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        if (!bounds_initialized_) {
            left_ = right_ = x;
            bottom_ = top_ = y;
            bounds_initialized_ = true;
            return;
        }
        left_ = std::min(left_, x);
        right_ = std::max(right_, x);
        bottom_ = std::min(bottom_, y);
        top_ = std::max(top_, y);
    }
    CachedGlyph& glyph_;
    bool bounds_initialized_ = false;
    float left_ = 0, right_ = 0, bottom_ = 0, top_ = 0;
public:
    void finish() {
        if (!bounds_initialized_) return;
        if (!glyph_.outline.has_bounds || glyph_.commands.size() == 1) {
            glyph_.outline.left = left_;
            glyph_.outline.right = right_;
            glyph_.outline.bottom = bottom_;
            glyph_.outline.top = top_;
            glyph_.outline.has_bounds = true;
        } else {
            glyph_.outline.left = std::min(glyph_.outline.left, left_);
            glyph_.outline.right = std::max(glyph_.outline.right, right_);
            glyph_.outline.bottom = std::min(glyph_.outline.bottom, bottom_);
            glyph_.outline.top = std::max(glyph_.outline.top, top_);
        }
    }
};

bool replayGlyph(const CachedGlyph& glyph, OutlineSink& sink) {
    for (const auto& command : glyph.commands) {
        bool result = true;
        switch (command.type) {
            case GlyphCommand::Type::move: result = sink.moveTo(command.x, command.y); break;
            case GlyphCommand::Type::line: result = sink.lineTo(command.x, command.y); break;
            case GlyphCommand::Type::quadratic:
                result = sink.quadraticTo(command.c1x, command.c1y, command.x, command.y); break;
            case GlyphCommand::Type::cubic:
                result = sink.cubicTo(command.c1x, command.c1y, command.c2x, command.c2y, command.x, command.y); break;
            case GlyphCommand::Type::close: result = sink.close(); break;
        }
        if (!result) return false;
    }
    return true;
}

struct nector_font {
    std::vector<uint8_t> bytes;
    FaceTables face;
    const TableRef* cmap = nullptr;
    const TableRef* glyf = nullptr;
    const TableRef* loca = nullptr;
    const TableRef* hmtx = nullptr;
    const TableRef* kern = nullptr;
    std::unique_ptr<CffFont> cff;
    size_t cmap_offset = 0;
    uint16_t cmap_format = 0;
    uint16_t units_per_em = 1000;
    int16_t ascent = 800;
    int16_t descent = -200;
    int16_t line_gap = 0;
    uint16_t glyph_count = 0;
    uint16_t metric_count = 0;
    bool long_loca = false;

    mutable std::list<std::pair<uint16_t, CachedGlyph>> glyph_cache;
    mutable std::map<uint16_t, std::list<std::pair<uint16_t, CachedGlyph>>::iterator> glyph_cache_index;
    std::vector<nector_glyph_run_item> layout_items;
    nector_glyph_run layout_run{};

    bool parse(int32_t face_index, std::string& error) {
        if (!parseFaceTables(bytes, face_index, face, error)) return false;
        const TableRef* head = face.find(tag_head);
        const TableRef* hhea = face.find(tag_hhea);
        const TableRef* maxp = face.find(tag_maxp);
        hmtx = face.find(tag_hmtx);
        cmap = face.find(tag_cmap);
        if (head == nullptr || hhea == nullptr || maxp == nullptr || hmtx == nullptr || cmap == nullptr ||
            head->size < 54 || hhea->size < 36 || maxp->size < 6) {
            error = "required sfnt table is missing or truncated";
            return false;
        }
        units_per_em = read16(head->data + 18);
        if (units_per_em < 16 || units_per_em > 16384) {
            error = "invalid unitsPerEm";
            return false;
        }
        const uint16_t loca_format = read16(head->data + 50);
        if (loca_format > 1) {
            error = "invalid indexToLocFormat";
            return false;
        }
        long_loca = loca_format == 1;
        ascent = readSigned16(hhea->data + 4);
        descent = readSigned16(hhea->data + 6);
        line_gap = readSigned16(hhea->data + 8);
        metric_count = read16(hhea->data + 34);
        glyph_count = read16(maxp->data + 4);
        if (glyph_count == 0 || metric_count == 0 || metric_count > glyph_count ||
            hmtx->size < static_cast<size_t>(metric_count) * 4 + static_cast<size_t>(glyph_count - metric_count) * 2) {
            error = "invalid glyph metrics";
            return false;
        }
        glyf = face.find(tag_glyf);
        loca = face.find(tag_loca);
        const TableRef* cff_table = face.find(tag_cff);
        if ((glyf == nullptr) == (cff_table == nullptr)) {
            error = "font must contain exactly one of glyf or CFF";
            return false;
        }
        if (glyf != nullptr) {
            if (loca == nullptr || loca->size < (static_cast<size_t>(glyph_count) + 1) * (long_loca ? 4 : 2)) {
                error = "invalid loca table";
                return false;
            }
        } else {
            cff = std::make_unique<CffFont>();
            if (!cff->parse(cff_table->data, cff_table->size, error)) return false;
            if (cff->glyphCount() != glyph_count) {
                error = "CFF and maxp glyph counts disagree";
                return false;
            }
        }
        kern = face.find(tag_kern);
        return selectCmap(error);
    }

    bool selectCmap(std::string& error) {
        if (cmap->size < 4) {
            error = "truncated cmap";
            return false;
        }
        const uint16_t count = read16(cmap->data + 2);
        if (!rangeValid(4, static_cast<size_t>(count) * 8, cmap->size)) {
            error = "invalid cmap encoding directory";
            return false;
        }
        int best_score = -1;
        size_t best_offset = 0;
        uint16_t best_format = 0;
        for (size_t index = 0; index < count; ++index) {
            const uint8_t* record = cmap->data + 4 + index * 8;
            const uint16_t platform = read16(record);
            const uint16_t encoding = read16(record + 2);
            const uint32_t offset = read32(record + 4);
            if (!rangeValid(offset, 2, cmap->size)) continue;
            const uint16_t format = read16(cmap->data + offset);
            int score = -1;
            if (platform == 3 && encoding == 10 && (format == 10 || format == 12 || format == 13)) score = 100;
            else if (platform == 0 && (format == 10 || format == 12 || format == 13)) score = 95;
            else if (platform == 3 && encoding == 1 && format == 4) score = 90;
            else if (platform == 0 && format == 4) score = 85;
            else if ((platform == 3 || platform == 0) && (format == 0 || format == 2 || format == 6)) score = 50;
            if (score <= best_score) continue;
            size_t required = 0;
            if (format == 0) required = 262;
            else if (format == 2) required = 518;
            else if (format == 4 || format == 6) required = 8;
            else if (format == 10 || format == 12 || format == 13) required = 20;
            else continue;
            if (!rangeValid(offset, required, cmap->size)) continue;
            uint32_t declared = required;
            if (format == 0 || format == 2 || format == 4 || format == 6) declared = read16(cmap->data + offset + 2);
            else declared = read32(cmap->data + offset + 4);
            if (declared < required || !rangeValid(offset, declared, cmap->size)) continue;
            best_score = score;
            best_offset = offset;
            best_format = format;
        }
        if (best_score < 0) {
            error = "font has no supported Unicode cmap";
            return false;
        }
        cmap_offset = best_offset;
        cmap_format = best_format;
        return true;
    }

    uint32_t glyphIndex(uint32_t codepoint) const {
        const uint8_t* table = cmap->data + cmap_offset;
        const size_t remaining = cmap->size - cmap_offset;
        if (cmap_format == 0) {
            return codepoint < 256 ? table[6 + codepoint] : 0;
        }
        if (cmap_format == 2) {
            if (codepoint > 0xffff) return 0;
            const uint8_t high = static_cast<uint8_t>(codepoint >> 8);
            const uint8_t low = static_cast<uint8_t>(codepoint);
            const uint16_t sub_key = read16(table + 6 + high * 2);
            const size_t sub_offset = 518 + static_cast<size_t>(sub_key) * 8;
            if (!rangeValid(sub_offset, 8, remaining)) return 0;
            const uint8_t* sub = table + sub_offset;
            const uint16_t first = read16(sub);
            const uint16_t count = read16(sub + 2);
            const int16_t delta = readSigned16(sub + 4);
            const uint16_t range_offset = read16(sub + 6);
            if (low < first || static_cast<uint32_t>(low - first) >= count || first == 0xff) return 0;
            const size_t position = sub_offset + 6 + range_offset + static_cast<size_t>(low - first) * 2;
            if (!rangeValid(position, 2, remaining)) return 0;
            uint16_t glyph = read16(table + position);
            if (glyph != 0) glyph = static_cast<uint16_t>(glyph + delta);
            return glyph;
        }
        if (cmap_format == 4) {
            if (codepoint > 0xffff || remaining < 16) return 0;
            const uint16_t segments = static_cast<uint16_t>(read16(table + 6) / 2);
            if (segments == 0 || !rangeValid(14, static_cast<size_t>(segments) * 8 + 2, remaining)) return 0;
            const uint8_t* ends = table + 14;
            const uint8_t* starts = ends + segments * 2 + 2;
            const uint8_t* deltas = starts + segments * 2;
            const uint8_t* offsets = deltas + segments * 2;
            for (uint16_t index = 0; index < segments; ++index) {
                const uint16_t end = read16(ends + index * 2);
                if (codepoint > end) continue;
                const uint16_t start = read16(starts + index * 2);
                if (codepoint < start) return 0;
                const int16_t delta = readSigned16(deltas + index * 2);
                const uint16_t range_offset = read16(offsets + index * 2);
                if (range_offset == 0) return static_cast<uint16_t>(codepoint + delta);
                const size_t position = static_cast<size_t>(offsets - table) + index * 2 + range_offset +
                                        static_cast<size_t>(codepoint - start) * 2;
                if (!rangeValid(position, 2, remaining)) return 0;
                uint16_t glyph = read16(table + position);
                return glyph == 0 ? 0 : static_cast<uint16_t>(glyph + delta);
            }
            return 0;
        }
        if (cmap_format == 6) {
            if (remaining < 10) return 0;
            const uint16_t first = read16(table + 6);
            const uint16_t count = read16(table + 8);
            if (codepoint < first || codepoint - first >= count || !rangeValid(10, static_cast<size_t>(count) * 2, remaining)) return 0;
            return read16(table + 10 + static_cast<size_t>(codepoint - first) * 2);
        }
        if (remaining < 20) return 0;
        if (cmap_format == 10) {
            const uint32_t first = read32(table + 12);
            const uint32_t count = read32(table + 16);
            if (codepoint < first || codepoint - first >= count || !rangeValid(20, static_cast<size_t>(count) * 2, remaining)) return 0;
            return read16(table + 20 + static_cast<size_t>(codepoint - first) * 2);
        }
        const uint32_t groups = read32(table + 12);
        if (groups > (remaining - 16) / 12) return 0;
        size_t low = 0;
        size_t high = groups;
        while (low < high) {
            const size_t middle = low + (high - low) / 2;
            const uint8_t* group = table + 16 + middle * 12;
            const uint32_t start = read32(group);
            const uint32_t end = read32(group + 4);
            if (codepoint < start) high = middle;
            else if (codepoint > end) low = middle + 1;
            else {
                const uint32_t glyph = read32(group + 8);
                if (cmap_format == 13) return glyph;
                const uint64_t value = static_cast<uint64_t>(glyph) + codepoint - start;
                return value <= 0xffff ? static_cast<uint32_t>(value) : 0;
            }
        }
        return 0;
    }

    float advance(uint16_t glyph) const {
        if (glyph >= glyph_count) glyph = 0;
        const uint16_t index = std::min(glyph, static_cast<uint16_t>(metric_count - 1));
        return read16(hmtx->data + static_cast<size_t>(index) * 4);
    }

    size_t glyphOffset(uint16_t glyph, size_t& length) const {
        length = 0;
        if (glyph >= glyph_count) return 0;
        size_t start = 0;
        size_t end = 0;
        if (long_loca) {
            start = read32(loca->data + static_cast<size_t>(glyph) * 4);
            end = read32(loca->data + static_cast<size_t>(glyph + 1) * 4);
        } else {
            start = static_cast<size_t>(read16(loca->data + static_cast<size_t>(glyph) * 2)) * 2;
            end = static_cast<size_t>(read16(loca->data + static_cast<size_t>(glyph + 1) * 2)) * 2;
        }
        if (end < start || !rangeValid(start, end - start, glyf->size)) return std::numeric_limits<size_t>::max();
        length = end - start;
        return start;
    }

    bool trueTypeGlyph(uint16_t glyph, OutlineSink& sink, int depth, std::string& error,
                       std::vector<ContourPoint>* captured_points = nullptr,
                       const GlyphTransform& transform = GlyphTransform{}) const {
        if (depth > 16) {
            error = "composite glyph recursion limit exceeded";
            return false;
        }
        size_t length = 0;
        const size_t offset = glyphOffset(glyph, length);
        if (offset == std::numeric_limits<size_t>::max()) {
            error = "invalid glyph location";
            return false;
        }
        if (length == 0) return true;
        if (length < 10) {
            error = "truncated glyf header";
            return false;
        }
        const uint8_t* data = glyf->data + offset;
        const int16_t contours = readSigned16(data);
        size_t cursor = 10;
        if (contours >= 0) {
            const size_t contour_count = static_cast<size_t>(contours);
            if (contour_count == 0) return true;
            if (!rangeValid(cursor, contour_count * 2 + 2, length)) {
                error = "truncated simple glyph contours";
                return false;
            }
            const size_t point_count = static_cast<size_t>(read16(data + cursor + (contour_count - 1) * 2)) + 1;
            std::vector<uint16_t> ends(contour_count);
            for (size_t index = 0; index < contour_count; ++index) {
                ends[index] = read16(data + cursor + index * 2);
                if (index > 0 && ends[index] <= ends[index - 1]) {
                    error = "unordered simple glyph contour ends";
                    return false;
                }
            }
            cursor += contour_count * 2;
            const uint16_t instruction_length = read16(data + cursor);
            cursor += 2;
            if (!rangeValid(cursor, instruction_length, length)) {
                error = "truncated glyph instructions";
                return false;
            }
            cursor += instruction_length;
            std::vector<uint8_t> flags;
            flags.reserve(point_count);
            while (flags.size() < point_count) {
                if (cursor >= length) {
                    error = "truncated glyph flags";
                    return false;
                }
                const uint8_t flag = data[cursor++];
                flags.push_back(flag);
                if ((flag & 8) != 0) {
                    if (cursor >= length) {
                        error = "truncated repeated glyph flag";
                        return false;
                    }
                    const uint8_t repeat = data[cursor++];
                    if (repeat > point_count - flags.size()) {
                        error = "glyph flag repeat exceeds point count";
                        return false;
                    }
                    flags.insert(flags.end(), repeat, flag);
                }
            }
            std::vector<ContourPoint> points(point_count);
            int32_t x = 0;
            for (size_t index = 0; index < point_count; ++index) {
                const uint8_t flag = flags[index];
                int32_t delta = 0;
                if ((flag & 2) != 0) {
                    if (cursor >= length) {
                        error = "truncated glyph x coordinate";
                        return false;
                    }
                    delta = data[cursor++];
                    if ((flag & 16) == 0) delta = -delta;
                } else if ((flag & 16) == 0) {
                    if (!rangeValid(cursor, 2, length)) {
                        error = "truncated glyph x coordinate";
                        return false;
                    }
                    delta = readSigned16(data + cursor);
                    cursor += 2;
                }
                x += delta;
                points[index].x = static_cast<float>(x);
                points[index].on_curve = (flag & 1) != 0;
            }
            int32_t y = 0;
            for (size_t index = 0; index < point_count; ++index) {
                const uint8_t flag = flags[index];
                int32_t delta = 0;
                if ((flag & 4) != 0) {
                    if (cursor >= length) {
                        error = "truncated glyph y coordinate";
                        return false;
                    }
                    delta = data[cursor++];
                    if ((flag & 32) == 0) delta = -delta;
                } else if ((flag & 32) == 0) {
                    if (!rangeValid(cursor, 2, length)) {
                        error = "truncated glyph y coordinate";
                        return false;
                    }
                    delta = readSigned16(data + cursor);
                    cursor += 2;
                }
                y += delta;
                points[index].y = static_cast<float>(y);
            }
            if (captured_points != nullptr &&
                !captureGlyphPoints(*captured_points, points, transform, error)) {
                return false;
            }
            size_t begin = 0;
            for (size_t contour = 0; contour < contour_count; ++contour) {
                const size_t end = static_cast<size_t>(ends[contour]) + 1;
                if (end <= begin || end > points.size()) {
                    error = "invalid glyph contour range";
                    return false;
                }
                std::vector<ContourPoint> part(points.begin() + static_cast<std::ptrdiff_t>(begin),
                                               points.begin() + static_cast<std::ptrdiff_t>(end));
                if (!emitTrueTypeContour(part, sink)) {
                    error = "rasterization path rejected glyph outline";
                    return false;
                }
                begin = end;
            }
            return true;
        }
        if (contours != -1) {
            error = "unsupported negative glyph contour count";
            return false;
        }
        bool more = true;
        size_t components = 0;
        std::vector<ContourPoint> local_points;
        while (more) {
            if (++components > 128 || !rangeValid(cursor, 4, length)) {
                error = "invalid composite glyph";
                return false;
            }
            const uint16_t flags = read16(data + cursor);
            const uint16_t component_glyph = read16(data + cursor + 2);
            cursor += 4;
            if (component_glyph >= glyph_count) {
                error = "composite glyph references an invalid glyph";
                return false;
            }
            const bool words = (flags & 1) != 0;
            const bool xy = (flags & 2) != 0;
            float dx = 0;
            float dy = 0;
            uint32_t parent_point = 0;
            uint32_t component_point = 0;
            if (words) {
                if (!rangeValid(cursor, 4, length)) {
                    error = "truncated composite arguments";
                    return false;
                }
                if (xy) {
                    dx = readSigned16(data + cursor);
                    dy = readSigned16(data + cursor + 2);
                } else {
                    parent_point = read16(data + cursor);
                    component_point = read16(data + cursor + 2);
                }
                cursor += 4;
            } else {
                if (!rangeValid(cursor, 2, length)) {
                    error = "truncated composite arguments";
                    return false;
                }
                if (xy) {
                    dx = static_cast<int8_t>(data[cursor]);
                    dy = static_cast<int8_t>(data[cursor + 1]);
                } else {
                    parent_point = data[cursor];
                    component_point = data[cursor + 1];
                }
                cursor += 2;
            }
            float a = 1, b = 0, c = 0, d = 1;
            if ((flags & 8) != 0) {
                if (!rangeValid(cursor, 2, length)) {
                    error = "truncated composite scale";
                    return false;
                }
                a = d = readSigned16(data + cursor) / 16384.0f;
                cursor += 2;
            } else if ((flags & 64) != 0) {
                if (!rangeValid(cursor, 4, length)) {
                    error = "truncated composite scale";
                    return false;
                }
                a = readSigned16(data + cursor) / 16384.0f;
                d = readSigned16(data + cursor + 2) / 16384.0f;
                cursor += 4;
            } else if ((flags & 128) != 0) {
                if (!rangeValid(cursor, 8, length)) {
                    error = "truncated composite transform";
                    return false;
                }
                a = readSigned16(data + cursor) / 16384.0f;
                b = readSigned16(data + cursor + 2) / 16384.0f;
                c = readSigned16(data + cursor + 4) / 16384.0f;
                d = readSigned16(data + cursor + 6) / 16384.0f;
                cursor += 8;
            }
            GlyphTransform child_transform{a, b, c, d, dx, dy};
            if (xy) {
                TransformSink transformed(sink, a, b, c, d, dx, dy);
                if (!trueTypeGlyph(component_glyph, transformed, depth + 1, error,
                                   &local_points, child_transform)) {
                    return false;
                }
            } else {
                if (parent_point >= local_points.size()) {
                    error = "composite parent point index is out of range";
                    return false;
                }
                std::vector<ContourPoint> component_points;
                NullSink null_sink;
                const GlyphTransform scaled_transform{a, b, c, d, 0, 0};
                if (!trueTypeGlyph(component_glyph, null_sink, depth + 1, error,
                                   &component_points, scaled_transform)) {
                    return false;
                }
                if (component_point >= component_points.size()) {
                    error = "composite component point index is out of range";
                    return false;
                }
                dx = local_points[parent_point].x - component_points[component_point].x;
                dy = local_points[parent_point].y - component_points[component_point].y;
                child_transform.dx = dx;
                child_transform.dy = dy;
                TransformSink transformed(sink, a, b, c, d, dx, dy);
                if (!trueTypeGlyph(component_glyph, transformed, depth + 1, error)) return false;
                const GlyphTransform offset_transform{1, 0, 0, 1, dx, dy};
                if (!captureGlyphPoints(local_points, component_points, offset_transform, error)) return false;
            }
            more = (flags & 32) != 0;
        }
        return captured_points == nullptr ||
               captureGlyphPoints(*captured_points, local_points, transform, error);
    }

    bool parseGlyphOutline(uint16_t glyph, OutlineSink& sink, GlyphOutline& outline, std::string& error) const {
        outline.advance = advance(glyph);
        if (glyf != nullptr) return trueTypeGlyph(glyph, sink, 0, error);
        const float horizontal_advance = outline.advance;
        const bool result = cff->glyphOutline(glyph, sink, outline, error);
        outline.advance = horizontal_advance;
        return result;
    }

    bool glyphOutlineById(uint16_t glyph, OutlineSink& sink, GlyphOutline& outline, std::string& error) const {
        const auto found = glyph_cache_index.find(glyph);
        if (found != glyph_cache_index.end()) {
            glyph_cache.splice(glyph_cache.begin(), glyph_cache, found->second);
            outline = found->second->second.outline;
            if (!replayGlyph(found->second->second, sink)) {
                error = "rasterization path rejected glyph outline";
                return false;
            }
            return true;
        }
        CachedGlyph cached;
        RecordingSink recorder(cached);
        if (!parseGlyphOutline(glyph, recorder, cached.outline, error)) return false;
        recorder.finish();
        glyph_cache.push_front({glyph, std::move(cached)});
        glyph_cache_index[glyph] = glyph_cache.begin();
        if (glyph_cache.size() > 256) {
            glyph_cache_index.erase(glyph_cache.back().first);
            glyph_cache.pop_back();
        }
        outline = glyph_cache.front().second.outline;
        return replayGlyph(glyph_cache.front().second, sink);
    }

    bool glyphOutline(uint32_t codepoint, OutlineSink& sink, GlyphOutline& outline, std::string& error) const {
        uint32_t mapped = glyphIndex(codepoint);
        if (mapped >= glyph_count) mapped = 0;
        return glyphOutlineById(static_cast<uint16_t>(mapped), sink, outline, error);
    }

    float kerning(uint32_t left_codepoint, uint32_t right_codepoint) const {
        if (kern == nullptr || kern->size < 4) return 0;
        const uint16_t left = static_cast<uint16_t>(std::min<uint32_t>(glyphIndex(left_codepoint), glyph_count - 1));
        const uint16_t right = static_cast<uint16_t>(std::min<uint32_t>(glyphIndex(right_codepoint), glyph_count - 1));
        const uint16_t tables = read16(kern->data + 2);
        size_t cursor = 4;
        float adjustment = 0;
        for (uint16_t index = 0; index < tables; ++index) {
            if (!rangeValid(cursor, 6, kern->size)) break;
            const uint16_t length = read16(kern->data + cursor + 2);
            const uint16_t coverage = read16(kern->data + cursor + 4);
            if (length < 6 || !rangeValid(cursor, length, kern->size)) break;
            const uint8_t format = static_cast<uint8_t>(coverage >> 8);
            const bool horizontal = (coverage & 1) != 0;
            const bool cross_stream = (coverage & 4) != 0;
            if (format == 0 && horizontal && !cross_stream && length >= 14) {
                const uint16_t pairs = read16(kern->data + cursor + 6);
                if (rangeValid(cursor + 14, static_cast<size_t>(pairs) * 6, cursor + length)) {
                    const uint32_t key = (static_cast<uint32_t>(left) << 16) | right;
                    size_t low = 0;
                    size_t high = pairs;
                    while (low < high) {
                        const size_t middle = low + (high - low) / 2;
                        const uint8_t* pair = kern->data + cursor + 14 + middle * 6;
                        const uint32_t other = read32(pair);
                        if (key < other) high = middle;
                        else if (key > other) low = middle + 1;
                        else {
                            adjustment += readSigned16(pair + 4);
                            break;
                        }
                    }
                }
            }
            cursor += length;
        }
        return adjustment;
    }
};

namespace {

bool decodeCodepoint(const char* text, size_t size, size_t& cursor, uint32_t& codepoint) {
    if (cursor >= size) return false;
    const uint8_t first = static_cast<uint8_t>(text[cursor++]);
    if (first < 0x80) {
        codepoint = first;
        return true;
    }
    int count = 0;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; codepoint = first & 0x1f; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; codepoint = first & 0x0f; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; codepoint = first & 7; }
    else return false;
    for (int index = 0; index < count; ++index) {
        const uint8_t next = static_cast<uint8_t>(text[cursor++]);
        codepoint = (codepoint << 6) | (next & 0x3f);
    }
    return true;
}

nector_status convertParseError(const std::string& error) {
    if (error.find("unsupported") != std::string::npos || error.find("not supported") != std::string::npos) {
        return nectorFail(nector_unsupported, error.c_str());
    }
    return nectorFail(nector_invalid_format, error.c_str());
}

nector_status layoutText(nector_font_handle font, float pixel_size, const char* text, size_t length,
                         std::vector<nector_glyph_run_item>& items, float& width, float& height) {
    items.clear();
    width = 0;
    height = 0;
    const float scale = pixel_size / font->units_per_em;
    const float line_height = (font->ascent - font->descent + font->line_gap) * scale;
    size_t cursor = 0;
    uint32_t previous = 0;
    float pen_x = 0;
    float pen_y = 0;
    int32_t line = 0;
    while (cursor < length) {
        const size_t cluster = cursor;
        uint32_t codepoint = 0;
        if (!decodeCodepoint(text, length, cursor, codepoint)) return nectorFail(nector_invalid_format, "invalid UTF-8 text");
        if (codepoint == '\r') continue;
        if (codepoint == '\n') {
            width = std::max(width, pen_x);
            pen_x = 0;
            pen_y += line_height;
            previous = 0;
            ++line;
            continue;
        }
        uint32_t mapped = font->glyphIndex(codepoint);
        if (mapped >= font->glyph_count) mapped = 0;
        const uint16_t glyph = static_cast<uint16_t>(mapped);
        if (previous != 0) pen_x += font->kerning(previous, codepoint) * scale;
        NullSink null_sink;
        GlyphOutline outline;
        std::string error;
        if (!font->glyphOutlineById(glyph, null_sink, outline, error)) return convertParseError(error);
        nector_glyph_run_item item{};
        item.glyph_id = glyph;
        item.codepoint = codepoint;
        item.cluster = cluster;
        item.advance = outline.advance * scale;
        item.pen_x = pen_x;
        item.baseline_y = pen_y;
        item.line_index = line;
        item.has_tight_bounds = outline.has_bounds ? 1 : 0;
        if (outline.has_bounds) {
            item.tight_bounds = {pen_x + outline.left * scale, pen_y - outline.top * scale,
                                 (outline.right - outline.left) * scale, (outline.top - outline.bottom) * scale};
        }
        items.push_back(item);
        pen_x += item.advance;
        previous = codepoint;
    }
    width = std::max(width, pen_x);
    height = line_height * (line + 1);
    return nector_ok;
}

bool readFile(const char* path, std::vector<uint8_t>& bytes, std::string& error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        error = "cannot open font file";
        return false;
    }
    const std::streamoff end = stream.tellg();
    if (end < 0 || static_cast<uint64_t>(end) > max_font_size) {
        error = "font file exceeds size limit";
        return false;
    }
    bytes.resize(static_cast<size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), end)) {
        error = "cannot read font file";
        return false;
    }
    return true;
}

bool prepareFontBytes(const uint8_t* data, size_t size, std::vector<uint8_t>& output, std::string& error) {
    if (data == nullptr || size < 12 || size > max_font_size) {
        error = "font buffer size is invalid";
        return false;
    }
    const uint32_t signature = read32(data);
    if (signature == 0x774f4646u) return decodeWoff(data, size, output, error);
    if (signature == 0x774f4632u) return nector_font_detail::decodeWoff2(data, size, output, error);
    output.assign(data, data + size);
    return true;
}

} // namespace

nector_status nectorLoadFont(const char* path, int32_t face_index, nector_font_handle* out_font) {
    if (path == nullptr || path[0] == '\0' || out_font == nullptr) return nectorFail(nector_invalid_argument, "font path or output is null");
    *out_font = nullptr;
    nector_api_try {
        std::vector<uint8_t> encoded;
        std::string error;
        if (!readFile(path, encoded, error)) return nectorFail(nector_io_error, error.c_str());
        return nectorLoadFontMemory(encoded.data(), encoded.size(), face_index, out_font);
    } nector_api_catch
}

nector_status nectorLoadFontMemory(const uint8_t* data, size_t size, int32_t face_index, nector_font_handle* out_font) {
    if (out_font == nullptr || face_index < 0) return nectorFail(nector_invalid_argument, "font output or face index is invalid");
    *out_font = nullptr;
    nector_api_try {
        std::unique_ptr<nector_font> font = std::make_unique<nector_font>();
        std::string error;
        if (!prepareFontBytes(data, size, font->bytes, error)) return convertParseError(error);
        if (!font->parse(face_index, error)) return convertParseError(error);
        *out_font = font.release();
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyFont(nector_font_handle font) {
    delete font;
}

nector_status nectorFontGetFaceCount(const uint8_t* data, size_t size, int32_t* out_count) {
    if (out_count == nullptr) return nectorFail(nector_invalid_argument, "out_count is null");
    *out_count = 0;
    nector_api_try {
        std::string error;
        int32_t count = 0;
        if (!sfntFaceCount(data, size, count, error)) return convertParseError(error);
        *out_count = count;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorFontGetMetrics(nector_font_handle font, float pixel_size, nector_font_metrics* out_metrics) {
    if (font == nullptr || out_metrics == nullptr || !nectorFinite(pixel_size) || pixel_size <= 0 || pixel_size > 4096) {
        return nectorFail(nector_invalid_argument, "font, metrics, or pixel size is invalid");
    }
    const float scale = pixel_size / font->units_per_em;
    out_metrics->ascent = font->ascent * scale;
    out_metrics->descent = font->descent * scale;
    out_metrics->line_gap = font->line_gap * scale;
    out_metrics->line_height = (font->ascent - font->descent + font->line_gap) * scale;
    return nector_ok;
}

nector_status nectorFontMeasureText(nector_font_handle font, float pixel_size, const char* utf8_text, nector_text_metrics* out_metrics) {
    if (font == nullptr || out_metrics == nullptr || !nectorFinite(pixel_size) || pixel_size <= 0 || pixel_size > 4096 || utf8_text == nullptr) {
        return nectorFail(nector_invalid_argument, "text measurement argument is invalid");
    }
    out_metrics->width = 0;
    out_metrics->height = 0;
    out_metrics->glyph_count = 0;
    const size_t length = std::strlen(utf8_text);
    if (length > max_text_size) return nectorFail(nector_limit_exceeded, "text exceeds 16 MiB");
    const nector_status utf8_status = nectorCoreValidateUtf8(utf8_text, length);
    if (utf8_status != nector_ok) return utf8_status;
    nector_api_try {
        std::vector<nector_glyph_run_item> items;
        const nector_status status = layoutText(font, pixel_size, utf8_text, length, items, out_metrics->width, out_metrics->height);
        if (status != nector_ok) return status;
        out_metrics->glyph_count = static_cast<int32_t>(items.size());
        return nector_ok;
    } nector_api_catch
}

nector_status nectorFontLayoutText(nector_font_handle font, float pixel_size, const char* utf8_text,
                                    nector_glyph_run* out_run) {
    if (font == nullptr || out_run == nullptr || !nectorFinite(pixel_size) || pixel_size <= 0 || pixel_size > 4096 ||
        utf8_text == nullptr) {
        return nectorFail(nector_invalid_argument, "text layout argument is invalid");
    }
    *out_run = {};
    const size_t length = std::strlen(utf8_text);
    if (length > max_text_size) return nectorFail(nector_limit_exceeded, "text exceeds 16 MiB");
    const nector_status utf8_status = nectorCoreValidateUtf8(utf8_text, length);
    if (utf8_status != nector_ok) return utf8_status;
    nector_api_try {
        float width = 0;
        float height = 0;
        const nector_status status = layoutText(font, pixel_size, utf8_text, length, font->layout_items, width, height);
        if (status != nector_ok) return status;
        font->layout_run.items = font->layout_items.data();
        font->layout_run.count = font->layout_items.size();
        font->layout_run.width = width;
        font->layout_run.height = height;
        *out_run = font->layout_run;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorFontDrawText(nector_painter_handle painter, nector_font_handle font, float pixel_size,
                                 float x, float baseline_y, const char* utf8_text, nector_color color) {
    if (painter == nullptr || font == nullptr || !nectorFinite(pixel_size) || pixel_size <= 0 || pixel_size > 4096 ||
        !nectorFinite(x) || !nectorFinite(baseline_y) || utf8_text == nullptr) {
        return nectorFail(nector_invalid_argument, "text drawing argument is invalid");
    }
    const size_t length = std::strlen(utf8_text);
    if (length > max_text_size) return nectorFail(nector_limit_exceeded, "text exceeds 16 MiB");
    const nector_status utf8_status = nectorCoreValidateUtf8(utf8_text, length);
    if (utf8_status != nector_ok) return utf8_status;
    nector_api_try {
        std::vector<nector_glyph_run_item> items;
        float width = 0;
        float height = 0;
        nector_status status = layoutText(font, pixel_size, utf8_text, length, items, width, height);
        if (status != nector_ok) return status;
        nector_path_handle path = nullptr;
        status = nectorCreatePath(&path);
        if (status != nector_ok) return status;
        std::unique_ptr<nector_path, decltype(&nectorDestroyPath)> path_guard(path, nectorDestroyPath);
        status = nectorPainterSetFillColor(painter, color);
        if (status != nector_ok) return status;
        const float scale = pixel_size / font->units_per_em;
        for (const auto& item : items) {
            status = nectorPathClear(path);
            if (status != nector_ok) return status;
            PathSink sink(path, scale, x + item.pen_x, baseline_y + item.baseline_y);
            GlyphOutline outline;
            std::string error;
            if (!font->glyphOutlineById(static_cast<uint16_t>(item.glyph_id), sink, outline, error)) {
                if (sink.status_ != nector_ok) return sink.status_;
                return convertParseError(error);
            }
            status = nectorPainterFillPath(painter, path, nector_fill_nonzero);
            if (status != nector_ok) return status;
        }
        return nector_ok;
    } nector_api_catch
}

nector_status nectorFontGetGlyphPath(nector_font_handle font, uint32_t codepoint, float pixel_size, nector_path_handle path) {
    if (font == nullptr || path == nullptr || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
        !nectorFinite(pixel_size) || pixel_size <= 0 || pixel_size > 4096) {
        return nectorFail(nector_invalid_argument, "glyph path argument is invalid");
    }
    nector_api_try {
        nector_status status = nectorPathClear(path);
        if (status != nector_ok) return status;
        PathSink sink(path, pixel_size / font->units_per_em, 0, 0);
        GlyphOutline outline;
        std::string error;
        if (!font->glyphOutline(codepoint, sink, outline, error)) {
            if (sink.status_ != nector_ok) return sink.status_;
            return convertParseError(error);
        }
        return nector_ok;
    } nector_api_catch
}
