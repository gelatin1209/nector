#ifndef nector_cdsl_package_schema_h
#define nector_cdsl_package_schema_h

#include "package_reader.h"
#include "scene.h"

#include <cmath>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nector_cdsl_detail {

struct Value {
    ValueType type = ValueType::number;
    std::string string;
    double number = 0;
    bool boolean = false;
    nector_color color{0, 0, 0, 1};
};

struct Property {
    size_t offset = 0;
    uint32_t payload_size = 0;
    nector_node_property property = nector_property_text;
    Value value;
};

struct NodeData {
    size_t offset = 0;
    int32_t parent = -1;
    nector_node_type type = nector_node_window;
    std::string id;
    std::vector<Property> properties;
    nector_node_handle handle = nullptr;
};

struct Binding {
    size_t offset = 0;
    uint32_t node = 0;
    nector_ui_event_type event = nector_ui_none;
    std::string action;
};

struct ActionOperation {
    size_t offset = 0;
    uint32_t payload_size = 0;
    Operation opcode = Operation::emit;
    uint32_t node = 0;
    Property property;
    std::string name;
    std::string text;
    double value = 0;
};

struct Action {
    size_t offset = 0;
    std::string id;
    std::vector<ActionOperation> operations;
};

struct PackageRecords {
    std::vector<NodeData> nodes;
    std::vector<Binding> bindings;
    std::vector<Action> actions;
    size_t total_operations = 0;
    size_t data_end_offset = 0;
    size_t text_end_offset = 0;
};

inline bool validRootDimensions(const PackageRecords& package) {
    double width = 640;
    double height = 480;
    for (const auto& property : package.nodes.front().properties) {
        if (property.value.type != ValueType::number) continue;
        if (property.property == nector_property_width) width = property.value.number;
        if (property.property == nector_property_height) height = property.value.number;
    }
    return width >= 1 && width <= 32768 && height >= 1 && height <= 32768 &&
           std::floor(width) == width && std::floor(height) == height;
}

inline bool propertyType(nector_node_property property, ValueType& type) {
    switch (property) {
        case nector_property_text:
        case nector_property_title:
        case nector_property_placeholder:
        case nector_property_source: type = ValueType::string; return true;
        case nector_property_width:
        case nector_property_height:
        case nector_property_padding:
        case nector_property_gap:
        case nector_property_flex:
        case nector_property_font_size:
        case nector_property_value:
        case nector_property_minimum:
        case nector_property_maximum:
        case nector_property_step:
        case nector_property_caret:
        case nector_property_corner_radius: type = ValueType::number; return true;
        case nector_property_visible:
        case nector_property_enabled:
        case nector_property_checked:
        case nector_property_word_wrap: type = ValueType::boolean; return true;
        case nector_property_background:
        case nector_property_foreground:
        case nector_property_border:
        case nector_property_accent: type = ValueType::color; return true;
        default: return false;
    }
}

inline bool operationType(Operation operation, ValueType& type) {
    switch (operation) {
        case Operation::set_string: type = ValueType::string; return true;
        case Operation::set_number: type = ValueType::number; return true;
        case Operation::set_boolean: type = ValueType::boolean; return true;
        case Operation::set_color: type = ValueType::color; return true;
        case Operation::emit: return false;
    }
    return false;
}

inline bool validEvent(uint32_t value) {
    return value >= static_cast<uint32_t>(nector_ui_click) && value <= static_cast<uint32_t>(nector_ui_quit);
}

inline const char* propertyName(nector_node_property property) {
    switch (property) {
        case nector_property_text: return "text";
        case nector_property_title: return "title";
        case nector_property_placeholder: return "placeholder";
        case nector_property_source: return "source";
        case nector_property_width: return "width";
        case nector_property_height: return "height";
        case nector_property_padding: return "padding";
        case nector_property_gap: return "gap";
        case nector_property_flex: return "flex";
        case nector_property_font_size: return "font_size";
        case nector_property_value: return "value";
        case nector_property_minimum: return "minimum";
        case nector_property_maximum: return "maximum";
        case nector_property_step: return "step";
        case nector_property_caret: return "caret";
        case nector_property_corner_radius: return "corner_radius";
        case nector_property_visible: return "visible";
        case nector_property_enabled: return "enabled";
        case nector_property_checked: return "checked";
        case nector_property_word_wrap: return "word_wrap";
        case nector_property_background: return "background";
        case nector_property_foreground: return "foreground";
        case nector_property_border: return "border";
        case nector_property_accent: return "accent";
        default: return "unknown";
    }
}

inline const char* nodeTypeName(nector_node_type type) {
    switch (type) {
        case nector_node_window: return "window";
        case nector_node_column: return "column";
        case nector_node_row: return "row";
        case nector_node_label: return "label";
        case nector_node_button: return "button";
        case nector_node_line_edit: return "line_edit";
        case nector_node_check_box: return "check_box";
        case nector_node_slider: return "slider";
        case nector_node_progress_bar: return "progress_bar";
        case nector_node_spacer: return "spacer";
        case nector_node_canvas: return "canvas";
        case nector_node_image: return "image";
    }
    return "unknown";
}

