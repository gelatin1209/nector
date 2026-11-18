#include "svg_internal.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace nector_image_internal {
namespace {

constexpr double pi = 3.14159265358979323846;

struct Matrix {
    double a = 1;
    double b = 0;
    double c = 0;
    double d = 1;
    double e = 0;
    double f = 0;
};

struct PathDeleter {
    void operator()(nector_path_handle path) const { nectorDestroyPath(path); }
};
using PathPtr = std::unique_ptr<nector_path, PathDeleter>;

struct PainterDeleter {
    void operator()(nector_painter_handle painter) const { nectorDestroyPainter(painter); }
};
using PainterPtr = std::unique_ptr<nector_painter, PainterDeleter>;

struct Paint {
    bool enabled = true;
    nector_color color{0, 0, 0, 1};
};

struct Style {
    Paint fill;
    Paint stroke{{false}, {0, 0, 0, 1}};
    double stroke_width = 1;
    double opacity = 1;
    double fill_opacity = 1;
    double stroke_opacity = 1;
    nector_fill_rule fill_rule = nector_fill_nonzero;
    nector_line_cap line_cap = nector_cap_butt;
    nector_line_join line_join = nector_join_miter;
};

std::string_view trim(std::string_view value) {
    size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t' || value[first] == '\r' || value[first] == '\n')) ++first;
    size_t last = value.size();
    while (last > first && (value[last - 1] == ' ' || value[last - 1] == '\t' || value[last - 1] == '\r' || value[last - 1] == '\n')) --last;
    return value.substr(first, last - first);
}

std::string lowercase(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

bool validNumber(double value) {
    return std::isfinite(value) && std::fabs(value) <= 1.0e7;
}

bool validMatrix(const Matrix& value) {
    return validNumber(value.a) && validNumber(value.b) && validNumber(value.c) && validNumber(value.d) &&
           validNumber(value.e) && validNumber(value.f);
}

Matrix multiply(const Matrix& left, const Matrix& right) {
    return {left.a * right.a + left.c * right.b,
            left.b * right.a + left.d * right.b,
            left.a * right.c + left.c * right.d,
            left.b * right.c + left.d * right.d,
            left.a * right.e + left.c * right.f + left.e,
            left.b * right.e + left.d * right.f + left.f};
}

Matrix translation(double x, double y) {
    Matrix result;
    result.e = x;
    result.f = y;
    return result;
}

Matrix scaling(double x, double y) {
    Matrix result;
    result.a = x;
    result.d = y;
    return result;
}

nector_transform rasterTransform(const Matrix& value) {
    return {static_cast<float>(value.a), static_cast<float>(value.c), static_cast<float>(value.e),
            static_cast<float>(value.b), static_cast<float>(value.d), static_cast<float>(value.f)};
}

int hexDigit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool namedColor(std::string_view name, nector_color* color) {
    struct NamedColor { const char* name; uint32_t rgb; };
    static const NamedColor colors[] = {
        {"black", 0x000000}, {"silver", 0xc0c0c0}, {"gray", 0x808080}, {"grey", 0x808080},
        {"white", 0xffffff}, {"maroon", 0x800000}, {"red", 0xff0000}, {"purple", 0x800080},
        {"fuchsia", 0xff00ff}, {"magenta", 0xff00ff}, {"green", 0x008000}, {"lime", 0x00ff00},
        {"olive", 0x808000}, {"yellow", 0xffff00}, {"navy", 0x000080}, {"blue", 0x0000ff},
        {"teal", 0x008080}, {"aqua", 0x00ffff}, {"cyan", 0x00ffff}, {"orange", 0xffa500},
        {"pink", 0xffc0cb}, {"brown", 0xa52a2a}, {"gold", 0xffd700}, {"indigo", 0x4b0082},
        {"violet", 0xee82ee}, {"coral", 0xff7f50}, {"salmon", 0xfa8072}, {"khaki", 0xf0e68c},
        {"turquoise", 0x40e0d0}, {"beige", 0xf5f5dc}, {"ivory", 0xfffff0}, {"lavender", 0xe6e6fa},
        {"plum", 0xdda0dd}, {"orchid", 0xda70d6}, {"crimson", 0xdc143c}, {"chocolate", 0xd2691e},
        {"tan", 0xd2b48c}, {"skyblue", 0x87ceeb}, {"steelblue", 0x4682b4}, {"royalblue", 0x4169e1},
        {"slateblue", 0x6a5acd}, {"seagreen", 0x2e8b57}, {"forestgreen", 0x228b22}, {"darkgreen", 0x006400},
        {"lightgreen", 0x90ee90}, {"darkred", 0x8b0000}, {"darkblue", 0x00008b}, {"lightgray", 0xd3d3d3},
        {"lightgrey", 0xd3d3d3}, {"darkgray", 0xa9a9a9}, {"darkgrey", 0xa9a9a9}, {"rebeccapurple", 0x663399}
    };
    for (const auto& named : colors) {
        if (name == named.name) {
            *color = {static_cast<float>((named.rgb >> 16) & 0xff) / 255.0f,
                      static_cast<float>((named.rgb >> 8) & 0xff) / 255.0f,
                      static_cast<float>(named.rgb & 0xff) / 255.0f, 1};
            return true;
        }
    }
    return false;
}

nector_status parseColor(std::string_view input, nector_color* out_color) {
    const std::string value = lowercase(trim(input));
    if (value == "transparent") {
        *out_color = {0, 0, 0, 0};
        return nector_ok;
    }
    if (namedColor(value, out_color)) return nector_ok;
    if (value.size() >= 4 && value[0] == '#') {
        const size_t digits = value.size() - 1;
        if (digits != 3 && digits != 4 && digits != 6 && digits != 8) {
            return nectorFail(nector_invalid_format, "invalid SVG hexadecimal color");
        }
        std::array<unsigned int, 4> components{0, 0, 0, 255};
        if (digits <= 4) {
            for (size_t index = 0; index < digits; ++index) {
                const int digit = hexDigit(value[index + 1]);
                if (digit < 0) return nectorFail(nector_invalid_format, "invalid SVG hexadecimal color");
                components[index] = static_cast<unsigned int>(digit * 17);
            }
        } else {
            for (size_t index = 0; index < digits / 2; ++index) {
                const int high = hexDigit(value[index * 2 + 1]);
                const int low = hexDigit(value[index * 2 + 2]);
                if (high < 0 || low < 0) return nectorFail(nector_invalid_format, "invalid SVG hexadecimal color");
                components[index] = static_cast<unsigned int>(high * 16 + low);
            }
        }
        *out_color = {components[0] / 255.0f, components[1] / 255.0f, components[2] / 255.0f, components[3] / 255.0f};
        return nector_ok;
    }
    if (value.size() > 5 && value.compare(0, 4, "rgb(") == 0 && value.back() == ')') {
        const std::string_view arguments(value.data() + 4, value.size() - 5);
        const bool comma_mode = arguments.find(',') != std::string_view::npos;
        std::array<double, 3> components{};
        size_t offset = 0;
        auto space = [](char character) { return std::isspace(static_cast<unsigned char>(character)) != 0; };
        for (size_t index = 0; index < 3; ++index) {
            while (offset < arguments.size() && space(arguments[offset])) ++offset;
            size_t end = offset;
            if (comma_mode) {
                end = arguments.find(',', offset);
                if ((index < 2) != (end != std::string_view::npos)) {
                    return nectorFail(nector_invalid_format, "invalid SVG rgb() separators");
                }
            } else {
                while (end < arguments.size() && !space(arguments[end])) ++end;
            }
            const std::string_view token = trim(arguments.substr(offset,
                end == std::string_view::npos ? end : end - offset));
            if (token.empty()) return nectorFail(nector_invalid_format, "invalid SVG rgb() color");
            const bool percentage = token.back() == '%';
            const std::string_view number = percentage ? token.substr(0, token.size() - 1) : token;
            if (!parseSvgNumber(number, &components[index])) {
                return nectorFail(nector_invalid_format, "invalid SVG rgb() component");
            }
            components[index] = percentage ? components[index] * 255.0 / 100.0 : components[index];
            components[index] = std::max(0.0, std::min(255.0, components[index]));
            if (end == std::string_view::npos) {
                offset = arguments.size();
            } else {
                offset = comma_mode ? end + 1 : end;
            }
        }
        while (offset < arguments.size() && space(arguments[offset])) ++offset;
        if (offset != arguments.size()) return nectorFail(nector_invalid_format, "too many SVG rgb() components");
        *out_color = {static_cast<float>(components[0] / 255.0), static_cast<float>(components[1] / 255.0),
                      static_cast<float>(components[2] / 255.0), 1};
        return nector_ok;
    }
    if (value.compare(0, 4, "url(") == 0) return nectorFail(nector_unsupported, "SVG paint servers are unsupported");
    return nectorFail(nector_invalid_format, "unknown SVG color");
}

nector_status parsePaint(std::string_view value, Paint* paint) {
    if (trim(value) == "none") {
        paint->enabled = false;
        return nector_ok;
    }
    if (trim(value) == "inherit") return nector_ok;
    paint->enabled = true;
    return parseColor(value, &paint->color);
}

nector_status parseOpacity(std::string_view input, double* output) {
    std::string_view value = trim(input);
    bool percentage = false;
    if (!value.empty() && value.back() == '%') {
        percentage = true;
        value = value.substr(0, value.size() - 1);
    }
    double number = 0;
    if (!parseSvgNumber(value, &number)) return nectorFail(nector_invalid_format, "invalid SVG opacity");
    if (percentage) number /= 100.0;
    *output = std::max(0.0, std::min(1.0, number));
    return nector_ok;
}

nector_status applyStyle(Style* style, std::string_view name, std::string_view value) {
    if (name == "fill") return parsePaint(value, &style->fill);
    if (name == "stroke") return parsePaint(value, &style->stroke);
    if (name == "stroke-width") {
        double number = 0;
        if (!parseSvgNumber(value, &number) || !validNumber(number) || number < 0) {
            return nectorFail(nector_invalid_format, "invalid SVG stroke-width");
        }
        style->stroke_width = number;
    } else if (name == "opacity") {
        return parseOpacity(value, &style->opacity);
    } else if (name == "fill-opacity") {
        return parseOpacity(value, &style->fill_opacity);
    } else if (name == "stroke-opacity") {
        return parseOpacity(value, &style->stroke_opacity);
    } else if (name == "fill-rule") {
        if (trim(value) == "evenodd") style->fill_rule = nector_fill_even_odd;
        else if (trim(value) == "nonzero") style->fill_rule = nector_fill_nonzero;
        else return nectorFail(nector_invalid_format, "invalid SVG fill-rule");
    } else if (name == "stroke-linecap") {
        if (trim(value) == "butt") style->line_cap = nector_cap_butt;
        else if (trim(value) == "round") style->line_cap = nector_cap_round;
        else if (trim(value) == "square") style->line_cap = nector_cap_square;
        else return nectorFail(nector_invalid_format, "invalid SVG stroke-linecap");
    } else if (name == "stroke-linejoin") {
        if (trim(value) == "miter") style->line_join = nector_join_miter;
        else if (trim(value) == "round") style->line_join = nector_join_round;
        else if (trim(value) == "bevel") style->line_join = nector_join_bevel;
        else return nectorFail(nector_invalid_format, "invalid SVG stroke-linejoin");
    }
    return nector_ok;
}

bool presentationAttribute(std::string_view name) {
    return name == "fill" || name == "stroke" || name == "stroke-width" || name == "opacity" ||
           name == "fill-opacity" || name == "stroke-opacity" || name == "fill-rule" ||
           name == "stroke-linecap" || name == "stroke-linejoin";
}

nector_status deriveStyle(const XmlNode& node, const Style& parent, Style* output) {
    *output = parent;
    const double inherited_opacity = parent.opacity;
    output->opacity = 1;
    for (const auto& attribute : node.attributes) {
        const std::string_view name = xmlLocalName(attribute.first);
        if (presentationAttribute(name)) {
            const nector_status status = applyStyle(output, name, attribute.second);
            if (status != nector_ok) return status;
        }
    }
    const std::string* inline_style = xmlAttribute(node, "style");
    if (inline_style != nullptr) {
        size_t offset = 0;
        while (offset <= inline_style->size()) {
            const size_t end = inline_style->find(';', offset);
            const std::string_view declaration = trim(std::string_view(*inline_style).substr(offset,
                end == std::string::npos ? end : end - offset));
            if (!declaration.empty()) {
                const size_t colon = declaration.find(':');
                if (colon == std::string_view::npos) return nectorFail(nector_invalid_format, "invalid SVG style declaration");
                const nector_status status = applyStyle(output, trim(declaration.substr(0, colon)), trim(declaration.substr(colon + 1)));
                if (status != nector_ok) return status;
            }
            if (end == std::string::npos) break;
            offset = end + 1;
        }
    }
    output->opacity = inherited_opacity * output->opacity;
    return nector_ok;
}

nector_status transformArguments(std::string_view name, std::string_view arguments, Matrix* output) {
    std::vector<double> values;
    SvgNumberScanner scanner(arguments);
    while (scanner.nextIsNumber()) {
        double value = 0;
        if (!scanner.number(&value) || !validNumber(value)) return nectorFail(nector_invalid_format, "invalid SVG transform number");
        values.push_back(value);
        if (values.size() > 6) return nectorFail(nector_invalid_format, "too many SVG transform arguments");
    }
    if (!scanner.atEnd()) return nectorFail(nector_invalid_format, "invalid SVG transform arguments");
    Matrix result;
    auto count = [&values](size_t expected) { return values.size() == expected; };
    if (name == "matrix" && count(6)) {
        result = {values[0], values[1], values[2], values[3], values[4], values[5]};
    } else if (name == "translate" && (count(1) || count(2))) {
        result = translation(values[0], count(2) ? values[1] : 0);
    } else if (name == "scale" && (count(1) || count(2))) {
        result = scaling(values[0], count(2) ? values[1] : values[0]);
    } else if (name == "rotate" && (count(1) || count(3))) {
        const double angle = values[0] * pi / 180.0;
        Matrix rotation;
        rotation.a = std::cos(angle);
        rotation.b = std::sin(angle);
        rotation.c = -rotation.b;
        rotation.d = rotation.a;
        result = rotation;
        if (count(3)) result = multiply(multiply(translation(values[1], values[2]), rotation), translation(-values[1], -values[2]));
    } else if (name == "skewX" && count(1)) {
        result.c = std::tan(values[0] * pi / 180.0);
    } else if (name == "skewY" && count(1)) {
        result.b = std::tan(values[0] * pi / 180.0);
    } else {
        return nectorFail(nector_invalid_format, "invalid or unsupported SVG transform");
    }
    if (!validMatrix(result)) return nectorFail(nector_invalid_format, "non-finite SVG transform");
    *output = result;
    return nector_ok;
}

nector_status parseTransform(std::string_view text, Matrix* output) {
    Matrix result;
    size_t offset = 0;
    bool any = false;
    while (offset < text.size()) {
        while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' ||
               text[offset] == '\n' || text[offset] == ',')) ++offset;
        if (offset == text.size()) break;
        const size_t name_start = offset;
        while (offset < text.size() && ((text[offset] >= 'A' && text[offset] <= 'Z') ||
               (text[offset] >= 'a' && text[offset] <= 'z'))) ++offset;
        if (name_start == offset) return nectorFail(nector_invalid_format, "invalid SVG transform name");
        const std::string_view name = text.substr(name_start, offset - name_start);
        while (offset < text.size() && (text[offset] == ' ' || text[offset] == '\t')) ++offset;
        if (offset >= text.size() || text[offset] != '(') return nectorFail(nector_invalid_format, "SVG transform requires parentheses");
        const size_t close = text.find(')', offset + 1);
        if (close == std::string_view::npos) return nectorFail(nector_invalid_format, "unterminated SVG transform");
        Matrix local;
        const nector_status status = transformArguments(name, text.substr(offset + 1, close - offset - 1), &local);
        if (status != nector_ok) return status;
        result = multiply(result, local);
        if (!validMatrix(result)) return nectorFail(nector_invalid_format, "SVG transform overflow");
        any = true;
        offset = close + 1;
    }
    if (!any && !trim(text).empty()) return nectorFail(nector_invalid_format, "empty SVG transform");
    *output = result;
    return nector_ok;
}

