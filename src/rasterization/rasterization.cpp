#include "rasterization_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace {
constexpr int coverage_scale = 16;
constexpr int coverage_samples = 256;
constexpr size_t max_surface_pixels = 100000000;

struct Bounds {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    bool empty() const { return x0 >= x1 || y0 >= y1; }
};

bool validPoint(float x, float y) {
    return nectorFinite(x) && nectorFinite(y) && std::fabs(x) <= 1.0e7f && std::fabs(y) <= 1.0e7f;
}

bool validColor(nector_color color) {
    return nectorFinite(color.r) && nectorFinite(color.g) && nectorFinite(color.b) && nectorFinite(color.a) &&
           color.r >= 0 && color.r <= 1 && color.g >= 0 && color.g <= 1 &&
           color.b >= 0 && color.b <= 1 && color.a >= 0 && color.a <= 1;
}

bool validTransform(nector_transform value) {
    return nectorFinite(value.m00) && nectorFinite(value.m01) && nectorFinite(value.m02) &&
           nectorFinite(value.m10) && nectorFinite(value.m11) && nectorFinite(value.m12);
}

nector_point transformPoint(nector_transform value, nector_point point) {
    return {value.m00 * point.x + value.m01 * point.y + value.m02,
            value.m10 * point.x + value.m11 * point.y + value.m12};
}

bool invertTransform(nector_transform value, nector_transform* out_inverse) {
    const float determinant = value.m00 * value.m11 - value.m01 * value.m10;
    if (std::fabs(determinant) < 1.0e-12f) return false;
    const float inverse = 1.0f / determinant;
    out_inverse->m00 = value.m11 * inverse;
    out_inverse->m01 = -value.m01 * inverse;
    out_inverse->m02 = (value.m01 * value.m12 - value.m11 * value.m02) * inverse;
    out_inverse->m10 = -value.m10 * inverse;
    out_inverse->m11 = value.m00 * inverse;
    out_inverse->m12 = (value.m10 * value.m02 - value.m00 * value.m12) * inverse;
    return true;
}

Bounds clipBounds(nector_painter_handle painter) {
    const auto& clip = painter->state.clip;
    Bounds result;
    result.x0 = std::max(0, static_cast<int>(std::ceil(clip.x)));
    result.y0 = std::max(0, static_cast<int>(std::ceil(clip.y)));
    result.x1 = std::min(painter->surface->width, static_cast<int>(std::ceil(clip.x + clip.width)));
    result.y1 = std::min(painter->surface->height, static_cast<int>(std::ceil(clip.y + clip.height)));
    return result;
}

Bounds pathBounds(const std::vector<std::vector<nector_point>>& paths, Bounds clip, float expansion) {
    float min_x = static_cast<float>(clip.x1);
    float min_y = static_cast<float>(clip.y1);
    float max_x = static_cast<float>(clip.x0);
    float max_y = static_cast<float>(clip.y0);
    bool any = false;
    for (const auto& path : paths) {
        for (const auto point : path) {
            min_x = std::min(min_x, point.x);
            min_y = std::min(min_y, point.y);
            max_x = std::max(max_x, point.x);
            max_y = std::max(max_y, point.y);
            any = true;
        }
    }
    if (!any) return {};
    Bounds result;
    result.x0 = std::max(clip.x0, static_cast<int>(std::floor(min_x - expansion)));
    result.y0 = std::max(clip.y0, static_cast<int>(std::floor(min_y - expansion)));
    result.x1 = std::min(clip.x1, static_cast<int>(std::ceil(max_x + expansion + 1.0f)));
    result.y1 = std::min(clip.y1, static_cast<int>(std::ceil(max_y + expansion + 1.0f)));
    return result;
}

nector_color paintColor(nector_painter_handle painter, float x, float y) {
    const auto& state = painter->state;
    if (state.paint_type == nector_painter::PaintType::solid) return state.color;
    const nector_point first = transformPoint(state.transform, state.first_point);
    const nector_point second = transformPoint(state.transform, state.second_point);
    float amount = 0.0f;
    if (state.paint_type == nector_painter::PaintType::linear) {
        const float dx = second.x - first.x;
        const float dy = second.y - first.y;
        const float denominator = dx * dx + dy * dy;
        amount = denominator > 1.0e-12f ? ((x - first.x) * dx + (y - first.y) * dy) / denominator : 0.0f;
    } else {
        const float dx = x - first.x;
        const float dy = y - first.y;
        const float sx = std::sqrt(state.transform.m00 * state.transform.m00 + state.transform.m10 * state.transform.m10);
        const float sy = std::sqrt(state.transform.m01 * state.transform.m01 + state.transform.m11 * state.transform.m11);
        const float radius = state.radius * (sx + sy) * 0.5f;
        amount = radius > 1.0e-12f ? std::sqrt(dx * dx + dy * dy) / radius : 0.0f;
    }
    amount = std::max(0.0f, std::min(1.0f, amount));
    const auto& a = state.color;
    const auto& b = state.second_color;
    return {a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount,
            a.b + (b.b - a.b) * amount, a.a + (b.a - a.a) * amount};
}

