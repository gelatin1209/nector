#include "compiler.h"
#include "package_format.h"
#include "core.h"
#include "scene.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nector_cdsl_detail {
namespace {

enum class TokenKind { identifier, number, string, color, symbol, end };

struct Token {
    TokenKind kind = TokenKind::end;
    std::string text;
    double number = 0;
    int line = 1;
    int column = 1;
};

class Lexer {
public:
    Lexer(const char* source) : source_(source), size_(std::strlen(source)) {}

    bool next(Token& token, std::string& error) {
        if (!skipSpace(error)) return false;
        token.line = line_;
        token.column = column_;
        token.text.clear();
        token.number = 0;
        if (cursor_ >= size_) {
            token.kind = TokenKind::end;
            return true;
        }
        const char value = source_[cursor_];
        if (value == '"' || value == '\'') return stringToken(token, error);
        if (value == '#') return colorToken(token, error);
        if (isIdentifierStart(value)) {
            token.kind = TokenKind::identifier;
            const size_t begin = cursor_;
            while (cursor_ < size_ && isIdentifierPart(source_[cursor_])) advance();
            token.text.assign(source_ + begin, cursor_ - begin);
            return true;
        }
        if (value == '+' || value == '-' || value == '.' || (value >= '0' && value <= '9')) {
            if (value == '.' && (cursor_ + 1 >= size_ || source_[cursor_ + 1] < '0' || source_[cursor_ + 1] > '9')) {
                token.kind = TokenKind::symbol;
                token.text = ".";
                advance();
                return true;
            }
            return numberToken(token, error);
        }
        if (std::strchr("{}=;.,", value) != nullptr) {
            token.kind = TokenKind::symbol;
            token.text.assign(1, value);
            advance();
            return true;
        }
        fail(error, "unexpected character", token.line, token.column);
        return false;
    }

private:
    static bool isIdentifierStart(char value) {
        return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
    }
    static bool isIdentifierPart(char value) {
        return isIdentifierStart(value) || (value >= '0' && value <= '9');
    }
    static void fail(std::string& error, const std::string& message, int line, int column) {
        error = "line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + message;
    }
    void advance() {
        if (source_[cursor_] == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
        ++cursor_;
    }
    bool skipSpace(std::string& error) {
        while (cursor_ < size_) {
            const char value = source_[cursor_];
            if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
                advance();
                continue;
            }
            if (value == '/' && cursor_ + 1 < size_ && source_[cursor_ + 1] == '/') {
                while (cursor_ < size_ && source_[cursor_] != '\n') advance();
                continue;
            }
            if (value == '/' && cursor_ + 1 < size_ && source_[cursor_ + 1] == '*') {
                const int start_line = line_;
                const int start_column = column_;
                advance();
                advance();
                bool closed = false;
                while (cursor_ < size_) {
                    if (source_[cursor_] == '*' && cursor_ + 1 < size_ && source_[cursor_ + 1] == '/') {
                        advance();
                        advance();
                        closed = true;
                        break;
                    }
                    advance();
                }
                if (!closed) {
                    fail(error, "unterminated block comment", start_line, start_column);
                    return false;
                }
                continue;
            }
            break;
        }
        return true;
    }
    static void appendUtf8(std::string& output, uint32_t codepoint) {
        if (codepoint <= 0x7f) output.push_back(static_cast<char>(codepoint));
        else if (codepoint <= 0x7ff) {
            output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }
    bool readHex(uint32_t& value, int digits) {
        value = 0;
        for (int index = 0; index < digits; ++index) {
            if (cursor_ >= size_) return false;
            const char digit = source_[cursor_];
            uint32_t part = 0;
            if (digit >= '0' && digit <= '9') part = static_cast<uint32_t>(digit - '0');
            else if (digit >= 'a' && digit <= 'f') part = static_cast<uint32_t>(digit - 'a' + 10);
            else if (digit >= 'A' && digit <= 'F') part = static_cast<uint32_t>(digit - 'A' + 10);
            else return false;
            value = (value << 4) | part;
            advance();
        }
        return true;
    }
    bool stringToken(Token& token, std::string& error) {
        const char quote = source_[cursor_];
        advance();
        token.kind = TokenKind::string;
        while (cursor_ < size_) {
            char value = source_[cursor_];
            if (value == quote) {
                advance();
                return true;
            }
            if (value == '\n' || value == '\r') {
                fail(error, "newline in string literal", token.line, token.column);
                return false;
            }
            if (value != '\\') {
                token.text.push_back(value);
                advance();
                continue;
            }
            advance();
            if (cursor_ >= size_) break;
            value = source_[cursor_];
            advance();
            switch (value) {
                case 'n': token.text.push_back('\n'); break;
                case 'r': token.text.push_back('\r'); break;
                case 't': token.text.push_back('\t'); break;
                case '\\': token.text.push_back('\\'); break;
                case '\'': token.text.push_back('\''); break;
                case '"': token.text.push_back('"'); break;
                case 'u': {
                    uint32_t codepoint = 0;
                    if (!readHex(codepoint, 4) || (codepoint >= 0xdc00 && codepoint <= 0xdfff)) {
                        fail(error, "invalid Unicode escape", token.line, token.column);
                        return false;
                    }
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (cursor_ + 2 > size_ || source_[cursor_] != '\\' || source_[cursor_ + 1] != 'u') {
                            fail(error, "missing low surrogate", token.line, token.column);
                            return false;
                        }
                        advance();
                        advance();
                        uint32_t low = 0;
                        if (!readHex(low, 4) || low < 0xdc00 || low > 0xdfff) {
                            fail(error, "invalid low surrogate", token.line, token.column);
                            return false;
                        }
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                    }
                    appendUtf8(token.text, codepoint);
                    break;
                }
                default:
                    fail(error, "unknown string escape", line_, column_ - 1);
                    return false;
            }
        }
        fail(error, "unterminated string literal", token.line, token.column);
        return false;
    }
    bool colorToken(Token& token, std::string& error) {
        token.kind = TokenKind::color;
        token.text.push_back('#');
        advance();
        while (cursor_ < size_) {
            const char value = source_[cursor_];
            if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F'))) break;
            token.text.push_back(value);
            advance();
        }
        if (token.text.size() != 4 && token.text.size() != 5 && token.text.size() != 7 && token.text.size() != 9) {
            fail(error, "colors use #rgb, #rgba, #rrggbb, or #rrggbbaa", token.line, token.column);
            return false;
        }
        return true;
    }
    bool numberToken(Token& token, std::string& error) {
        const size_t begin = cursor_;
        if (source_[cursor_] == '+' || source_[cursor_] == '-') advance();
        bool digits = false;
        while (cursor_ < size_ && source_[cursor_] >= '0' && source_[cursor_] <= '9') {
            digits = true;
            advance();
        }
        if (cursor_ < size_ && source_[cursor_] == '.') {
            advance();
            while (cursor_ < size_ && source_[cursor_] >= '0' && source_[cursor_] <= '9') {
                digits = true;
                advance();
            }
        }
        if (!digits) {
            fail(error, "invalid number", token.line, token.column);
            return false;
        }
        if (cursor_ < size_ && (source_[cursor_] == 'e' || source_[cursor_] == 'E')) {
            advance();
            if (cursor_ < size_ && (source_[cursor_] == '+' || source_[cursor_] == '-')) advance();
            const size_t exponent_begin = cursor_;
            while (cursor_ < size_ && source_[cursor_] >= '0' && source_[cursor_] <= '9') advance();
            if (exponent_begin == cursor_) {
                fail(error, "invalid number exponent", token.line, token.column);
                return false;
            }
        }
        token.text.assign(source_ + begin, cursor_ - begin);
        errno = 0;
        char* end = nullptr;
        token.number = std::strtod(token.text.c_str(), &end);
        if (errno == ERANGE || end != token.text.c_str() + token.text.size() || !std::isfinite(token.number)) {
            fail(error, "number is outside the finite range", token.line, token.column);
            return false;
        }
        token.kind = TokenKind::number;
        return true;
    }