nector_status numberAttribute(const XmlNode& node, std::string_view name, double fallback, double* output, bool* present = nullptr) {
    const std::string* value = xmlAttribute(node, name);
    if (value == nullptr) {
        *output = fallback;
        if (present != nullptr) *present = false;
        return nector_ok;
    }
    if (!parseSvgNumber(*value, output) || !validNumber(*output)) {
        return nectorFail(nector_invalid_format, "invalid SVG numeric attribute");
    }
    if (present != nullptr) *present = true;
    return nector_ok;
}

float coordinate(double value) {
    return static_cast<float>(value);
}

class PathParser {
public:
    PathParser(nector_path_handle path, std::string_view text) : path_(path), scanner_(text) {}

    nector_status parse() {
        if (scanner_.atEnd()) return nector_ok;
        for (;;) {
            char command = 0;
            if (!scanner_.command(&command)) return nectorFail(nector_invalid_format, "SVG path requires a command");
            const bool relative = command >= 'a' && command <= 'z';
            const char upper = relative ? static_cast<char>(command - 'a' + 'A') : command;
            if (upper == 'Z') {
                if (!has_current_) return nectorFail(nector_invalid_format, "SVG close path has no subpath");
                if (subpath_points_ > 1 && !closed_) {
                    const nector_status status = nectorPathClose(path_);
                    if (status != nector_ok) return status;
                }
                current_x_ = start_x_;
                current_y_ = start_y_;
                closed_ = true;
            } else {
                if (upper != 'M' && upper != 'L' && upper != 'H' && upper != 'V' && upper != 'C' && upper != 'Q') {
                    return nectorFail(nector_unsupported, "unsupported SVG path command");
                }
                bool first = true;
                do {
                    const nector_status status = segment(upper, relative, first);
                    if (status != nector_ok) return status;
                    first = false;
                    if (upper == 'M') {
                        const nector_status implicit_status = implicitLines(relative);
                        if (implicit_status != nector_ok) return implicit_status;
                        break;
                    }
                } while (scanner_.nextIsNumber());
            }
            if (scanner_.atEnd()) return nector_ok;
        }
    }

private:
    bool values(double* output, size_t count) {
        for (size_t index = 0; index < count; ++index) {
            if (!scanner_.number(&output[index]) || !validNumber(output[index])) return false;
        }
        return true;
    }

