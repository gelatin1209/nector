#include "record_decoder.h"

#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace nddbg {
namespace {

using nector_cdsl_detail::ActionOperation;
using nector_cdsl_detail::Property;
using nector_cdsl_detail::Value;
using nector_cdsl_detail::ValueType;

std::string quoted(const std::string& value) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    output.put('"');
    for (unsigned char byte : value) {
        switch (byte) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (byte < 32 || byte == 127) {
                    output << "\\x" << std::setw(2) << static_cast<unsigned>(byte);
                } else {
                    output.put(static_cast<char>(byte));
                }
                break;
        }
    }
    output.put('"');
    return output.str();
}

void writeValue(std::ostringstream& output, const Value& value) {
    switch (value.type) {
        case ValueType::string: output << quoted(value.string); break;
        case ValueType::number: output << std::setprecision(17) << value.number; break;
        case ValueType::boolean: output << (value.boolean ? "true" : "false"); break;
        case ValueType::color:
            output << "rgba(" << std::setprecision(9) << value.color.r << ',' << value.color.g << ','
                   << value.color.b << ',' << value.color.a << ')';
            break;
    }
}

nddbg::DecodedRecord propertyRecord(const std::string& label, const Property& property) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << label << " name=" << nector_cdsl_detail::propertyName(property.property)
           << " type=" << nector_cdsl_detail::valueTypeName(property.value.type)
           << " payload_size=" << property.payload_size << " value=";
    writeValue(output, property.value);
    return {property.offset, 8 + static_cast<size_t>(property.payload_size), output.str()};
}

nddbg::DecodedRecord operationPropertyRecord(size_t action, size_t operation, const ActionOperation& value) {
    return propertyRecord("operation_property[" + std::to_string(action) + "," + std::to_string(operation) + "]",
                          value.property);
}

}

size_t nodeRecordSize(const nector_cdsl_detail::PackageRecords& records, size_t node_index) {
    const auto& node = records.nodes[node_index];
    const size_t next_offset = node_index + 1 < records.nodes.size()
        ? records.nodes[node_index + 1].offset
        : !records.bindings.empty() ? records.bindings.front().offset : records.data_end_offset;
    return next_offset - node.offset;
}

size_t actionRecordSize(const nector_cdsl_detail::PackageRecords& records, size_t action_index) {
    const auto& action = records.actions[action_index];
    const size_t next_offset = action_index + 1 < records.actions.size()
        ? records.actions[action_index + 1].offset : records.text_end_offset;
    return next_offset - action.offset;
}

std::vector<DecodedRecord> decodeRecords(const nector_cdsl_detail::PackageRecords& records) {
    std::vector<DecodedRecord> output;
    for (size_t node_index = 0; node_index < records.nodes.size(); ++node_index) {
        const auto& node = records.nodes[node_index];
        std::ostringstream fields;
        fields.imbue(std::locale::classic());
        fields << "node[" << node_index << "] parent=" << node.parent
               << " type=" << nector_cdsl_detail::nodeTypeName(node.type)
               << " id=" << quoted(node.id) << " property_count=" << node.properties.size();
        output.push_back({node.offset, nodeRecordSize(records, node_index), fields.str()});
        for (size_t property_index = 0; property_index < node.properties.size(); ++property_index) {
            output.push_back(propertyRecord("property[" + std::to_string(node_index) + "," +
                                            std::to_string(property_index) + "]", node.properties[property_index]));
        }
    }
    for (size_t binding_index = 0; binding_index < records.bindings.size(); ++binding_index) {
        const auto& binding = records.bindings[binding_index];
        std::ostringstream fields;
        fields.imbue(std::locale::classic());
        fields << "binding[" << binding_index << "] node=" << binding.node
               << " event=" << nector_cdsl_detail::eventName(binding.event)
               << " action=" << quoted(binding.action);
        output.push_back({binding.offset, 12 + binding.action.size(), fields.str()});
    }
    for (size_t action_index = 0; action_index < records.actions.size(); ++action_index) {
        const auto& action = records.actions[action_index];
        std::ostringstream fields;
        fields.imbue(std::locale::classic());
        fields << "action[" << action_index << "] id=" << quoted(action.id)
               << " operation_count=" << action.operations.size();
        output.push_back({action.offset, actionRecordSize(records, action_index), fields.str()});
        for (size_t operation_index = 0; operation_index < action.operations.size(); ++operation_index) {
            const auto& operation = action.operations[operation_index];
            std::ostringstream operation_fields;
            operation_fields.imbue(std::locale::classic());
            operation_fields << "operation[" << action_index << ',' << operation_index << "] opcode="
                             << nector_cdsl_detail::operationName(operation.opcode)
                             << " payload_size=" << operation.payload_size;
            if (operation.opcode == nector_cdsl_detail::Operation::emit) {
                operation_fields << " name=" << quoted(operation.name) << " text=" << quoted(operation.text)
                                 << " value=" << std::setprecision(17) << operation.value;
            } else {
                operation_fields << " node=" << operation.node;
            }
            output.push_back({operation.offset, 8 + static_cast<size_t>(operation.payload_size), operation_fields.str()});
            if (operation.opcode != nector_cdsl_detail::Operation::emit) {
                output.push_back(operationPropertyRecord(action_index, operation_index, operation));
            }
        }
    }
    return output;
}

}