    const char* source_;
    size_t size_;
    size_t cursor_ = 0;
    int line_ = 1;
    int column_ = 1;
};

struct Value {
    ValueType type = ValueType::number;
    std::string string;
    double number = 0;
    bool boolean = false;
    std::array<float, 4> color{0, 0, 0, 1};
};

struct Property {
    std::string name;
    Value value;
    int line = 1;
};

struct Node {
    std::string type;
    std::string id;
    int parent = -1;
    int depth = 0;
    std::vector<Property> properties;
};

struct SetOperation {
    std::string node;
    Property property;
};

struct ActionOperation {
    bool emit = false;
    SetOperation set;
    std::string name;
    std::string text;
    double value = 0;
};

struct Action {
    std::string id;
    std::vector<ActionOperation> operations;
};

class Parser {
public:
    Parser(const char* source) : lexer_(source) {}

    bool parse(std::vector<Node>& nodes, std::vector<Action>& actions, std::string& error) {
        if (!advance(error)) return false;
        while (current_.kind != TokenKind::end) {
            if (current_.kind != TokenKind::identifier) return fail(error, "expected a node or action declaration");
            if (current_.text == "action") {
                if (!parseAction(actions, error)) return false;
            } else {
                if (!parseNode(nodes, -1, 0, error)) return false;
            }
            if (nodes.size() > max_node_count || actions.size() > max_action_count) {
                return fail(error, "declaration count exceeds package limit");
            }
        }
        return true;
    }

private:
    bool advance(std::string& error) { return lexer_.next(current_, error); }
    bool fail(std::string& error, const std::string& message) {
        error = "line " + std::to_string(current_.line) + ", column " + std::to_string(current_.column) + ": " + message;
        return false;
    }
    bool symbol(const char* value) const { return current_.kind == TokenKind::symbol && current_.text == value; }
    bool consumeSymbol(const char* value, std::string& error) {
        if (!symbol(value)) return fail(error, std::string("expected '") + value + "'");
        return advance(error);
    }
    bool parseNode(std::vector<Node>& nodes, int parent, int depth, std::string& error) {
        if (depth > 256) return fail(error, "node nesting exceeds 256 levels");
        Node node;
        node.type = current_.text;
        node.parent = parent;
        node.depth = depth;
        if (!advance(error)) return false;
        if (current_.kind != TokenKind::string || current_.text.empty()) return fail(error, "node ID must be a nonempty string");
        node.id = current_.text;
        if (!advance(error)) return false;
        if (!consumeSymbol("{", error)) return false;
        if (nodes.size() >= max_node_count) return fail(error, "node count exceeds package limit");
        const size_t node_index = nodes.size();
        nodes.push_back(std::move(node));
        while (!symbol("}")) {
            if (current_.kind == TokenKind::end) return fail(error, "unterminated node body");
            if (current_.kind != TokenKind::identifier) return fail(error, "expected a property or child node");
            const std::string name = current_.text;
            const int property_line = current_.line;
            if (!advance(error)) return false;
            if (symbol("{") || current_.kind == TokenKind::string) {
                if (current_.kind != TokenKind::string) return fail(error, "child node ID must be a string");
                if (!parseChild(nodes, name, static_cast<int>(node_index), depth + 1, error)) return false;
            } else {
                if (!parseProperty(nodes[node_index], name, property_line, error)) return false;
            }
        }
        return advance(error);
    }
    bool parseChild(std::vector<Node>& nodes, const std::string& type, int parent, int depth, std::string& error) {
        if (depth > 256) return fail(error, "node nesting exceeds 256 levels");
        Node node;
        node.type = type;
        node.parent = parent;
        node.depth = depth;
        node.id = current_.text;
        if (node.id.empty()) return fail(error, "node ID must be nonempty");
        if (!advance(error)) return false;
        if (!consumeSymbol("{", error)) return false;
        if (nodes.size() >= max_node_count) return fail(error, "node count exceeds package limit");
        const size_t node_index = nodes.size();
        nodes.push_back(std::move(node));
        while (!symbol("}")) {
            if (current_.kind == TokenKind::end) return fail(error, "unterminated node body");
            if (current_.kind != TokenKind::identifier) return fail(error, "expected a property or child node");
            const std::string name = current_.text;
            const int property_line = current_.line;
            if (!advance(error)) return false;
            if (current_.kind == TokenKind::string) {
                if (!parseChild(nodes, name, static_cast<int>(node_index), depth + 1, error)) return false;
            } else {
                if (!parseProperty(nodes[node_index], name, property_line, error)) return false;
            }
        }
        return advance(error);
    }
    bool parseProperty(Node& node, const std::string& name, int line, std::string& error) {
        if (!consumeSymbol("=", error)) return false;
        Property property;
        property.name = name;
        property.line = line;
        if (!parseValue(property.value, error)) return false;
        if (!consumeSymbol(";", error)) return false;
        node.properties.push_back(std::move(property));
        return true;
    }
    bool parseAction(std::vector<Action>& actions, std::string& error) {
        if (!advance(error)) return false;
        if (current_.kind != TokenKind::string || current_.text.empty()) return fail(error, "action ID must be a nonempty string");
        Action action;
        action.id = current_.text;
        if (!advance(error)) return false;
        if (!consumeSymbol("{", error)) return false;
        while (!symbol("}")) {
            if (current_.kind != TokenKind::identifier) return fail(error, "expected set or emit in action");
            ActionOperation operation;
            if (current_.text == "set") {
                if (!advance(error)) return false;
                if (current_.kind != TokenKind::string || current_.text.empty()) return fail(error, "set target must be a node ID string");
                operation.set.node = current_.text;
                if (!advance(error) || !consumeSymbol(".", error)) return false;
                if (current_.kind != TokenKind::identifier) return fail(error, "expected target property name");
                operation.set.property.name = current_.text;
                operation.set.property.line = current_.line;
                if (!advance(error) || !consumeSymbol("=", error)) return false;
                if (!parseValue(operation.set.property.value, error)) return false;
                if (!consumeSymbol(";", error)) return false;
            } else if (current_.text == "emit") {
                operation.emit = true;
                if (!advance(error)) return false;
                if (current_.kind != TokenKind::string || current_.text.empty()) return fail(error, "emit name must be a nonempty string");
                operation.name = current_.text;
                if (!advance(error)) return false;
                if (symbol(",")) {
                    if (!advance(error)) return false;
                    if (current_.kind != TokenKind::string) return fail(error, "emit text must be a string");
                    operation.text = current_.text;
                    if (!advance(error)) return false;
                    if (symbol(",")) {
                        if (!advance(error)) return false;
                        if (current_.kind != TokenKind::number) return fail(error, "emit value must be a number");
                        operation.value = current_.number;
                        if (!advance(error)) return false;
                    }
                }
                if (!consumeSymbol(";", error)) return false;
            } else {
                return fail(error, "only set and emit are valid action operations");
            }
            action.operations.push_back(std::move(operation));
            ++total_operations_;
            if (action.operations.size() > max_operation_count || total_operations_ > max_operation_count) {
                return fail(error, "action operation limit exceeded");
            }
        }
        if (!advance(error)) return false;
        actions.push_back(std::move(action));
        return true;
    }
    bool parseValue(Value& value, std::string& error) {
        if (current_.kind == TokenKind::string) {
            value.type = ValueType::string;
            value.string = current_.text;
        } else if (current_.kind == TokenKind::number) {
            value.type = ValueType::number;
            value.number = current_.number;
        } else if (current_.kind == TokenKind::color) {
            value.type = ValueType::color;
            if (!parseColor(current_.text, value.color)) return fail(error, "invalid color literal");
        } else if (current_.kind == TokenKind::identifier && (current_.text == "true" || current_.text == "false")) {
            value.type = ValueType::boolean;
            value.boolean = current_.text == "true";
        } else {
            return fail(error, "expected a string, number, boolean, or color value");
        }
        return advance(error);
    }
    static bool parseColor(const std::string& text, std::array<float, 4>& color) {
        auto digit = [](char value) -> int {
            if (value >= '0' && value <= '9') return value - '0';
            if (value >= 'a' && value <= 'f') return value - 'a' + 10;
            if (value >= 'A' && value <= 'F') return value - 'A' + 10;
            return -1;
        };
        std::array<int, 8> values{};
        const size_t digits = text.size() - 1;
        for (size_t index = 0; index < digits; ++index) {
            values[index] = digit(text[index + 1]);
            if (values[index] < 0) return false;
        }
        const size_t channels = digits <= 4 ? digits : digits / 2;
        for (size_t index = 0; index < channels; ++index) {
            const int component = digits <= 4 ? values[index] * 17 : values[index * 2] * 16 + values[index * 2 + 1];
            color[index] = static_cast<float>(component) / 255.0f;
        }
        if (channels == 3) color[3] = 1.0f;
        return true;
    }