    nector_status prepare() {
        if (!has_current_) return nectorFail(nector_invalid_format, "SVG path segment requires moveto");
        if (closed_) {
            const nector_status status = nectorPathMoveTo(path_, coordinate(current_x_), coordinate(current_y_));
            if (status != nector_ok) return status;
            subpath_points_ = 1;
            closed_ = false;
        }
        return nector_ok;
    }

    nector_status move(double x, double y) {
        const nector_status status = nectorPathMoveTo(path_, coordinate(x), coordinate(y));
        if (status != nector_ok) return status;
        current_x_ = start_x_ = x;
        current_y_ = start_y_ = y;
        has_current_ = true;
        closed_ = false;
        subpath_points_ = 1;
        return nector_ok;
    }

    nector_status line(double x, double y) {
        const nector_status status = prepare();
        if (status != nector_ok) return status;
        const nector_status line_status = nectorPathLineTo(path_, coordinate(x), coordinate(y));
        if (line_status != nector_ok) return line_status;
        current_x_ = x;
        current_y_ = y;
        ++subpath_points_;
        return nector_ok;
    }

    nector_status segment(char command, bool relative, bool first) {
        std::array<double, 6> value{};
        const size_t count = command == 'M' || command == 'L' ? 2 : command == 'H' || command == 'V' ? 1 :
                             command == 'C' ? 6 : 4;
        if (!values(value.data(), count)) return nectorFail(nector_invalid_format, "invalid SVG path arguments");
        const double origin_x = relative && has_current_ ? current_x_ : 0;
        const double origin_y = relative && has_current_ ? current_y_ : 0;
        if (command == 'M') {
            if (!first) return nectorFail(nector_invalid_format, "invalid SVG moveto");
            return move(origin_x + value[0], origin_y + value[1]);
        }
        if (command == 'L') return line(origin_x + value[0], origin_y + value[1]);
        if (command == 'H') return line(origin_x + value[0], current_y_);
        if (command == 'V') return line(current_x_, origin_y + value[0]);
        const nector_status status = prepare();
        if (status != nector_ok) return status;
        nector_status curve_status = nector_ok;
        if (command == 'C') {
            curve_status = nectorPathCubicTo(path_, coordinate(origin_x + value[0]), coordinate(origin_y + value[1]),
                coordinate(origin_x + value[2]), coordinate(origin_y + value[3]),
                coordinate(origin_x + value[4]), coordinate(origin_y + value[5]));
            current_x_ = origin_x + value[4];
            current_y_ = origin_y + value[5];
        } else {
            curve_status = nectorPathQuadraticTo(path_, coordinate(origin_x + value[0]), coordinate(origin_y + value[1]),
                coordinate(origin_x + value[2]), coordinate(origin_y + value[3]));
            current_x_ = origin_x + value[2];
            current_y_ = origin_y + value[3];
        }
        if (curve_status != nector_ok) return curve_status;
        subpath_points_ += command == 'C' ? 3 : 2;
        return nector_ok;
    }