inline const char* eventName(nector_ui_event_type event) {
    switch (event) {
        case nector_ui_none: return "none";
        case nector_ui_click: return "click";
        case nector_ui_text_changed: return "text_changed";
        case nector_ui_value_changed: return "value_changed";
        case nector_ui_toggled: return "toggled";
        case nector_ui_paint: return "paint";
        case nector_ui_focus: return "focus";
        case nector_ui_blur: return "blur";
        case nector_ui_custom: return "custom";
        case nector_ui_animation_finished: return "animation_finished";
        case nector_ui_quit: return "quit";
        case nector_ui_scroll: return "scroll";
    }
    return "unknown";
}

inline const char* valueTypeName(ValueType type) {
    switch (type) {
        case ValueType::string: return "string";
        case ValueType::number: return "number";
        case ValueType::boolean: return "boolean";
        case ValueType::color: return "color";
    }
    return "unknown";
}

inline const char* operationName(Operation operation) {
    switch (operation) {
        case Operation::set_string: return "set_string";
        case Operation::set_number: return "set_number";
        case Operation::set_boolean: return "set_boolean";
        case Operation::set_color: return "set_color";
        case Operation::emit: return "emit";
    }
    return "unknown";
}

inline bool readProperty(Reader& reader, Property& property, std::string& error) {
    property.offset = reader.offset();
    uint16_t property_value = 0;
    uint16_t type_value = 0;
    if (!reader.read16(property_value) || !reader.read16(type_value) || !reader.read32(property.payload_size)) {
        error = "truncated property header";
        return false;
    }
    property.property = static_cast<nector_node_property>(property_value);
    ValueType expected = ValueType::number;
    if (!propertyType(property.property, expected) || type_value != static_cast<uint16_t>(expected)) {
        error = "property type does not match its definition";
        return false;
    }
    bool valid = false;
    Reader payload = reader.subreader(property.payload_size, valid);
    if (!valid) {
        error = "property payload exceeds section bounds";
        return false;
    }
    property.value.type = expected;
    switch (expected) {
        case ValueType::string: {
            const uint8_t* bytes = nullptr;
            if (!payload.readBytes(property.payload_size, bytes)) break;
            if (property.payload_size == 0) {
                property.value.string.clear();
            } else {
                property.value.string.assign(reinterpret_cast<const char*>(bytes), property.payload_size);
            }
            if (nectorCoreValidateUtf8(property.value.string.data(), property.value.string.size()) != nector_ok) {
                error = "property string is not valid UTF-8";
                return false;
            }
            break;
        }
        case ValueType::number:
            if (!payload.readDouble(property.value.number) || std::fabs(property.value.number) > 1.0e12) {
                error = "property number is invalid";
                return false;
            }
            break;
        case ValueType::boolean: {
            uint32_t value = 0;
            if (!payload.read32(value) || value > 1) {
                error = "property boolean is invalid";
                return false;
            }
            property.value.boolean = value != 0;
            break;
        }
        case ValueType::color:
            if (!payload.readFloat(property.value.color.r) || !payload.readFloat(property.value.color.g) ||
                !payload.readFloat(property.value.color.b) || !payload.readFloat(property.value.color.a) ||
                property.value.color.r < 0 || property.value.color.r > 1 || property.value.color.g < 0 || property.value.color.g > 1 ||
                property.value.color.b < 0 || property.value.color.b > 1 || property.value.color.a < 0 || property.value.color.a > 1) {
                error = "property color is invalid";
                return false;
            }
            break;
    }
    if (!payload.empty()) {
        error = "property payload has trailing bytes";
        return false;
    }
    return true;
}