    Lexer lexer_;
    Token current_;
    size_t total_operations_ = 0;
};

const std::unordered_map<std::string, nector_node_type>& nodeTypes() {
    static const std::unordered_map<std::string, nector_node_type> values{
        {"window", nector_node_window}, {"column", nector_node_column}, {"row", nector_node_row},
        {"label", nector_node_label}, {"button", nector_node_button}, {"line_edit", nector_node_line_edit},
        {"check_box", nector_node_check_box}, {"slider", nector_node_slider}, {"progress_bar", nector_node_progress_bar},
        {"spacer", nector_node_spacer}, {"canvas", nector_node_canvas}, {"image", nector_node_image}
    };
    return values;
}

const std::unordered_map<std::string, std::pair<nector_node_property, ValueType>>& properties() {
    static const std::unordered_map<std::string, std::pair<nector_node_property, ValueType>> values{
        {"text", {nector_property_text, ValueType::string}}, {"title", {nector_property_title, ValueType::string}},
        {"placeholder", {nector_property_placeholder, ValueType::string}}, {"source", {nector_property_source, ValueType::string}},
        {"width", {nector_property_width, ValueType::number}}, {"height", {nector_property_height, ValueType::number}},
        {"padding", {nector_property_padding, ValueType::number}}, {"gap", {nector_property_gap, ValueType::number}},
        {"flex", {nector_property_flex, ValueType::number}}, {"font_size", {nector_property_font_size, ValueType::number}},
        {"value", {nector_property_value, ValueType::number}}, {"minimum", {nector_property_minimum, ValueType::number}},
        {"maximum", {nector_property_maximum, ValueType::number}}, {"step", {nector_property_step, ValueType::number}},
        {"caret", {nector_property_caret, ValueType::number}}, {"corner_radius", {nector_property_corner_radius, ValueType::number}},
        {"visible", {nector_property_visible, ValueType::boolean}}, {"enabled", {nector_property_enabled, ValueType::boolean}},
        {"checked", {nector_property_checked, ValueType::boolean}}, {"word_wrap", {nector_property_word_wrap, ValueType::boolean}},
        {"background", {nector_property_background, ValueType::color}}, {"foreground", {nector_property_foreground, ValueType::color}},
        {"border", {nector_property_border, ValueType::color}}, {"accent", {nector_property_accent, ValueType::color}}
    };
    return values;
}

const std::unordered_map<std::string, nector_ui_event_type>& events() {
    static const std::unordered_map<std::string, nector_ui_event_type> values{
        {"on_click", nector_ui_click}, {"on_text_changed", nector_ui_text_changed},
        {"on_value_changed", nector_ui_value_changed}, {"on_toggled", nector_ui_toggled},
        {"on_paint", nector_ui_paint}, {"on_focus", nector_ui_focus}, {"on_blur", nector_ui_blur},
        {"on_custom", nector_ui_custom}, {"on_animation_finished", nector_ui_animation_finished}, {"on_quit", nector_ui_quit}
    };
    return values;
}

void appendDouble(std::vector<uint8_t>& output, double value) {
    uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "unexpected double size");
    std::memcpy(&bits, &value, sizeof(bits));
    append64(output, bits);
}