    nector_status implicitLines(bool relative) {
        while (scanner_.nextIsNumber()) {
            const nector_status status = segment('L', relative, false);
            if (status != nector_ok) return status;
        }
        return nector_ok;
    }

    nector_path_handle path_;
    SvgNumberScanner scanner_;
    double current_x_ = 0;
    double current_y_ = 0;
    double start_x_ = 0;
    double start_y_ = 0;
    size_t subpath_points_ = 0;
    bool has_current_ = false;
    bool closed_ = false;
};

nector_status drawPath(nector_painter_handle painter, nector_path_handle path, const Style& style,
                       const Matrix& transform, bool fill_allowed) {
    nector_status status = nectorPainterSetTransform(painter, rasterTransform(transform));
    if (status != nector_ok) return status;
    if (fill_allowed && style.fill.enabled && style.fill.color.a > 0 && style.opacity > 0 && style.fill_opacity > 0) {
        nector_color color = style.fill.color;
        color.a *= static_cast<float>(style.opacity * style.fill_opacity);
        status = nectorPainterSetFillColor(painter, color);
        if (status != nector_ok) return status;
        status = nectorPainterFillPath(painter, path, style.fill_rule);
        if (status != nector_ok) return status;
    }
    if (style.stroke.enabled && style.stroke_width > 0 && style.stroke.color.a > 0 && style.opacity > 0 && style.stroke_opacity > 0) {
        nector_color color = style.stroke.color;
        color.a *= static_cast<float>(style.opacity * style.stroke_opacity);
        status = nectorPainterSetFillColor(painter, color);
        if (status != nector_ok) return status;
        status = nectorPainterStrokePath(painter, path, static_cast<float>(style.stroke_width), style.line_cap, style.line_join);
        if (status != nector_ok) return status;
    }
    return nector_ok;
}