inline bool readDataSection(const uint8_t* data, size_t size, size_t absolute_offset,
                            PackageRecords& package, std::string& error) {
    Reader reader(data, size, absolute_offset);
    package.data_end_offset = absolute_offset + size;
    uint32_t node_count = 0;
    uint32_t binding_count = 0;
    if (!reader.read32(node_count) || !reader.read32(binding_count) || node_count == 0 ||
        node_count > max_node_count || binding_count > max_node_count * 10) {
        error = "invalid Data Section counts";
        return false;
    }
    package.nodes.reserve(node_count);
    std::unordered_set<std::string> ids;
    for (uint32_t index = 0; index < node_count; ++index) {
        NodeData node;
        node.offset = reader.offset();
        uint32_t parent = 0;
        uint16_t type = 0;
        uint16_t reserved = 0;
        uint32_t property_count = 0;
        if (!reader.read32(parent) || !reader.read16(type) || !reader.read16(reserved) ||
            !reader.read32(property_count) || !reader.readString(node.id) || reserved != 0 ||
            type > static_cast<uint16_t>(nector_node_image) || node.id.empty() || property_count > 64) {
            error = "invalid node record at index " + std::to_string(index);
            return false;
        }
        node.parent = static_cast<int32_t>(parent);
        node.type = static_cast<nector_node_type>(type);
        if (index == 0) {
            if (node.parent != -1 || node.type != nector_node_window) {
                error = "first node is not the root window";
                return false;
            }
        } else if (node.parent < 0 || static_cast<uint32_t>(node.parent) >= index || node.type == nector_node_window) {
            error = "node has an invalid parent or nested window";
            return false;
        }
        if (!ids.insert(node.id).second) {
            error = "duplicate node ID '" + node.id + "'";
            return false;
        }
        std::unordered_set<uint16_t> properties;
        node.properties.reserve(property_count);
        for (uint32_t property_index = 0; property_index < property_count; ++property_index) {
            Property property;
            if (!readProperty(reader, property, error)) return false;
            if (!properties.insert(static_cast<uint16_t>(property.property)).second) {
                error = "duplicate property on node '" + node.id + "'";
                return false;
            }
            node.properties.push_back(std::move(property));
        }
        package.nodes.push_back(std::move(node));
    }
    package.bindings.reserve(binding_count);
    std::unordered_set<std::string> binding_keys;
    for (uint32_t index = 0; index < binding_count; ++index) {
        Binding binding;
        binding.offset = reader.offset();
        uint32_t event = 0;
        uint32_t node = 0;
        if (!reader.read32(node) || !reader.read32(event) || !reader.readString(binding.action) ||
            node >= node_count || !validEvent(event) || binding.action.empty()) {
            error = "invalid event binding record";
            return false;
        }
        binding.node = node;
        binding.event = static_cast<nector_ui_event_type>(event);
        const std::string key = std::to_string(node) + ":" + std::to_string(event);
        if (!binding_keys.insert(key).second) {
            error = "duplicate event binding";
            return false;
        }
        package.bindings.push_back(std::move(binding));
    }
    if (!reader.empty()) {
        error = "Data Section has trailing bytes";
        return false;
    }
    if (!validRootDimensions(package)) {
        error = "root window dimensions are invalid";
        return false;
    }
    return true;
}

inline bool readTextSection(const uint8_t* data, size_t size, size_t absolute_offset,
                            PackageRecords& package, std::string& error) {
    Reader reader(data, size, absolute_offset);
    package.text_end_offset = absolute_offset + size;
    uint32_t action_count = 0;
    if (!reader.read32(action_count) || action_count > max_action_count) {
        error = "invalid Text Section action count";
        return false;
    }
    package.actions.reserve(action_count);
    std::unordered_set<std::string> action_ids;
    size_t total_operations = 0;
    for (uint32_t action_index = 0; action_index < action_count; ++action_index) {
        Action action;
        action.offset = reader.offset();
        uint32_t operation_count = 0;
        if (!reader.readString(action.id) || !reader.read32(operation_count) || action.id.empty() ||
            operation_count > max_operation_count || total_operations + operation_count > max_operation_count) {
            error = "invalid action record";
            return false;
        }
        total_operations += operation_count;
        if (!action_ids.insert(action.id).second) {
            error = "duplicate action ID '" + action.id + "'";
            return false;
        }
        action.operations.reserve(operation_count);
        for (uint32_t operation_index = 0; operation_index < operation_count; ++operation_index) {
            ActionOperation operation;
            operation.offset = reader.offset();
            uint16_t opcode = 0;
            uint16_t reserved = 0;
            if (!reader.read16(opcode) || !reader.read16(reserved) || !reader.read32(operation.payload_size) || reserved != 0 ||
                opcode < static_cast<uint16_t>(Operation::set_string) || opcode > static_cast<uint16_t>(Operation::emit)) {
                error = "invalid action operation header";
                return false;
            }
            bool valid = false;
            Reader payload = reader.subreader(operation.payload_size, valid);
            if (!valid) {
                error = "action operation exceeds Text Section bounds";
                return false;
            }
            operation.opcode = static_cast<Operation>(opcode);
            if (operation.opcode == Operation::emit) {
                if (!payload.readString(operation.name) || !payload.readString(operation.text) || !payload.readDouble(operation.value) ||
                    operation.name.empty() || std::fabs(operation.value) > 1.0e12) {
                    error = "invalid emit operation";
                    return false;
                }
            } else {
                if (!payload.read32(operation.node) || operation.node >= package.nodes.size() ||
                    !readProperty(payload, operation.property, error)) {
                    if (error.empty()) error = "invalid property-set operation";
                    return false;
                }
                ValueType expected = ValueType::string;
                if (!operationType(operation.opcode, expected) || operation.property.value.type != expected) {
                    error = "property-set opcode and value type disagree";
                    return false;
                }
            }
            if (!payload.empty()) {
                error = "action operation has trailing bytes";
                return false;
            }
            action.operations.push_back(std::move(operation));
        }
        package.actions.push_back(std::move(action));
    }
    package.total_operations = total_operations;
    if (!reader.empty()) {
        error = "Text Section has trailing bytes";
        return false;
    }
    for (const auto& binding : package.bindings) {
        if (action_ids.find(binding.action) == action_ids.end()) {
            error = "event references unknown action '" + binding.action + "'";
            return false;
        }
    }
    return true;
}

}
#endif