void appendFloat(std::vector<uint8_t>& output, float value) {
    uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "unexpected float size");
    std::memcpy(&bits, &value, sizeof(bits));
    append32(output, bits);
}

void appendProperty(std::vector<uint8_t>& output, nector_node_property property, const Value& value) {
    append16(output, static_cast<uint16_t>(property));
    append16(output, static_cast<uint16_t>(value.type));
    const size_t size_position = output.size();
    append32(output, 0);
    const size_t payload_begin = output.size();
    switch (value.type) {
        case ValueType::string: appendBytes(output, reinterpret_cast<const uint8_t*>(value.string.data()), value.string.size()); break;
        case ValueType::number: appendDouble(output, value.number); break;
        case ValueType::boolean: append32(output, value.boolean ? 1 : 0); break;
        case ValueType::color:
            for (float component : value.color) appendFloat(output, component);
            break;
    }
    const uint32_t payload_size = static_cast<uint32_t>(output.size() - payload_begin);
    for (int byte = 0; byte < 4; ++byte) output[size_position + byte] = static_cast<uint8_t>(payload_size >> (byte * 8));
}

bool validateUtf8(const std::string& value, const std::string& description, std::string& error) {
    if (nectorCoreValidateUtf8(value.data(), value.size()) != nector_ok) {
        error = description + " contains invalid UTF-8";
        return false;
    }
    if (value.size() > 16u * 1024u * 1024u) {
        error = description + " exceeds string size limit";
        return false;
    }
    return true;
}