nector_status makeRectangle(const XmlNode& node, nector_path_handle path) {
    double x = 0, y = 0, width = 0, height = 0, rx = 0, ry = 0;
    bool has_rx = false, has_ry = false;
    nector_status status = numberAttribute(node, "x", 0, &x);
    if (status == nector_ok) status = numberAttribute(node, "y", 0, &y);
    if (status == nector_ok) status = numberAttribute(node, "width", 0, &width);
    if (status == nector_ok) status = numberAttribute(node, "height", 0, &height);
    if (status == nector_ok) status = numberAttribute(node, "rx", 0, &rx, &has_rx);
    if (status == nector_ok) status = numberAttribute(node, "ry", 0, &ry, &has_ry);
    if (status != nector_ok) return status;
    if (width < 0 || height < 0 || rx < 0 || ry < 0) return nectorFail(nector_invalid_format, "negative SVG rectangle size");
    if (width == 0 || height == 0) return nector_ok;
    if (!has_rx && has_ry) rx = ry;
    if (!has_ry && has_rx) ry = rx;
    rx = std::min(rx, width * 0.5);
    ry = std::min(ry, height * 0.5);
    if (rx == 0 || ry == 0) return nectorPathAddRectangle(path, {coordinate(x), coordinate(y), coordinate(width), coordinate(height)});
    constexpr double kappa = 0.5522847498307936;
    status = nectorPathMoveTo(path, coordinate(x + rx), coordinate(y));
    if (status == nector_ok) status = nectorPathLineTo(path, coordinate(x + width - rx), coordinate(y));
    if (status == nector_ok) status = nectorPathCubicTo(path, coordinate(x + width - rx + kappa * rx), coordinate(y),
        coordinate(x + width), coordinate(y + ry - kappa * ry), coordinate(x + width), coordinate(y + ry));
    if (status == nector_ok) status = nectorPathLineTo(path, coordinate(x + width), coordinate(y + height - ry));
    if (status == nector_ok) status = nectorPathCubicTo(path, coordinate(x + width), coordinate(y + height - ry + kappa * ry),
        coordinate(x + width - rx + kappa * rx), coordinate(y + height), coordinate(x + width - rx), coordinate(y + height));
    if (status == nector_ok) status = nectorPathLineTo(path, coordinate(x + rx), coordinate(y + height));
    if (status == nector_ok) status = nectorPathCubicTo(path, coordinate(x + rx - kappa * rx), coordinate(y + height),
        coordinate(x), coordinate(y + height - ry + kappa * ry), coordinate(x), coordinate(y + height - ry));
    if (status == nector_ok) status = nectorPathLineTo(path, coordinate(x), coordinate(y + ry));
    if (status == nector_ok) status = nectorPathCubicTo(path, coordinate(x), coordinate(y + ry - kappa * ry),
        coordinate(x + rx - kappa * rx), coordinate(y), coordinate(x + rx), coordinate(y));
    if (status == nector_ok) status = nectorPathClose(path);
    return status;
}