void blendPixel(nector_surface_handle surface, int x, int y, nector_color source) {
    if (source.a <= 0) return;
    const size_t offset = (static_cast<size_t>(y) * surface->width + x) * 4;
    uint8_t* destination = surface->pixels.data() + offset;
    const float destination_alpha = destination[3] / 255.0f;
    const float output_alpha = source.a + destination_alpha * (1.0f - source.a);
    if (output_alpha <= 0) {
        std::memset(destination, 0, 4);
        return;
    }
    const float source_weight = source.a / output_alpha;
    const float destination_weight = destination_alpha * (1.0f - source.a) / output_alpha;
    destination[0] = static_cast<uint8_t>(std::lround(255.0f * std::min(1.0f, source.r * source_weight + destination[0] / 255.0f * destination_weight)));
    destination[1] = static_cast<uint8_t>(std::lround(255.0f * std::min(1.0f, source.g * source_weight + destination[1] / 255.0f * destination_weight)));
    destination[2] = static_cast<uint8_t>(std::lround(255.0f * std::min(1.0f, source.b * source_weight + destination[2] / 255.0f * destination_weight)));
    destination[3] = static_cast<uint8_t>(std::lround(output_alpha * 255.0f));
}

void resolveCoverage(nector_painter_handle painter, Bounds bounds, const std::vector<uint16_t>& coverage) {
    const int width = bounds.x1 - bounds.x0;
    for (int y = bounds.y0; y < bounds.y1; ++y) {
        for (int x = bounds.x0; x < bounds.x1; ++x) {
            const uint16_t samples = coverage[static_cast<size_t>(y - bounds.y0) * width + (x - bounds.x0)];
            if (samples == 0) continue;
            nector_color color = paintColor(painter, x + 0.5f, y + 0.5f);
            color.a *= static_cast<float>(samples) / coverage_samples;
            blendPixel(painter->surface, x, y, color);
        }
    }
}

using Polyline = std::vector<nector_point>;
using Polylines = std::vector<Polyline>;

class PathFlattener {
public:
    PathFlattener(nector_transform transform, bool device_space)
        : transform_(transform), device_space_(device_space) {}

    Polyline flatten(const nector_path::Subpath& subpath) {
        Polyline points{output(subpath.start)};
        size_t generated = 0;
        nector_point current = subpath.start;
        for (const auto& segment : subpath.segments) {
            if (segment.type == nector_path::Segment::Type::line) {
                points.push_back(output(segment.end));
            } else if (segment.type == nector_path::Segment::Type::quadratic) {
                flattenQuadratic(current, segment.control1, segment.end, 0, generated, points);
            } else {
                flattenCubic(current, segment.control1, segment.control2, segment.end, 0, generated, points);
            }
            current = segment.end;
        }
        return points;
    }

private:
    nector_transform transform_;
    bool device_space_;

    nector_point output(nector_point point) const {
        return device_space_ ? transformPoint(transform_, point) : point;
    }

    static float distanceFromLine(nector_point point, nector_point a, nector_point b) {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float denominator = std::sqrt(dx * dx + dy * dy);
        if (denominator < 1.0e-12f) {
            const float px = point.x - a.x;
            const float py = point.y - a.y;
            return std::sqrt(px * px + py * py);
        }
        return std::fabs(dx * (a.y - point.y) - (a.x - point.x) * dy) / denominator;
    }

    bool flat(nector_point a, nector_point b, nector_point c) const {
        return distanceFromLine(transformPoint(transform_, b), transformPoint(transform_, a),
                                transformPoint(transform_, c)) <= 0.08f;
    }

    bool flat(nector_point a, nector_point b, nector_point c, nector_point d) const {
        const nector_point ta = transformPoint(transform_, a);
        const nector_point td = transformPoint(transform_, d);
        return std::max(distanceFromLine(transformPoint(transform_, b), ta, td),
                        distanceFromLine(transformPoint(transform_, c), ta, td)) <= 0.08f;
    }