bool buildData(const std::vector<Node>& nodes, std::vector<uint8_t>& output, std::string& error) {
    struct Binding { uint32_t node; nector_ui_event_type event; std::string action; };
    std::vector<Binding> bindings;
    std::unordered_set<std::string> ids;
    std::unordered_set<std::string> binding_keys;
    int roots = 0;
    for (size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        const auto type = nodeTypes().find(node.type);
        if (type == nodeTypes().end()) {
            error = "unknown node type '" + node.type + "'";
            return false;
        }
        if (!validateUtf8(node.id, "node ID", error) || !ids.insert(node.id).second) {
            if (error.empty()) error = "duplicate node ID '" + node.id + "'";
            return false;
        }
        if (node.parent < 0) {
            ++roots;
            if (node.type != "window") {
                error = "root node must be a window";
                return false;
            }
        } else if (node.type == "window") {
            error = "window nodes cannot be nested";
            return false;
        }
        std::unordered_set<std::string> seen;
        for (const auto& property : node.properties) {
            if (!seen.insert(property.name).second) {
                error = "duplicate property '" + property.name + "' on node '" + node.id + "'";
                return false;
            }
            const auto event = events().find(property.name);
            if (event != events().end()) {
                if (property.value.type != ValueType::string || property.value.string.empty()) {
                    error = "event binding '" + property.name + "' must be an action ID string";
                    return false;
                }
                const std::string key = std::to_string(index) + ":" + std::to_string(static_cast<int>(event->second));
                if (!binding_keys.insert(key).second) {
                    error = "duplicate event binding";
                    return false;
                }
                bindings.push_back({static_cast<uint32_t>(index), event->second, property.value.string});
                continue;
            }
            const auto definition = properties().find(property.name);
            if (definition == properties().end()) {
                error = "unknown property '" + property.name + "'";
                return false;
            }
            if (definition->second.second != property.value.type) {
                error = "property '" + property.name + "' has the wrong value type";
                return false;
            }
            if (property.value.type == ValueType::number && std::fabs(property.value.number) > 1.0e12) {
                error = "property '" + property.name + "' exceeds numeric limit";
                return false;
            }
            if (property.value.type == ValueType::string && !validateUtf8(property.value.string, "property '" + property.name + "'", error)) return false;
        }
    }
    if (roots != 1 || nodes.empty()) {
        error = "DSL must contain exactly one root window";
        return false;
    }
    append32(output, static_cast<uint32_t>(nodes.size()));
    append32(output, static_cast<uint32_t>(bindings.size()));
    for (const auto& node : nodes) {
        append32(output, static_cast<uint32_t>(node.parent));
        append16(output, static_cast<uint16_t>(nodeTypes().at(node.type)));
        append16(output, 0);
        size_t property_count = 0;
        for (const auto& property : node.properties) if (events().find(property.name) == events().end()) ++property_count;
        append32(output, static_cast<uint32_t>(property_count));
        appendString(output, node.id);
        for (const auto& property : node.properties) {
            const auto definition = properties().find(property.name);
            if (definition != properties().end()) appendProperty(output, definition->second.first, property.value);
        }
    }
    for (const auto& binding : bindings) {
        append32(output, binding.node);
        append32(output, static_cast<uint32_t>(binding.event));
        appendString(output, binding.action);
    }
    return true;
}