nector_status makePoints(std::string_view text, nector_path_handle path, bool polygon) {
    SvgNumberScanner scanner(text);
    bool first = true;
    size_t count = 0;
    while (scanner.nextIsNumber()) {
        double x = 0, y = 0;
        if (!scanner.number(&x) || !scanner.number(&y) || !validNumber(x) || !validNumber(y)) {
            return nectorFail(nector_invalid_format, "invalid SVG points list");
        }
        nector_status status = first ? nectorPathMoveTo(path, coordinate(x), coordinate(y)) :
                                      nectorPathLineTo(path, coordinate(x), coordinate(y));
        if (status != nector_ok) return status;
        first = false;
        ++count;
    }
    if (!scanner.atEnd()) return nectorFail(nector_invalid_format, "invalid SVG points list");
    if (polygon && count > 1) return nectorPathClose(path);
    return nector_ok;
}

bool ignoredElement(std::string_view name) {
    return name == "defs" || name == "metadata" || name == "title" || name == "desc" || name == "style" ||
           name == "script" || name == "symbol" || name == "clipPath" || name == "mask" || name == "pattern" ||
           name == "linearGradient" || name == "radialGradient" || name == "filter";
}

nector_status renderNode(nector_painter_handle painter, const XmlNode& node, const Style& parent_style,
                         const Matrix& parent_transform, unsigned int depth) {
    if (depth > 128) return nectorFail(nector_limit_exceeded, "SVG render nesting limit exceeded");
    const std::string_view name = xmlLocalName(node.name);
    if (ignoredElement(name)) return nector_ok;
    Style style;
    nector_status status = deriveStyle(node, parent_style, &style);
    if (status != nector_ok) return status;
    Matrix transform = parent_transform;
    const std::string* transform_text = xmlAttribute(node, "transform");
    if (transform_text != nullptr) {
        Matrix local;
        status = parseTransform(*transform_text, &local);
        if (status != nector_ok) return status;
        transform = multiply(transform, local);
        if (!validMatrix(transform)) return nectorFail(nector_invalid_format, "SVG transform overflow");
    }
    if (name == "g" || name == "svg") {
        for (const auto& child : node.children) {
            status = renderNode(painter, *child, style, transform, depth + 1);
            if (status != nector_ok) return status;
        }
        return nector_ok;
    }
    PathPtr path;
    nector_path_handle raw_path = nullptr;
    status = nectorCreatePath(&raw_path);
    if (status != nector_ok) return status;
    path.reset(raw_path);
    bool fill_allowed = true;
    if (name == "path") {
        const std::string* data = xmlAttribute(node, "d");
        if (data == nullptr) return nectorFail(nector_invalid_format, "SVG path lacks d attribute");
        status = PathParser(path.get(), *data).parse();
    } else if (name == "rect") {
        status = makeRectangle(node, path.get());
    } else if (name == "circle" || name == "ellipse") {
        double cx = 0, cy = 0, rx = 0, ry = 0;
        status = numberAttribute(node, "cx", 0, &cx);
        if (status == nector_ok) status = numberAttribute(node, "cy", 0, &cy);
        if (status == nector_ok) status = numberAttribute(node, name == "circle" ? "r" : "rx", 0, &rx);
        if (status == nector_ok && name == "ellipse") status = numberAttribute(node, "ry", 0, &ry);
        if (status == nector_ok && name == "circle") ry = rx;
        if (status != nector_ok) return status;
        if (rx < 0 || ry < 0) return nectorFail(nector_invalid_format, "negative SVG radius");
        if (rx > 0 && ry > 0) status = nectorPathAddEllipse(path.get(), coordinate(cx), coordinate(cy), coordinate(rx), coordinate(ry));
    } else if (name == "line") {
        double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        status = numberAttribute(node, "x1", 0, &x1);
        if (status == nector_ok) status = numberAttribute(node, "y1", 0, &y1);
        if (status == nector_ok) status = numberAttribute(node, "x2", 0, &x2);
        if (status == nector_ok) status = numberAttribute(node, "y2", 0, &y2);
        if (status == nector_ok) status = nectorPathMoveTo(path.get(), coordinate(x1), coordinate(y1));
        if (status == nector_ok) status = nectorPathLineTo(path.get(), coordinate(x2), coordinate(y2));
        fill_allowed = false;
    } else if (name == "polyline" || name == "polygon") {
        const std::string* points = xmlAttribute(node, "points");
        if (points == nullptr) return nectorFail(nector_invalid_format, "SVG polygon lacks points");
        status = makePoints(*points, path.get(), name == "polygon");
    } else {
        return nector_ok;
    }
    if (status != nector_ok) return status;
    return drawPath(painter, path.get(), style, transform, fill_allowed);
}

