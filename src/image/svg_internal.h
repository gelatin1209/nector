#ifndef nector_svg_internal_h
#define nector_svg_internal_h

#include "image_internal.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nector_image_internal {

struct XmlNode {
    std::string name;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<std::unique_ptr<XmlNode>> children;
};

std::string_view xmlLocalName(std::string_view name);
const std::string* xmlAttribute(const XmlNode& node, std::string_view name);
nector_status parseXml(std::string_view input, std::unique_ptr<XmlNode>* out_root);

class SvgNumberScanner {
public:
    explicit SvgNumberScanner(std::string_view text) : text_(text) {}
    void skipSpace();
    bool nextIsNumber();
    bool number(double* out_value);
    bool command(char* out_command);
    bool atEnd();

private:
    std::string_view text_;
    size_t offset_ = 0;
    bool has_number_ = false;
};

bool parseSvgNumber(std::string_view text, double* out_value);

}

#endif
