#include "decompiler.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <ostream>
#include <sstream>
#include <string>

namespace nddbg {
namespace {

using nector_cdsl_detail::PackageRecords;
using nector_cdsl_detail::Value;
using nector_cdsl_detail::ValueType;

std::string quoted(const std::string& value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output.put('"');
    for (const unsigned char byte : value) {
        switch (byte) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (byte < 32 || byte == 127) {
                    output << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                           << static_cast<unsigned>(byte) << std::dec;
                } else {
                    output.put(static_cast<char>(byte));
                }
                break;
        }
    }
    output.put('"');
    return output.str();
}

std::string number(double value) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return output.str();
}

std::string color(const nector_color& value) {
    const auto channel = [](float component) {
        return static_cast<unsigned>(std::lround(static_cast<double>(component) * 255.0));
    };
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << '#' << std::hex << std::setfill('0')
           << std::setw(2) << channel(value.r) << std::setw(2) << channel(value.g)
           << std::setw(2) << channel(value.b) << std::setw(2) << channel(value.a);
    return output.str();
}

void writeValue(std::ostream& output, const Value& value) {
    switch (value.type) {
        case ValueType::string: output << quoted(value.string); break;
        case ValueType::number: output << number(value.number); break;
        case ValueType::boolean: output << (value.boolean ? "true" : "false"); break;
        case ValueType::color: output << color(value.color); break;
    }
}

void indent(std::ostream& output, size_t depth) {
    for (size_t index = 0; index < depth * 4; ++index) output.put(' ');
}

void writeNode(std::ostream& output, const PackageRecords& records, size_t node_index, size_t depth) {
    const auto& node = records.nodes[node_index];
    indent(output, depth);
    output << nector_cdsl_detail::nodeTypeName(node.type) << ' ' << quoted(node.id) << " {\n";
    for (const auto& property : node.properties) {
        indent(output, depth + 1);
        output << nector_cdsl_detail::propertyName(property.property) << " = ";
        writeValue(output, property.value);
        output << ";\n";
    }
    for (const auto& binding : records.bindings) {
        if (binding.node != node_index) continue;
        indent(output, depth + 1);
        output << "on_" << nector_cdsl_detail::eventName(binding.event) << " = " << quoted(binding.action) << ";\n";
    }
    for (size_t child_index = 0; child_index < records.nodes.size(); ++child_index) {
        if (records.nodes[child_index].parent == static_cast<int32_t>(node_index)) {
            writeNode(output, records, child_index, depth + 1);
        }
    }
    indent(output, depth);
    output << "}\n";
}

}

void writeDecompilation(std::ostream& output, const PackageRecords& records) {
    if (!records.nodes.empty()) writeNode(output, records, 0, 0);
    for (const auto& action : records.actions) {
        output << "\naction " << quoted(action.id) << " {\n";
        for (const auto& operation : action.operations) {
            output << "    ";
            if (operation.opcode == nector_cdsl_detail::Operation::emit) {
                output << "emit " << quoted(operation.name) << ", " << quoted(operation.text)
                       << ", " << number(operation.value) << ";\n";
            } else {
                output << "set " << quoted(records.nodes[operation.node].id) << '.'
                       << nector_cdsl_detail::propertyName(operation.property.property) << " = ";
                writeValue(output, operation.property.value);
                output << ";\n";
            }
        }
        output << "}\n";
    }
}

}