nector_status parseLength(std::string_view input, double* output, bool* percentage) {
    std::string_view value = trim(input);
    *percentage = false;
    if (!value.empty() && value.back() == '%') {
        *percentage = true;
        value = value.substr(0, value.size() - 1);
    } else if (value.size() >= 2 && value.substr(value.size() - 2) == "px") {
        value = value.substr(0, value.size() - 2);
    }
    if (!parseSvgNumber(value, output) || !validNumber(*output) || *output < 0) {
        return nectorFail(nector_invalid_format, "invalid SVG length");
    }
    return nector_ok;
}

struct Viewport {
    int32_t width = 0;
    int32_t height = 0;
    Matrix transform;
};

nector_status viewport(const XmlNode& root, int32_t output_width, int32_t output_height, Viewport* output) {
    double view_x = 0, view_y = 0, view_width = 0, view_height = 0;
    bool has_view_box = false;
    const std::string* view_box = xmlAttribute(root, "viewBox");
    if (view_box != nullptr) {
        SvgNumberScanner scanner(*view_box);
        double* values[] = {&view_x, &view_y, &view_width, &view_height};
        for (double* value : values) {
            if (!scanner.number(value) || !validNumber(*value)) return nectorFail(nector_invalid_format, "invalid SVG viewBox");
        }
        if (!scanner.atEnd() || view_width <= 0 || view_height <= 0) return nectorFail(nector_invalid_format, "invalid SVG viewBox");
        has_view_box = true;
    }
    double root_width = 0, root_height = 0;
    bool width_percent = false, height_percent = false;
    const std::string* width_text = xmlAttribute(root, "width");
    const std::string* height_text = xmlAttribute(root, "height");
    nector_status status = nector_ok;
    if (width_text != nullptr) status = parseLength(*width_text, &root_width, &width_percent);
    if (status == nector_ok && height_text != nullptr) status = parseLength(*height_text, &root_height, &height_percent);
    if (status != nector_ok) return status;
    const bool has_width = width_text != nullptr && !width_percent && root_width > 0;
    const bool has_height = height_text != nullptr && !height_percent && root_height > 0;
    if (has_view_box) {
        if (!has_width && !has_height) {
            root_width = view_width;
            root_height = view_height;
        } else if (!has_width) {
            root_width = root_height * view_width / view_height;
        } else if (!has_height) {
            root_height = root_width * view_height / view_width;
        }
    } else {
        if (!has_width) root_width = has_height ? root_height : 300;
        if (!has_height) root_height = has_width ? root_width : 150;
        if (width_text == nullptr && height_text == nullptr) {
            root_width = 300;
            root_height = 150;
        }
        view_width = root_width;
        view_height = root_height;
    }
    if (!validNumber(root_width) || !validNumber(root_height) || root_width <= 0 || root_height <= 0) {
        return nectorFail(nector_invalid_format, "invalid SVG viewport dimensions");
    }
    double target_width = output_width > 0 ? output_width : root_width;
    double target_height = output_height > 0 ? output_height : root_height;
    if (target_width < 1 || target_height < 1 || target_width > std::numeric_limits<int32_t>::max() ||
        target_height > std::numeric_limits<int32_t>::max()) {
        return nectorFail(nector_limit_exceeded, "SVG output dimensions are too large");
    }
    output->width = static_cast<int32_t>(std::llround(target_width));
    output->height = static_cast<int32_t>(std::llround(target_height));
    size_t pixel_size = 0;
    if (!pixelBufferSize(output->width, output->height, &pixel_size)) return nectorFail(nector_limit_exceeded, "SVG output is too large");
    const double device_width = static_cast<double>(output->width);
    const double device_height = static_cast<double>(output->height);
    double scale_x = device_width / view_width;
    double scale_y = device_height / view_height;
    double offset_x = 0, offset_y = 0;
    const std::string* aspect_text = xmlAttribute(root, "preserveAspectRatio");
    const std::string aspect = aspect_text == nullptr ? "xMidYMid meet" : *aspect_text;
    if (aspect.find("none") == std::string::npos) {
        const bool slice = aspect.find("slice") != std::string::npos;
        const double scale = slice ? std::max(scale_x, scale_y) : std::min(scale_x, scale_y);
        scale_x = scale_y = scale;
        const double extra_x = device_width - view_width * scale;
        const double extra_y = device_height - view_height * scale;
        offset_x = aspect.find("xMax") != std::string::npos ? extra_x : aspect.find("xMid") != std::string::npos ? extra_x * 0.5 : 0;
        offset_y = aspect.find("YMax") != std::string::npos ? extra_y : aspect.find("YMid") != std::string::npos ? extra_y * 0.5 : 0;
    }
    output->transform = multiply(translation(offset_x, offset_y), multiply(scaling(scale_x, scale_y), translation(-view_x, -view_y)));
    if (!validMatrix(output->transform)) return nectorFail(nector_invalid_format, "SVG viewport transform overflow");
    return nector_ok;
}

}