bool buildText(const std::vector<Node>& nodes, const std::vector<Action>& actions, std::vector<uint8_t>& output, std::string& error) {
    std::unordered_map<std::string, uint32_t> node_indices;
    for (size_t index = 0; index < nodes.size(); ++index) node_indices.emplace(nodes[index].id, static_cast<uint32_t>(index));
    std::unordered_set<std::string> action_ids;
    append32(output, static_cast<uint32_t>(actions.size()));
    for (const auto& action : actions) {
        if (!validateUtf8(action.id, "action ID", error) || !action_ids.insert(action.id).second) {
            if (error.empty()) error = "duplicate action ID '" + action.id + "'";
            return false;
        }
        appendString(output, action.id);
        append32(output, static_cast<uint32_t>(action.operations.size()));
        for (const auto& operation : action.operations) {
            if (operation.emit) {
                if (!validateUtf8(operation.name, "emit name", error) || !validateUtf8(operation.text, "emit text", error) ||
                    !std::isfinite(operation.value) || std::fabs(operation.value) > 1.0e12) {
                    if (error.empty()) error = "emit value exceeds numeric limit";
                    return false;
                }
                append16(output, static_cast<uint16_t>(Operation::emit));
                append16(output, 0);
                std::vector<uint8_t> payload;
                appendString(payload, operation.name);
                appendString(payload, operation.text);
                appendDouble(payload, operation.value);
                append32(output, static_cast<uint32_t>(payload.size()));
                appendBytes(output, payload.data(), payload.size());
                continue;
            }
            const auto node = node_indices.find(operation.set.node);
            if (node == node_indices.end()) {
                error = "action '" + action.id + "' targets unknown node '" + operation.set.node + "'";
                return false;
            }
            const auto definition = properties().find(operation.set.property.name);
            if (definition == properties().end() || events().find(operation.set.property.name) != events().end()) {
                error = "action '" + action.id + "' targets unknown property '" + operation.set.property.name + "'";
                return false;
            }
            if (definition->second.second != operation.set.property.value.type) {
                error = "action '" + action.id + "' sets the wrong value type for '" + operation.set.property.name + "'";
                return false;
            }
            Operation opcode = Operation::set_string;
            if (operation.set.property.value.type == ValueType::number) opcode = Operation::set_number;
            else if (operation.set.property.value.type == ValueType::boolean) opcode = Operation::set_boolean;
            else if (operation.set.property.value.type == ValueType::color) opcode = Operation::set_color;
            append16(output, static_cast<uint16_t>(opcode));
            append16(output, 0);
            std::vector<uint8_t> payload;
            append32(payload, node->second);
            appendProperty(payload, definition->second.first, operation.set.property.value);
            append32(output, static_cast<uint32_t>(payload.size()));
            appendBytes(output, payload.data(), payload.size());
        }
    }
    return true;
}