    void flattenQuadratic(nector_point a, nector_point b, nector_point c, int depth,
                          size_t& generated, Polyline& points) {
        if (depth >= 24 || generated >= 1000000 || flat(a, b, c)) {
            points.push_back(output(c));
            ++generated;
            return;
        }
        const nector_point ab{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        const nector_point bc{(b.x + c.x) * 0.5f, (b.y + c.y) * 0.5f};
        const nector_point middle{(ab.x + bc.x) * 0.5f, (ab.y + bc.y) * 0.5f};
        flattenQuadratic(a, ab, middle, depth + 1, generated, points);
        flattenQuadratic(middle, bc, c, depth + 1, generated, points);
    }

    void flattenCubic(nector_point a, nector_point b, nector_point c, nector_point d, int depth,
                      size_t& generated, Polyline& points) {
        if (depth >= 24 || generated >= 1000000 || flat(a, b, c, d)) {
            points.push_back(output(d));
            ++generated;
            return;
        }
        const nector_point ab{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        const nector_point bc{(b.x + c.x) * 0.5f, (b.y + c.y) * 0.5f};
        const nector_point cd{(c.x + d.x) * 0.5f, (c.y + d.y) * 0.5f};
        const nector_point abc{(ab.x + bc.x) * 0.5f, (ab.y + bc.y) * 0.5f};
        const nector_point bcd{(bc.x + cd.x) * 0.5f, (bc.y + cd.y) * 0.5f};
        const nector_point middle{(abc.x + bcd.x) * 0.5f, (abc.y + bcd.y) * 0.5f};
        flattenCubic(a, ab, abc, middle, depth + 1, generated, points);
        flattenCubic(middle, bcd, cd, d, depth + 1, generated, points);
    }
};

Polylines flattenedPaths(nector_painter_handle painter, nector_path_handle path, bool device_space) {
    Polylines result;
    result.reserve(path->subpaths.size());
    PathFlattener flattener(painter->state.transform, device_space);
    for (const auto& subpath : path->subpaths) {
        if (subpath.segments.empty()) continue;
        Polyline points = flattener.flatten(subpath);
        if (points.size() >= 2) result.push_back(std::move(points));
    }
    return result;
}

struct Crossing {
    float x;
    int direction;
};

void addCoverageSpan(std::vector<uint16_t>& coverage, Bounds bounds, int high_y, float start_x, float end_x) {
    if (!(end_x > start_x)) return;
    const int width = bounds.x1 - bounds.x0;
    const int low_y = high_y / coverage_scale - bounds.y0;
    const int begin = std::max(bounds.x0, static_cast<int>(std::floor(start_x)));
    const int end = std::min(bounds.x1 - 1, static_cast<int>(std::ceil(end_x)) - 1);
    for (int x = begin; x <= end; ++x) {
        const float overlap = std::min(end_x, static_cast<float>(x + 1)) - std::max(start_x, static_cast<float>(x));
        if (overlap <= 0) continue;
        auto& value = coverage[static_cast<size_t>(low_y) * width + (x - bounds.x0)];
        value = static_cast<uint16_t>(std::min(coverage_samples,
            static_cast<int>(value) + static_cast<int>(std::lround(overlap * coverage_scale))));
    }
}

float cross(nector_point a, nector_point b) {
    return a.x * b.y - a.y * b.x;
}

nector_point subtract(nector_point a, nector_point b) {
    return {a.x - b.x, a.y - b.y};
}

nector_point add(nector_point a, nector_point b) {
    return {a.x + b.x, a.y + b.y};
}

nector_point multiply(nector_point value, float scale) {
    return {value.x * scale, value.y * scale};
}

float dot(nector_point a, nector_point b) {
    return a.x * b.x + a.y * b.y;
}

float length(nector_point value) {
    return std::sqrt(dot(value, value));
}

nector_point normalized(nector_point value) {
    const float magnitude = length(value);
    return magnitude > 1.0e-12f ? multiply(value, 1.0f / magnitude) : nector_point{};
}

void appendOutline(Polylines& outlines, Polyline points, nector_transform transform) {
    if (points.size() < 3) return;
    double area = 0;
    for (size_t index = 0; index < points.size(); ++index) {
        const auto a = points[index];
        const auto b = points[(index + 1) % points.size()];
        area += static_cast<double>(a.x) * b.y - static_cast<double>(a.y) * b.x;
    }
    if (std::fabs(area) < 1.0e-12) return;
    if (area < 0) std::reverse(points.begin(), points.end());
    for (auto& point : points) point = transformPoint(transform, point);
    outlines.push_back(std::move(points));
}

void appendSegmentOutline(Polylines& outlines, nector_point a, nector_point b, float half_width,
                          nector_transform transform) {
    const nector_point direction = normalized(subtract(b, a));
    if (length(direction) < 0.5f) return;
    const nector_point normal{-direction.y * half_width, direction.x * half_width};
    appendOutline(outlines, {add(a, normal), add(b, normal), subtract(b, normal), subtract(a, normal)}, transform);
}

void appendDisc(Polylines& outlines, nector_point center, float radius, nector_transform transform) {
    const nector_point transformed = transformPoint(transform, center);
    const nector_point edge = transformPoint(transform, {center.x + radius, center.y});
    const float device_radius = length(subtract(edge, transformed));
    const int segments = std::max(16, std::min(128, static_cast<int>(std::ceil(device_radius * 2.0f))));
    Polyline points;
    points.reserve(static_cast<size_t>(segments));
    constexpr float pi = 3.14159265358979323846f;
    for (int index = 0; index < segments; ++index) {
        const float angle = 2.0f * pi * index / segments;
        points.push_back({center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
    }
    appendOutline(outlines, std::move(points), transform);
}

void appendJoin(Polylines& outlines, nector_point joint, nector_point incoming, nector_point outgoing,
                float half_width, nector_line_join join, float miter_limit, nector_transform transform) {
    const nector_point first = normalized(incoming);
    const nector_point second = normalized(outgoing);
    const float turn = cross(first, second);
    if (length(first) < 0.5f || length(second) < 0.5f || std::fabs(turn) < 1.0e-7f) return;
    if (join == nector_join_round) {
        appendDisc(outlines, joint, half_width, transform);
        return;
    }
    const float side = turn > 0 ? -1.0f : 1.0f;
    const nector_point n1{-first.y * side * half_width, first.x * side * half_width};
    const nector_point n2{-second.y * side * half_width, second.x * side * half_width};
    const nector_point outer1 = add(joint, n1);
    const nector_point outer2 = add(joint, n2);
    if (join == nector_join_miter) {
        const float denominator = 1.0f + dot(first, second);
        if (denominator > 1.0e-7f) {
            const float ratio = std::sqrt(2.0f / denominator);
            if (ratio <= miter_limit) {
                const nector_point miter = add(joint, multiply(add(n1, n2), 1.0f / denominator));
                appendOutline(outlines, {joint, outer1, miter, outer2}, transform);
                return;
            }
        }
    }
    appendOutline(outlines, {joint, outer1, outer2}, transform);
}

Polylines applyDash(const Polyline& points, const float* pattern, size_t count, float offset) {
    std::vector<float> dashes(pattern, pattern + count);
    if ((dashes.size() & 1) != 0) dashes.insert(dashes.end(), pattern, pattern + count);
    float total = 0;
    for (float value : dashes) total += value;
    offset = std::fmod(offset, total);
    if (offset < 0) offset += total;
    size_t dash_index = 0;
    while (offset >= dashes[dash_index]) {
        offset -= dashes[dash_index];
        dash_index = (dash_index + 1) % dashes.size();
    }
    float remaining = dashes[dash_index] - offset;
    bool on = (dash_index & 1) == 0;
    Polylines result;
    Polyline current;
    for (size_t index = 1; index < points.size(); ++index) {
        const nector_point a = points[index - 1];
        const nector_point delta = subtract(points[index], a);
        const float segment_length = length(delta);
        if (segment_length < 1.0e-12f) continue;
        const nector_point direction = multiply(delta, 1.0f / segment_length);
        float consumed = 0;
        while (consumed < segment_length) {
            const float step = std::min(remaining, segment_length - consumed);
            if (on) {
                if (current.empty()) current.push_back(add(a, multiply(direction, consumed)));
                current.push_back(add(a, multiply(direction, consumed + step)));
            }
            consumed += step;
            remaining -= step;
            if (remaining <= 1.0e-6f) {
                if (on && current.size() >= 2) result.push_back(std::move(current));
                current.clear();
                dash_index = (dash_index + 1) % dashes.size();
                remaining = dashes[dash_index];
                on = (dash_index & 1) == 0;
            }
        }
    }
    if (on && current.size() >= 2) result.push_back(std::move(current));
    return result;
}

void appendStrokePolyline(Polylines& outlines, const Polyline& source, bool closed,
                          const nector_stroke_style& style, nector_transform transform) {
    Polyline points = source;
    if (closed && points.size() > 2 && length(subtract(points.front(), points.back())) < 1.0e-6f) points.pop_back();
    if (points.size() < 2) return;
    const float half_width = style.width * 0.5f;
    const size_t segment_count = closed ? points.size() : points.size() - 1;
    for (size_t index = 0; index < segment_count; ++index) {
        nector_point a = points[index];
        nector_point b = points[(index + 1) % points.size()];
        if (!closed && style.cap == nector_cap_square) {
            const nector_point direction = normalized(subtract(b, a));
            if (index == 0) a = subtract(a, multiply(direction, half_width));
            if (index + 1 == segment_count) b = add(b, multiply(direction, half_width));
        }
        appendSegmentOutline(outlines, a, b, half_width, transform);
    }
    const size_t joint_begin = closed ? 0 : 1;
    const size_t joint_end = closed ? points.size() : points.size() - 1;
    for (size_t index = joint_begin; index < joint_end; ++index) {
        const nector_point joint = points[index];
        const nector_point previous = points[(index + points.size() - 1) % points.size()];
        const nector_point next = points[(index + 1) % points.size()];
        appendJoin(outlines, joint, subtract(joint, previous), subtract(next, joint), half_width,
                   style.join, style.miter_limit, transform);
    }
    if (!closed && style.cap == nector_cap_round) {
        appendDisc(outlines, points.front(), half_width, transform);
        appendDisc(outlines, points.back(), half_width, transform);
    }
}

Polylines strokeOutlines(nector_painter_handle painter, nector_path_handle path, const nector_stroke_style& style) {
    Polylines outlines;
    PathFlattener flattener(painter->state.transform, false);
    for (const auto& subpath : path->subpaths) {
        if (subpath.segments.empty()) continue;
        Polyline points = flattener.flatten(subpath);
        if (style.dash_count != 0) {
            if (subpath.closed) points.push_back(points.front());
            for (const auto& dashed : applyDash(points, style.dash_pattern, style.dash_count, style.dash_offset)) {
                appendStrokePolyline(outlines, dashed, false, style, painter->state.transform);
            }
        } else {
            appendStrokePolyline(outlines, points, subpath.closed, style, painter->state.transform);
        }
    }
    return outlines;
}

nector_status rasterizePolylines(nector_painter_handle painter, const Polylines& paths, nector_fill_rule fill_rule) {
    const Bounds bounds = pathBounds(paths, clipBounds(painter), 0.0f);
    if (bounds.empty()) return nector_ok;
    const int width = bounds.x1 - bounds.x0;
    const int height = bounds.y1 - bounds.y0;
    std::vector<uint16_t> coverage(static_cast<size_t>(width) * height, 0);
    std::vector<Crossing> crossings;
    for (int high_y = bounds.y0 * coverage_scale; high_y < bounds.y1 * coverage_scale; ++high_y) {
        const float sample_y = (high_y + 0.5f) / coverage_scale;
        crossings.clear();
        for (const auto& points : paths) {
            if (points.size() < 3) continue;
            for (size_t index = 0; index < points.size(); ++index) {
                const nector_point a = points[index];
                const nector_point b = points[(index + 1) % points.size()];
                if ((sample_y >= a.y && sample_y < b.y) || (sample_y >= b.y && sample_y < a.y)) {
                    const float amount = (sample_y - a.y) / (b.y - a.y);
                    crossings.push_back({a.x + (b.x - a.x) * amount, b.y > a.y ? 1 : -1});
                }
            }
        }
        std::sort(crossings.begin(), crossings.end(), [](Crossing a, Crossing b) { return a.x < b.x; });
        int winding = 0;
        bool inside = false;
        float start_x = 0;
        for (const auto crossing : crossings) {
            winding += crossing.direction;
            const bool next_inside = fill_rule == nector_fill_even_odd ? (winding & 1) != 0 : winding != 0;
            if (!inside && next_inside) start_x = crossing.x;
            if (inside && !next_inside) addCoverageSpan(coverage, bounds, high_y, start_x, crossing.x);
            inside = next_inside;
        }
    }
    resolveCoverage(painter, bounds, coverage);
    return nector_ok;
}

nector_status validatePainterPath(nector_painter_handle painter, nector_path_handle path) {
    if (painter == nullptr || painter->surface == nullptr || path == nullptr) {
        return nectorFail(nector_invalid_argument, "painter or path is null");
    }
    return nector_ok;
}
}

nector_status nectorCreateSurface(int32_t width, int32_t height, nector_surface_handle* out_surface) {
    if (out_surface == nullptr) return nectorFail(nector_invalid_argument, "out_surface is null");
    *out_surface = nullptr;
    if (width <= 0 || height <= 0) return nectorFail(nector_invalid_argument, "surface dimensions must be positive");
    const size_t pixels = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (pixels > max_surface_pixels || pixels > std::numeric_limits<size_t>::max() / 4) {
        return nectorFail(nector_limit_exceeded, "surface is too large");
    }
    nector_api_try {
        auto* surface = new nector_surface;
        surface->width = width;
        surface->height = height;
        surface->pixels.assign(pixels * 4, 0);
        *out_surface = surface;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorCreateSurfaceFromPixels(int32_t width, int32_t height, const uint8_t* rgba, size_t pixel_size,
                                             nector_surface_handle* out_surface) {
    if (rgba == nullptr) return nectorFail(nector_invalid_argument, "pixel buffer is null");
    nector_surface_handle surface = nullptr;
    nector_status status = nectorCreateSurface(width, height, &surface);
    if (status != nector_ok) return status;
    status = nectorSurfaceWritePixels(surface, rgba, pixel_size);
    if (status != nector_ok) {
        nectorDestroySurface(surface);
        return status;
    }
    *out_surface = surface;
    return nector_ok;
}

void nectorDestroySurface(nector_surface_handle surface) {
    delete surface;
}

nector_status nectorSurfaceGetSize(nector_surface_handle surface, int32_t* out_width, int32_t* out_height) {
    if (surface == nullptr || out_width == nullptr || out_height == nullptr) return nectorFail(nector_invalid_argument, "surface or output is null");
    *out_width = surface->width;
    *out_height = surface->height;
    return nector_ok;
}

nector_status nectorSurfaceClear(nector_surface_handle surface, nector_color color) {
    if (surface == nullptr) return nectorFail(nector_invalid_argument, "surface is null");
    if (!validColor(color)) return nectorFail(nector_invalid_argument, "color is invalid");
    const std::array<uint8_t, 4> value{
        static_cast<uint8_t>(std::lround(color.r * 255)), static_cast<uint8_t>(std::lround(color.g * 255)),
        static_cast<uint8_t>(std::lround(color.b * 255)), static_cast<uint8_t>(std::lround(color.a * 255))};
    for (size_t offset = 0; offset < surface->pixels.size(); offset += 4) {
        std::memcpy(surface->pixels.data() + offset, value.data(), 4);
    }
    return nector_ok;
}

nector_status nectorSurfaceReadPixels(nector_surface_handle surface, uint8_t* out_rgba, size_t pixel_size) {
    if (surface == nullptr || out_rgba == nullptr) return nectorFail(nector_invalid_argument, "surface or output buffer is null");
    if (pixel_size < surface->pixels.size()) return nectorFail(nector_invalid_argument, "pixel output buffer is too small");
    std::memcpy(out_rgba, surface->pixels.data(), surface->pixels.size());
    return nector_ok;
}

nector_status nectorSurfaceWritePixels(nector_surface_handle surface, const uint8_t* rgba, size_t pixel_size) {
    if (surface == nullptr || rgba == nullptr) return nectorFail(nector_invalid_argument, "surface or pixel buffer is null");
    if (pixel_size < surface->pixels.size()) return nectorFail(nector_invalid_argument, "pixel input buffer is too small");
    std::memcpy(surface->pixels.data(), rgba, surface->pixels.size());
    return nector_ok;
}

nector_status nectorCreatePainter(nector_surface_handle surface, nector_painter_handle* out_painter) {
    if (surface == nullptr || out_painter == nullptr) return nectorFail(nector_invalid_argument, "surface or out_painter is null");
    *out_painter = nullptr;
    nector_api_try {
        auto* painter = new nector_painter;
        painter->surface = surface;
        painter->state.clip = {0, 0, static_cast<float>(surface->width), static_cast<float>(surface->height)};
        *out_painter = painter;
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyPainter(nector_painter_handle painter) {
    delete painter;
}

nector_status nectorPainterSave(nector_painter_handle painter) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    nector_api_try {
        if (painter->stack.size() >= 256) return nectorFail(nector_limit_exceeded, "painter state stack limit exceeded");
        painter->stack.push_back(painter->state);
        return nector_ok;
    } nector_api_catch
}

nector_status nectorPainterRestore(nector_painter_handle painter) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    if (painter->stack.empty()) return nectorFail(nector_invalid_state, "painter state stack is empty");
    painter->state = painter->stack.back();
    painter->stack.pop_back();
    return nector_ok;
}

nector_status nectorPainterSetClip(nector_painter_handle painter, nector_rect clip) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    if (!nectorValidRect(clip)) return nectorFail(nector_invalid_argument, "clip is invalid");
    const auto current = painter->state.clip;
    const float x0 = std::max(current.x, clip.x);
    const float y0 = std::max(current.y, clip.y);
    const float x1 = std::min(current.x + current.width, clip.x + clip.width);
    const float y1 = std::min(current.y + current.height, clip.y + clip.height);
    painter->state.clip = {x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
    return nector_ok;
}

nector_status nectorPainterResetClip(nector_painter_handle painter) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    painter->state.clip = {0, 0, static_cast<float>(painter->surface->width), static_cast<float>(painter->surface->height)};
    return nector_ok;
}

nector_status nectorPainterSetTransform(nector_painter_handle painter, nector_transform transform) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    if (!validTransform(transform)) return nectorFail(nector_invalid_argument, "transform is invalid");
    painter->state.transform = transform;
    return nector_ok;
}

nector_status nectorPainterSetFillColor(nector_painter_handle painter, nector_color color) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    if (!validColor(color)) return nectorFail(nector_invalid_argument, "color is invalid");
    painter->state.color = color;
    painter->state.paint_type = nector_painter::PaintType::solid;
    return nector_ok;
}

nector_status nectorPainterSetLinearGradient(nector_painter_handle painter, nector_point start, nector_point end,
                                              nector_color start_color, nector_color end_color) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    if (!validPoint(start.x, start.y) || !validPoint(end.x, end.y) || !validColor(start_color) || !validColor(end_color)) {
        return nectorFail(nector_invalid_argument, "gradient is invalid");
    }
    painter->state.first_point = start;
    painter->state.second_point = end;
    painter->state.color = start_color;
    painter->state.second_color = end_color;
    painter->state.paint_type = nector_painter::PaintType::linear;
    return nector_ok;
}

nector_status nectorPainterSetRadialGradient(nector_painter_handle painter, nector_point center, float radius,
                                              nector_color inner_color, nector_color outer_color) {
    if (painter == nullptr) return nectorFail(nector_invalid_argument, "painter is null");
    if (!validPoint(center.x, center.y) || !nectorFinite(radius) || radius <= 0 || !validColor(inner_color) || !validColor(outer_color)) {
        return nectorFail(nector_invalid_argument, "radial gradient is invalid");
    }
    painter->state.first_point = center;
    painter->state.radius = radius;
    painter->state.color = inner_color;
    painter->state.second_color = outer_color;
    painter->state.paint_type = nector_painter::PaintType::radial;
    return nector_ok;
}

nector_status nectorPainterFillPath(nector_painter_handle painter, nector_path_handle path, nector_fill_rule fill_rule) {
    const nector_status validation = validatePainterPath(painter, path);
    if (validation != nector_ok) return validation;
    if (fill_rule != nector_fill_nonzero && fill_rule != nector_fill_even_odd) return nectorFail(nector_invalid_argument, "fill rule is invalid");
    nector_api_try {
        return rasterizePolylines(painter, flattenedPaths(painter, path, true), fill_rule);
    } nector_api_catch
}

nector_status nectorPainterStrokePath(nector_painter_handle painter, nector_path_handle path, float width,
                                       nector_line_cap cap, nector_line_join join) {
    const nector_stroke_style style{width, cap, join, 4.0f, 0.0f, nullptr, 0};
    return nectorPainterStrokePathEx(painter, path, &style);
}

nector_status nectorPainterStrokePathEx(nector_painter_handle painter, nector_path_handle path,
                                         const nector_stroke_style* style) {
    const nector_status validation = validatePainterPath(painter, path);
    if (validation != nector_ok) return validation;
    if (style == nullptr || !nectorFinite(style->width) || style->width <= 0 || style->width > 10000 ||
        !nectorFinite(style->miter_limit) || style->miter_limit < 1.0f || style->miter_limit > 1000.0f ||
        !nectorFinite(style->dash_offset) || std::fabs(style->dash_offset) > 1.0e7f ||
        style->cap < nector_cap_butt || style->cap > nector_cap_square ||
        style->join < nector_join_miter || style->join > nector_join_bevel || style->dash_count > 64 ||
        (style->dash_count != 0 && style->dash_pattern == nullptr)) {
        return nectorFail(nector_invalid_argument, "stroke style is invalid");
    }
    double dash_total = 0;
    for (size_t index = 0; index < style->dash_count; ++index) {
        if (!nectorFinite(style->dash_pattern[index]) || style->dash_pattern[index] <= 0 || style->dash_pattern[index] > 1.0e7f) {
            return nectorFail(nector_invalid_argument, "dash pattern is invalid");
        }
        dash_total += style->dash_pattern[index];
    }
    if (dash_total > 1.0e8) return nectorFail(nector_invalid_argument, "dash pattern is too large");
    nector_api_try {
        return rasterizePolylines(painter, strokeOutlines(painter, path, *style), nector_fill_nonzero);
    } nector_api_catch
}

nector_status nectorPainterFillRectangle(nector_painter_handle painter, nector_rect rect) {
    if (!nectorValidRect(rect)) return nectorFail(nector_invalid_argument, "rectangle is invalid");
    nector_path_handle path = nullptr;
    nector_status status = nectorCreatePath(&path);
    if (status != nector_ok) return status;
    status = nectorPathAddRectangle(path, rect);
    if (status == nector_ok) status = nectorPainterFillPath(painter, path, nector_fill_nonzero);
    nectorDestroyPath(path);
    return status;
}

nector_status nectorPainterDrawLine(nector_painter_handle painter, nector_point start, nector_point end, float width) {
    nector_path_handle path = nullptr;
    nector_status status = nectorCreatePath(&path);
    if (status != nector_ok) return status;
    status = nectorPathMoveTo(path, start.x, start.y);
    if (status == nector_ok) status = nectorPathLineTo(path, end.x, end.y);
    if (status == nector_ok) status = nectorPainterStrokePath(painter, path, width, nector_cap_round, nector_join_round);
    nectorDestroyPath(path);
    return status;
}

nector_status nectorPainterFillEllipse(nector_painter_handle painter, float cx, float cy, float rx, float ry) {
    nector_path_handle path = nullptr;
    nector_status status = nectorCreatePath(&path);
    if (status != nector_ok) return status;
    status = nectorPathAddEllipse(path, cx, cy, rx, ry);
    if (status == nector_ok) status = nectorPainterFillPath(painter, path, nector_fill_nonzero);
    nectorDestroyPath(path);
    return status;
}

nector_status nectorPainterDrawMask(nector_painter_handle painter, int32_t x, int32_t y, int32_t width, int32_t height,
                                     const uint8_t* alpha, size_t alpha_size, int32_t pitch, nector_color color) {
    if (painter == nullptr || alpha == nullptr) return nectorFail(nector_invalid_argument, "painter or alpha buffer is null");
    if (width < 0 || height < 0 || pitch < width || (height > 0 && static_cast<size_t>(pitch) * height > alpha_size)) {
        return nectorFail(nector_invalid_argument, "mask dimensions or pitch are invalid");
    }
    if (!validColor(color)) return nectorFail(nector_invalid_argument, "mask color is invalid");
    const Bounds clip = clipBounds(painter);
    for (int row = 0; row < height; ++row) {
        const int destination_y = y + row;
        if (destination_y < clip.y0 || destination_y >= clip.y1) continue;
        for (int column = 0; column < width; ++column) {
            const int destination_x = x + column;
            if (destination_x < clip.x0 || destination_x >= clip.x1) continue;
            nector_color source = color;
            source.a *= alpha[static_cast<size_t>(row) * pitch + column] / 255.0f;
            blendPixel(painter->surface, destination_x, destination_y, source);
        }
    }
    return nector_ok;
}

nector_status nectorPainterDrawSurface(nector_painter_handle painter, nector_surface_handle source, nector_rect destination,
                                        nector_rect source_rect, float opacity) {
    if (painter == nullptr || source == nullptr) return nectorFail(nector_invalid_argument, "painter or source surface is null");
    if (!nectorValidRect(destination) || !nectorValidRect(source_rect) || !nectorFinite(opacity) || opacity < 0 || opacity > 1) {
        return nectorFail(nector_invalid_argument, "surface draw arguments are invalid");
    }
    if (source_rect.x < 0 || source_rect.y < 0 || source_rect.x + source_rect.width > source->width ||
        source_rect.y + source_rect.height > source->height) return nectorFail(nector_invalid_argument, "source rectangle is outside the surface");
    if (destination.width == 0 || destination.height == 0 || source_rect.width == 0 || source_rect.height == 0 || opacity == 0) return nector_ok;
    nector_transform inverse;
    if (!invertTransform(painter->state.transform, &inverse)) return nector_ok;
    const nector_point corners[4] = {
        transformPoint(painter->state.transform, {destination.x, destination.y}),
        transformPoint(painter->state.transform, {destination.x + destination.width, destination.y}),
        transformPoint(painter->state.transform, {destination.x + destination.width, destination.y + destination.height}),
        transformPoint(painter->state.transform, {destination.x, destination.y + destination.height})};
    float min_x = corners[0].x, max_x = corners[0].x, min_y = corners[0].y, max_y = corners[0].y;
    for (int index = 1; index < 4; ++index) {
        min_x = std::min(min_x, corners[index].x); max_x = std::max(max_x, corners[index].x);
        min_y = std::min(min_y, corners[index].y); max_y = std::max(max_y, corners[index].y);
    }
    const Bounds clip = clipBounds(painter);
    const int x0 = std::max(clip.x0, static_cast<int>(std::floor(min_x)));
    const int y0 = std::max(clip.y0, static_cast<int>(std::floor(min_y)));
    const int x1 = std::min(clip.x1, static_cast<int>(std::ceil(max_x)));
    const int y1 = std::min(clip.y1, static_cast<int>(std::ceil(max_y)));
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const nector_point local = transformPoint(inverse, {x + 0.5f, y + 0.5f});
            const float u = (local.x - destination.x) / destination.width;
            const float v = (local.y - destination.y) / destination.height;
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            const float source_x = source_rect.x + u * source_rect.width - 0.5f;
            const float source_y = source_rect.y + v * source_rect.height - 0.5f;
            const int left = std::max(0, std::min(source->width - 1, static_cast<int>(std::floor(source_x))));
            const int top = std::max(0, std::min(source->height - 1, static_cast<int>(std::floor(source_y))));
            const int right = std::min(source->width - 1, left + 1);
            const int bottom = std::min(source->height - 1, top + 1);
            const float tx = std::max(0.0f, std::min(1.0f, source_x - std::floor(source_x)));
            const float ty = std::max(0.0f, std::min(1.0f, source_y - std::floor(source_y)));
            nector_color samples[4]{};
            const int positions[4][2] = {{left, top}, {right, top}, {left, bottom}, {right, bottom}};
            for (int index = 0; index < 4; ++index) {
                const size_t offset = (static_cast<size_t>(positions[index][1]) * source->width + positions[index][0]) * 4;
                samples[index] = {source->pixels[offset] / 255.0f, source->pixels[offset + 1] / 255.0f,
                                  source->pixels[offset + 2] / 255.0f, source->pixels[offset + 3] / 255.0f};
            }
            auto interpolate = [&](float nector_color::*channel) {
                const float top_value = samples[0].*channel * (1 - tx) + samples[1].*channel * tx;
                const float bottom_value = samples[2].*channel * (1 - tx) + samples[3].*channel * tx;
                return top_value * (1 - ty) + bottom_value * ty;
            };
            nector_color color{interpolate(&nector_color::r), interpolate(&nector_color::g),
                               interpolate(&nector_color::b), interpolate(&nector_color::a) * opacity};
            blendPixel(painter->surface, x, y, color);
        }
    }
    return nector_ok;
}