nector_status decodeSvg(const uint8_t* data, size_t size, int32_t output_width, int32_t output_height,
                        DecodedImage* out_image) {
    if (data == nullptr || out_image == nullptr) return nectorFail(nector_invalid_argument, "SVG input is null");
    if (size == 0 || size > max_svg_size) return nectorFail(nector_limit_exceeded, "SVG input size is invalid or too large");
    const nector_status utf8_status = nectorCoreValidateUtf8(reinterpret_cast<const char*>(data), size);
    if (utf8_status != nector_ok) return utf8_status;
    std::unique_ptr<XmlNode> root;
    nector_status status = parseXml(std::string_view(reinterpret_cast<const char*>(data), size), &root);
    if (status != nector_ok) return status;
    if (xmlLocalName(root->name) != "svg") return nectorFail(nector_invalid_format, "XML root is not SVG");
    Viewport view;
    status = viewport(*root, output_width, output_height, &view);
    if (status != nector_ok) return status;
    SurfacePtr surface;
    nector_surface_handle raw_surface = nullptr;
    status = nectorCreateSurface(view.width, view.height, &raw_surface);
    if (status != nector_ok) return status;
    surface.reset(raw_surface);
    PainterPtr painter;
    nector_painter_handle raw_painter = nullptr;
    status = nectorCreatePainter(surface.get(), &raw_painter);
    if (status != nector_ok) return status;
    painter.reset(raw_painter);
    Style initial_style;
    status = renderNode(painter.get(), *root, initial_style, view.transform, 0);
    if (status != nector_ok) return status;
    DecodedImage decoded;
    decoded.width = view.width;
    decoded.height = view.height;
    size_t pixel_size = 0;
    if (!pixelBufferSize(view.width, view.height, &pixel_size)) return nectorFail(nector_limit_exceeded, "SVG output is too large");
    decoded.pixels.resize(pixel_size);
    status = nectorSurfaceReadPixels(surface.get(), decoded.pixels.data(), decoded.pixels.size());
    if (status != nector_ok) return status;
    *out_image = std::move(decoded);
    return nector_ok;
}

}