bool buildPackage(const std::vector<uint8_t>& data, const std::vector<uint8_t>& text, std::vector<uint8_t>& output, std::string& error) {
    const uint64_t total = 40 + data.size() + 24 + text.size() + 36;
    if (total > max_package_size) {
        error = "compiled package exceeds 64 MiB";
        return false;
    }
    output.reserve(static_cast<size_t>(total));
    appendBytes(output, package_magic.data(), package_magic.size());
    append16(output, package_version);
    append32(output, 0);
    append32(output, 0);
    appendBytes(output, data_marker.data(), data_marker.size());
    append64(output, data.size());
    appendBytes(output, data.data(), data.size());
    appendBytes(output, text_marker.data(), text_marker.size());
    append64(output, text.size());
    appendBytes(output, text.data(), text.size());
    appendBytes(output, end_marker.data(), end_marker.size());
    append64(output, total);
    append32(output, nectorCoreCrc32(data.data(), data.size()));
    append32(output, nectorCoreCrc32(text.data(), text.size()));
    append32(output, 0);
    return output.size() == total;
}

} // namespace

bool compileSource(const char* source, std::vector<uint8_t>& output, std::string& error) {
    output.clear();
    error.clear();
    if (source == nullptr) {
        error = "DSL source is null";
        return false;
    }
    const size_t source_size = std::strlen(source);
    if (source_size > max_package_size) {
        error = "DSL source exceeds 64 MiB";
        return false;
    }
    if (nectorCoreValidateUtf8(source, source_size) != nector_ok) {
        error = "DSL source is not valid UTF-8";
        return false;
    }
    Parser parser(source);
    std::vector<Node> nodes;
    std::vector<Action> actions;
    if (!parser.parse(nodes, actions, error)) return false;
    std::unordered_set<std::string> known_actions;
    for (const auto& action : actions) known_actions.insert(action.id);
    for (const auto& node : nodes) {
        for (const auto& property : node.properties) {
            if (events().find(property.name) != events().end() && property.value.type == ValueType::string &&
                known_actions.find(property.value.string) == known_actions.end()) {
                error = "node '" + node.id + "' references unknown action '" + property.value.string + "'";
                return false;
            }
        }
    }
    std::vector<uint8_t> data;
    std::vector<uint8_t> text;
    if (!buildData(nodes, data, error) || !buildText(nodes, actions, text, error)) return false;
    return buildPackage(data, text, output, error);
}

} // namespace nector_cdsl_detail
