#include "svg_internal.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace nector_image_internal {
namespace {

bool svgSpace(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

bool nameStart(unsigned char value) {
    return value == ':' || value == '_' || (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z') || value >= 0x80;
}

bool nameCharacter(unsigned char value) {
    return nameStart(value) || value == '-' || value == '.' || (value >= '0' && value <= '9');
}

void appendUtf8(uint32_t codepoint, std::string* output) {
    if (codepoint <= 0x7f) {
        output->push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

class XmlParser {
public:
    explicit XmlParser(std::string_view input) : input_(input) {}

    bool parse(std::unique_ptr<XmlNode>* out_root) {
        if (input_.size() >= 3 && static_cast<uint8_t>(input_[0]) == 0xef &&
            static_cast<uint8_t>(input_[1]) == 0xbb && static_cast<uint8_t>(input_[2]) == 0xbf) {
            offset_ = 3;
        }
        if (!misc()) return false;
        if (offset_ >= input_.size() || input_[offset_] != '<') return fail("SVG document has no root element");
        std::unique_ptr<XmlNode> root;
        if (!element(&root, 0)) return false;
        if (!misc() || offset_ != input_.size()) return fail("unexpected content after SVG root");
        *out_root = std::move(root);
        return true;
    }

    const char* error() const { return error_; }

private:
    bool fail(const char* message) {
        if (error_ == nullptr) error_ = message;
        return false;
    }

    bool starts(std::string_view value) const {
        return offset_ <= input_.size() && value.size() <= input_.size() - offset_ &&
               input_.substr(offset_, value.size()) == value;
    }

    bool consume(std::string_view value) {
        if (!starts(value)) return false;
        offset_ += value.size();
        return true;
    }

    void whitespace() {
        while (offset_ < input_.size() && svgSpace(input_[offset_])) ++offset_;
    }

    bool comment() {
        if (!consume("<!--")) return false;
        const size_t end = input_.find("-->", offset_);
        if (end == std::string_view::npos) return fail("unterminated XML comment");
        offset_ = end + 3;
        return true;
    }

    bool processingInstruction() {
        if (!consume("<?")) return false;
        const size_t end = input_.find("?>", offset_);
        if (end == std::string_view::npos) return fail("unterminated XML processing instruction");
        offset_ = end + 2;
        return true;
    }

    bool misc() {
        for (;;) {
            whitespace();
            if (starts("<!--")) {
                if (!comment()) return false;
            } else if (starts("<?")) {
                if (!processingInstruction()) return false;
            } else if (starts("<!DOCTYPE") || starts("<!ENTITY")) {
                return fail("DTD and entity declarations are not allowed");
            } else {
                return true;
            }
        }
    }

    bool name(std::string* output) {
        if (offset_ >= input_.size() || !nameStart(static_cast<uint8_t>(input_[offset_]))) return false;
        const size_t start = offset_++;
        while (offset_ < input_.size() && nameCharacter(static_cast<uint8_t>(input_[offset_]))) ++offset_;
        output->assign(input_.substr(start, offset_ - start));
        return true;
    }

    bool entity(std::string* output) {
        if (!consume("&")) return false;
        const size_t end = input_.find(';', offset_);
        if (end == std::string_view::npos || end == offset_ || end - offset_ > 16) {
            return fail("invalid XML entity");
        }
        const std::string_view value = input_.substr(offset_, end - offset_);
        offset_ = end + 1;
        if (value == "amp") output->push_back('&');
        else if (value == "lt") output->push_back('<');
        else if (value == "gt") output->push_back('>');
        else if (value == "quot") output->push_back('"');
        else if (value == "apos") output->push_back('\'');
        else {
            if (value.size() < 2 || value[0] != '#') return fail("unknown XML entity");
            const bool hexadecimal = value[1] == 'x' || value[1] == 'X';
            const size_t first = hexadecimal ? 2 : 1;
            if (first == value.size()) return fail("empty numeric XML entity");
            uint32_t codepoint = 0;
            for (size_t index = first; index < value.size(); ++index) {
                const char digit = value[index];
                unsigned int number = 0;
                if (digit >= '0' && digit <= '9') number = static_cast<unsigned int>(digit - '0');
                else if (hexadecimal && digit >= 'a' && digit <= 'f') number = static_cast<unsigned int>(digit - 'a' + 10);
                else if (hexadecimal && digit >= 'A' && digit <= 'F') number = static_cast<unsigned int>(digit - 'A' + 10);
                else return fail("invalid numeric XML entity");
                const uint32_t base = hexadecimal ? 16u : 10u;
                if (codepoint > (0x10ffffu - number) / base) return fail("XML codepoint is out of range");
                codepoint = codepoint * base + number;
            }
            if (codepoint == 0 || codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
                (codepoint < 0x20 && codepoint != '\t' && codepoint != '\r' && codepoint != '\n')) {
                return fail("XML codepoint is not a valid character");
            }
            appendUtf8(codepoint, output);
        }
        return true;
    }

    bool attributeValue(std::string* output) {
        if (offset_ >= input_.size() || (input_[offset_] != '"' && input_[offset_] != '\'')) {
            return fail("XML attribute value must be quoted");
        }
        const char quote = input_[offset_++];
        output->clear();
        while (offset_ < input_.size() && input_[offset_] != quote) {
            if (input_[offset_] == '<') return fail("XML attribute contains '<'");
            if (input_[offset_] == '&') {
                if (!entity(output)) return false;
            } else {
                output->push_back(input_[offset_++]);
            }
        }
        if (offset_ == input_.size()) return fail("unterminated XML attribute");
        ++offset_;
        return true;
    }

    bool element(std::unique_ptr<XmlNode>* out_node, unsigned int depth) {
        if (depth > 128) return fail("XML nesting limit exceeded");
        if (nodes_ >= 10000) return fail("XML node limit exceeded");
        if (!consume("<") || starts("/") || starts("!") || starts("?")) return fail("invalid XML element");
        auto node = std::make_unique<XmlNode>();
        ++nodes_;
        if (!name(&node->name)) return fail("invalid XML element name");
        for (;;) {
            whitespace();
            if (consume("/>")) {
                *out_node = std::move(node);
                return true;
            }
            if (consume(">")) break;
            if (node->attributes.size() >= 128) return fail("XML attribute limit exceeded");
            std::string attribute_name;
            if (!name(&attribute_name)) return fail("invalid XML attribute name");
            const auto duplicate = std::find_if(node->attributes.begin(), node->attributes.end(),
                [&attribute_name](const auto& item) { return item.first == attribute_name; });
            if (duplicate != node->attributes.end()) return fail("duplicate XML attribute");
            whitespace();
            if (!consume("=")) return fail("XML attribute requires '='");
            whitespace();
            std::string value;
            if (!attributeValue(&value)) return false;
            node->attributes.emplace_back(std::move(attribute_name), std::move(value));
        }
        for (;;) {
            if (offset_ >= input_.size()) return fail("unterminated XML element");
            if (starts("</")) {
                consume("</");
                std::string closing_name;
                if (!name(&closing_name)) return fail("invalid XML closing tag");
                whitespace();
                if (!consume(">")) return fail("invalid XML closing tag");
                if (closing_name != node->name) return fail("mismatched XML closing tag");
                *out_node = std::move(node);
                return true;
            }
            if (starts("<!--")) {
                if (!comment()) return false;
            } else if (starts("<?")) {
                if (!processingInstruction()) return false;
            } else if (starts("<![CDATA[")) {
                const size_t end = input_.find("]]>", offset_ + 9);
                if (end == std::string_view::npos) return fail("unterminated CDATA section");
                offset_ = end + 3;
            } else if (starts("<!")) {
                return fail("unsupported XML declaration");
            } else if (input_[offset_] == '<') {
                std::unique_ptr<XmlNode> child;
                if (!element(&child, depth + 1)) return false;
                node->children.push_back(std::move(child));
            } else {
                ++offset_;
            }
        }
    }

    std::string_view input_;
    size_t offset_ = 0;
    size_t nodes_ = 0;
    const char* error_ = nullptr;
};

}

std::string_view xmlLocalName(std::string_view name) {
    const size_t colon = name.rfind(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

const std::string* xmlAttribute(const XmlNode& node, std::string_view name) {
    for (const auto& attribute : node.attributes) {
        if (xmlLocalName(attribute.first) == name) return &attribute.second;
    }
    return nullptr;
}

nector_status parseXml(std::string_view input, std::unique_ptr<XmlNode>* out_root) {
    if (out_root == nullptr) return nectorFail(nector_invalid_argument, "XML output is null");
    XmlParser parser(input);
    if (!parser.parse(out_root)) return nectorFail(nector_invalid_format, parser.error());
    return nector_ok;
}

void SvgNumberScanner::skipSpace() {
    while (offset_ < text_.size() && svgSpace(text_[offset_])) ++offset_;
}

bool SvgNumberScanner::nextIsNumber() {
    const size_t start = offset_;
    skipSpace();
    if (has_number_ && offset_ < text_.size() && text_[offset_] == ',') {
        ++offset_;
        skipSpace();
    }
    if (offset_ >= text_.size()) {
        offset_ = start;
        return false;
    }
    const char value = text_[offset_];
    if (value == '+' || value == '-' || value == '.' || (value >= '0' && value <= '9')) return true;
    offset_ = start;
    return false;
}

bool SvgNumberScanner::number(double* out_value) {
    if (out_value == nullptr || !nextIsNumber()) return false;
    const size_t start = offset_;
    if (text_[offset_] == '+' || text_[offset_] == '-') ++offset_;
    bool has_digits = false;
    while (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') {
        ++offset_;
        has_digits = true;
    }
    if (offset_ < text_.size() && text_[offset_] == '.') {
        ++offset_;
        while (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') {
            ++offset_;
            has_digits = true;
        }
    }
    if (!has_digits) {
        offset_ = start;
        return false;
    }
    if (offset_ < text_.size() && (text_[offset_] == 'e' || text_[offset_] == 'E')) {
        ++offset_;
        if (offset_ < text_.size() && (text_[offset_] == '+' || text_[offset_] == '-')) ++offset_;
        const size_t exponent_start = offset_;
        while (offset_ < text_.size() && text_[offset_] >= '0' && text_[offset_] <= '9') ++offset_;
        if (offset_ == exponent_start) {
            offset_ = start;
            return false;
        }
    }
    const std::string temporary(text_.substr(start, offset_ - start));
    char* end = nullptr;
    errno = 0;
    const double value = std::strtod(temporary.c_str(), &end);
    if (end != temporary.c_str() + temporary.size() || errno == ERANGE || !std::isfinite(value)) {
        offset_ = start;
        return false;
    }
    has_number_ = true;
    *out_value = value;
    return true;
}

bool SvgNumberScanner::command(char* out_command) {
    skipSpace();
    if (out_command == nullptr || offset_ >= text_.size()) return false;
    const char value = text_[offset_];
    if (!((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z'))) return false;
    ++offset_;
    has_number_ = false;
    *out_command = value;
    return true;
}

bool SvgNumberScanner::atEnd() {
    skipSpace();
    return offset_ == text_.size();
}

bool parseSvgNumber(std::string_view text, double* out_value) {
    SvgNumberScanner scanner(text);
    return scanner.number(out_value) && scanner.atEnd();
}

}
