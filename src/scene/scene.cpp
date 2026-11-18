#include "internal.h"

#include <cstring>

namespace {

nector_status validateText(const char* value, size_t maximum, bool allow_empty) {
    if (value == nullptr) return nectorFail(nector_invalid_argument, "text is null");
    size_t length = 0;
    while (length <= maximum && value[length] != '\0') ++length;
    if (length > maximum) return nectorFail(nector_limit_exceeded, "text is too long");
    if (!allow_empty && length == 0) return nectorFail(nector_invalid_argument, "text is empty");
    return nectorCoreValidateUtf8(value, length);
}

bool textNode(nector_node_type type) {
    return type == nector_node_label || type == nector_node_button ||
           type == nector_node_line_edit || type == nector_node_check_box;
}

bool fontNode(nector_node_type type) {
    return textNode(type);
}

bool valueNode(nector_node_type type) {
    return type == nector_node_slider || type == nector_node_progress_bar;
}

bool stringSupported(nector_node_handle node, nector_node_property property) {
    switch (property) {
        case nector_property_text: return textNode(node->type);
        case nector_property_title: return node->type == nector_node_window;
        case nector_property_placeholder: return node->type == nector_node_line_edit;
        case nector_property_source: return node->type == nector_node_image;
        default: return false;
    }
}

bool numberSupported(nector_node_handle node, nector_node_property property) {
    switch (property) {
        case nector_property_width:
        case nector_property_height:
        case nector_property_padding:
        case nector_property_flex:
        case nector_property_corner_radius:
            return true;
        case nector_property_gap:
        case nector_property_content_offset_x:
        case nector_property_content_offset_y:
        case nector_property_scroll_step:
        case nector_property_layout:
            return sceneNodeContainer(node->type);
        case nector_property_font_size:
            return fontNode(node->type);
        case nector_property_value:
        case nector_property_minimum:
        case nector_property_maximum:
            return valueNode(node->type);
        case nector_property_step:
            return node->type == nector_node_slider;
        case nector_property_caret:
            return node->type == nector_node_line_edit;
        default:
            return false;
    }
}

const std::string& stringValue(nector_node_handle node, nector_node_property property) {
    switch (property) {
        case nector_property_text: return node->text;
        case nector_property_title: return node->title;
        case nector_property_placeholder: return node->placeholder;
        case nector_property_source: return node->source;
        default: return node->text;
    }
}

bool colorSupported(nector_node_property property) {
    return property == nector_property_background || property == nector_property_foreground ||
           property == nector_property_border || property == nector_property_accent;
}

bool validColor(nector_color color) {
    return nectorFinite(color.r) && nectorFinite(color.g) && nectorFinite(color.b) &&
           nectorFinite(color.a) && color.r >= 0 && color.r <= 1 && color.g >= 0 &&
           color.g <= 1 && color.b >= 0 && color.b <= 1 && color.a >= 0 && color.a <= 1;
}

void markRemoved(nector_scene_handle scene, nector_node_handle node) {
    scene->nodes.erase(node->id);
    node->alive = false;
    if (scene->focused_node == node) scene->focused_node = nullptr;
    if (scene->pressed_node == node) {
        scene->pressed_node = nullptr;
        scene->has_active_touch = false;
    }
    for (const auto& child : node->children) markRemoved(scene, child.get());
}

} // namespace

bool sceneNodeValid(nector_node_handle node) {
    return node != nullptr && node->scene != nullptr && node->alive;
}

bool sceneNodeVisible(nector_node_handle node) {
    if (!sceneNodeValid(node)) return false;
    for (nector_node_handle current = node; current != nullptr; current = current->parent) {
        if (!current->alive || !current->visible) return false;
    }
    return true;
}

bool sceneNodeEnabled(nector_node_handle node) {
    if (!sceneNodeValid(node)) return false;
    for (nector_node_handle current = node; current != nullptr; current = current->parent) {
        if (!current->alive || !current->enabled) return false;
    }
    return true;
}

bool sceneNodeContainer(nector_node_type type) {
    return type == nector_node_window || type == nector_node_column || type == nector_node_row;
}

bool sceneNodeHorizontal(nector_node_handle node) {
    if (node->layout >= 0) return node->layout == 0;
    return node->type == nector_node_row;
}

bool sceneNodeFocusable(nector_node_handle node) {
    if (!sceneNodeVisible(node) || !sceneNodeEnabled(node)) return false;
    return node->type == nector_node_button || node->type == nector_node_line_edit ||
           node->type == nector_node_check_box || node->type == nector_node_slider;
}

bool sceneRectContains(nector_rect rect, float x, float y) {
    return rect.width > 0 && rect.height > 0 && x >= rect.x && y >= rect.y &&
           x < rect.x + rect.width && y < rect.y + rect.height;
}

nector_rect sceneIntersectRect(nector_rect first, nector_rect second) {
    const float x0 = std::max(first.x, second.x);
    const float y0 = std::max(first.y, second.y);
    const float x1 = std::min(first.x + first.width, second.x + second.width);
    const float y1 = std::min(first.y + first.height, second.y + second.height);
    if (x1 <= x0 || y1 <= y0) return {x0, y0, 0, 0};
    return {x0, y0, x1 - x0, y1 - y0};
}

float sceneClampFloat(double value, double minimum, double maximum) {
    return static_cast<float>(std::max(minimum, std::min(maximum, value)));
}

std::vector<size_t> sceneUtf8Offsets(const std::string& text) {
    std::vector<size_t> offsets;
    offsets.push_back(0);
    for (size_t index = 0; index < text.size();) {
        const unsigned char first = static_cast<unsigned char>(text[index]);
        size_t count = 1;
        if ((first & 0xe0u) == 0xc0u) count = 2;
        else if ((first & 0xf0u) == 0xe0u) count = 3;
        else if ((first & 0xf8u) == 0xf0u) count = 4;
        index = std::min(text.size(), index + count);
        offsets.push_back(index);
    }
    return offsets;
}

size_t sceneUtf8Length(const std::string& text) {
    return sceneUtf8Offsets(text).size() - 1;
}

size_t sceneByteOffsetForCaret(const std::string& text, double caret) {
    const std::vector<size_t> offsets = sceneUtf8Offsets(text);
    const size_t index = static_cast<size_t>(std::max(0.0, std::min(caret, static_cast<double>(offsets.size() - 1))));
    return offsets[index];
}

void sceneInitializeNode(nector_node_handle node, nector_scene_handle scene,
                         nector_node_type type, const std::string& id) {
    node->scene = scene;
    node->type = type;
    node->id = id;
    if (type == nector_node_spacer) node->flex = 1;
    if (type == nector_node_progress_bar) {
        node->maximum = 100;
        node->step = 1;
    }
    if (type == nector_node_window) node->background = {0.95f, 0.96f, 0.98f, 1};
    if (type == nector_node_button) {
        node->background = {0.91f, 0.92f, 0.94f, 1};
        node->corner_radius = 4;
        node->padding = 6;
    } else if (type == nector_node_line_edit) {
        node->background = {1, 1, 1, 1};
        node->corner_radius = 3;
        node->padding = 6;
    } else if (type == nector_node_progress_bar) {
        node->background = {0.86f, 0.88f, 0.91f, 1};
        node->corner_radius = 3;
    } else if (type == nector_node_slider) {
        node->padding = 4;
    } else if (type == nector_node_check_box) {
        node->padding = 2;
    }
}

void sceneClearNodeReferences(nector_scene_handle scene, nector_node_handle node) {
    if (scene->focused_node == node) scene->focused_node = nullptr;
    if (scene->pressed_node == node) scene->pressed_node = nullptr;
}

nector_status sceneSetNodeString(nector_node_handle node, nector_node_property property, const char* value) {
    if (!sceneNodeValid(node) || value == nullptr) return nectorFail(nector_invalid_argument, "node or value is null");
    if (!stringSupported(node, property)) return nectorFail(nector_invalid_argument, "string property is not supported by this node");
    const nector_status validation = validateText(value, max_scene_text_size, true);
    if (validation != nector_ok) return validation;
    std::string replacement(value);
    switch (property) {
        case nector_property_text:
            node->text = std::move(replacement);
            if (node->type == nector_node_line_edit) {
                node->caret = std::min(node->caret, static_cast<double>(sceneUtf8Length(node->text)));
            }
            break;
        case nector_property_title:
            node->title = std::move(replacement);
            if (node == node->scene->root.get()) node->scene->title = node->title;
            break;
        case nector_property_placeholder: node->placeholder = std::move(replacement); break;
        case nector_property_source: node->source = std::move(replacement); break;
        default: return nectorFail(nector_invalid_argument, "property is not a string");
    }
    return nector_ok;
}

nector_status sceneSetNodeNumber(nector_node_handle node, nector_node_property property, double value) {
    if (!sceneNodeValid(node) || !std::isfinite(value)) {
        return nectorFail(nector_invalid_argument, "node or numeric value is invalid");
    }
    if (!numberSupported(node, property)) {
        return nectorFail(nector_invalid_argument, "numeric property is not supported by this node");
    }
    switch (property) {
        case nector_property_width:
        case nector_property_height:
        case nector_property_padding:
        case nector_property_gap:
        case nector_property_flex:
        case nector_property_corner_radius:
            if (value < 0 || value > max_scene_dimension) {
                return nectorFail(nector_invalid_argument, "layout value is outside its valid range");
            }
            break;
        case nector_property_content_offset_x:
        case nector_property_content_offset_y:
            if (std::fabs(value) > max_scene_dimension) {
                return nectorFail(nector_invalid_argument, "content offset is outside its valid range");
            }
            break;
        case nector_property_scroll_step:
            if (value < 0 || value > max_scene_dimension) {
                return nectorFail(nector_invalid_argument, "scroll step is outside its valid range");
            }
            break;
        case nector_property_layout:
            if (value != 0 && value != 1) {
                return nectorFail(nector_invalid_argument, "layout must be zero (horizontal) or one (vertical)");
            }
            break;
        case nector_property_font_size:
            if (value <= 0 || value > 4096) return nectorFail(nector_invalid_argument, "font size is outside its valid range");
            break;
        case nector_property_value:
        case nector_property_minimum:
        case nector_property_maximum:
            if (std::fabs(value) > max_scene_value) return nectorFail(nector_invalid_argument, "value is outside its valid range");
            break;
        case nector_property_step:
            if (value <= 0 || value > max_scene_value) return nectorFail(nector_invalid_argument, "step is outside its valid range");
            break;
        case nector_property_caret:
            if (value < 0 || value > max_scene_text_size || std::floor(value) != value) {
                return nectorFail(nector_invalid_argument, "caret is outside its valid range");
            }
            break;
        default:
            return nectorFail(nector_invalid_argument, "property is not numeric");
    }
    switch (property) {
        case nector_property_width: node->width = value; break;
        case nector_property_height: node->height = value; break;
        case nector_property_padding: node->padding = value; break;
        case nector_property_gap: node->gap = value; break;
        case nector_property_flex: node->flex = value; break;
        case nector_property_font_size: node->font_size = value; break;
        case nector_property_value: node->value = std::max(node->minimum, std::min(node->maximum, value)); break;
        case nector_property_minimum:
            if (value > node->maximum) return nectorFail(nector_invalid_argument, "minimum exceeds maximum");
            node->minimum = value;
            node->value = std::max(node->minimum, std::min(node->maximum, node->value));
            break;
        case nector_property_maximum:
            if (value < node->minimum) return nectorFail(nector_invalid_argument, "maximum is below minimum");
            node->maximum = value;
            node->value = std::max(node->minimum, std::min(node->maximum, node->value));
            break;
        case nector_property_step: node->step = value; break;
        case nector_property_caret:
            node->caret = std::min(value, static_cast<double>(sceneUtf8Length(node->text)));
            break;
        case nector_property_corner_radius: node->corner_radius = value; break;
        case nector_property_content_offset_x: node->content_offset_x = value; break;
        case nector_property_content_offset_y: node->content_offset_y = value; break;
        case nector_property_scroll_step: node->scroll_step = value; break;
        case nector_property_layout: node->layout = static_cast<int32_t>(value); break;
        default: return nectorFail(nector_invalid_argument, "property is not numeric");
    }
    return nector_ok;
}

nector_status sceneEmitNodeEvent(nector_scene_handle scene, nector_ui_event_type type,
                                 nector_node_handle node, const char* text, const char* name,
                                 double value, nector_painter_handle painter) {
    if (scene == nullptr || scene->callback == nullptr) return nector_ok;
    if (!std::isfinite(value)) return nectorFail(nector_invalid_argument, "event value is not finite");
    if (node != nullptr && !sceneNodeValid(node)) return nector_ok;
    const nector_scene_callback callback = scene->callback;
    void* const user_data = scene->user_data;
    const std::string id_copy = node != nullptr ? node->id : std::string();
    const std::string text_copy = text != nullptr ? text : (node != nullptr ? node->text : std::string());
    const std::string name_copy = name != nullptr ? name : std::string();
    nector_ui_event event{};
    event.type = type;
    event.node = node;
    event.node_id = node != nullptr ? id_copy.c_str() : nullptr;
    event.text = (text != nullptr || node != nullptr) ? text_copy.c_str() : nullptr;
    event.name = name != nullptr ? name_copy.c_str() : nullptr;
    event.value = value;
    event.painter = painter;
    event.bounds = node != nullptr ? node->bounds : nector_rect{0, 0, 0, 0};
    callback(&event, user_data);
    return nector_ok;
}

nector_status sceneSetFocus(nector_scene_handle scene, nector_node_handle node) {
    if (scene == nullptr) return nectorFail(nector_invalid_argument, "scene is null");
    if (node != nullptr && !sceneNodeFocusable(node)) return nector_ok;
    if (scene->focused_node == node) return nector_ok;
    nector_node_handle previous = scene->focused_node;
    scene->focused_node = nullptr;
    if (previous != nullptr && sceneNodeValid(previous)) {
        const nector_status status = sceneEmitNodeEvent(scene, nector_ui_blur, previous, nullptr, nullptr, 0, nullptr);
        if (status != nector_ok) return status;
    }
    if (scene->focused_node != nullptr || node == nullptr || !sceneNodeFocusable(node)) return nector_ok;
    scene->focused_node = node;
    return sceneEmitNodeEvent(scene, nector_ui_focus, node, nullptr, nullptr, 0, nullptr);
}

nector_status nectorCreateScene(const char* title, int32_t width, int32_t height, nector_scene_handle* out_scene) {
    if (out_scene == nullptr) return nectorFail(nector_invalid_argument, "out_scene is null");
    *out_scene = nullptr;
    if (width <= 0 || height <= 0 || width > static_cast<int32_t>(max_scene_dimension) ||
        height > static_cast<int32_t>(max_scene_dimension)) {
        return nectorFail(nector_invalid_argument, "scene dimensions are invalid");
    }
    const nector_status title_status = validateText(title, max_scene_text_size, true);
    if (title_status != nector_ok) return title_status;
    nector_api_try {
        std::unique_ptr<nector_scene> scene(new nector_scene);
        scene->title = title;
        scene->width = width;
        scene->height = height;
        std::unique_ptr<nector_node> root(new nector_node);
        sceneInitializeNode(root.get(), scene.get(), nector_node_window, "root");
        root->title = scene->title;
        scene->nodes.emplace(root->id, root.get());
        scene->root = std::move(root);
        const nector_status layout_status = nectorSceneLayout(scene.get(), width, height);
        if (layout_status != nector_ok) return layout_status;
        *out_scene = scene.release();
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyScene(nector_scene_handle scene) {
    if (scene == nullptr) return;
    if (scene->operation_depth != 0) {
        scene->destroy_pending = true;
        return;
    }
    delete scene;
}

nector_status nectorSceneGetRoot(nector_scene_handle scene, nector_node_handle* out_root) {
    if (out_root == nullptr) return nectorFail(nector_invalid_argument, "out_root is null");
    *out_root = nullptr;
    if (scene == nullptr) return nectorFail(nector_invalid_argument, "scene is null");
    SceneGuard guard(scene);
    *out_root = scene->root.get();
    return nector_ok;
}

nector_status nectorSceneAddNode(nector_scene_handle scene, nector_node_handle parent,
                                 nector_node_type type, const char* id, nector_node_handle* out_node) {
    if (out_node == nullptr) return nectorFail(nector_invalid_argument, "out_node is null");
    *out_node = nullptr;
    if (scene == nullptr || !sceneNodeValid(parent) || parent->scene != scene) {
        return nectorFail(nector_invalid_argument, "scene or parent is invalid");
    }
    SceneGuard guard(scene);
    if (!sceneNodeContainer(parent->type)) return nectorFail(nector_invalid_argument, "parent is not a container");
    if (type < nector_node_column || type > nector_node_image) {
        return nectorFail(nector_invalid_argument, "node type is invalid for a child");
    }
    const nector_status id_status = validateText(id, max_scene_id_size, false);
    if (id_status != nector_ok) return id_status;
    if (scene->nodes.find(id) != scene->nodes.end()) return nectorFail(nector_invalid_state, "node id already exists");
    nector_api_try {
        std::unique_ptr<nector_node> node(new nector_node);
        sceneInitializeNode(node.get(), scene, type, id);
        node->parent = parent;
        nector_node_handle raw_node = node.get();
        scene->nodes.emplace(node->id, raw_node);
        try {
            parent->children.push_back(std::move(node));
        } catch (...) {
            scene->nodes.erase(raw_node->id);
            throw;
        }
        *out_node = raw_node;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorSceneFindNode(nector_scene_handle scene, const char* id, nector_node_handle* out_node) {
    if (out_node == nullptr) return nectorFail(nector_invalid_argument, "out_node is null");
    *out_node = nullptr;
    if (scene == nullptr) return nectorFail(nector_invalid_argument, "scene is null");
    SceneGuard guard(scene);
    const nector_status id_status = validateText(id, max_scene_id_size, false);
    if (id_status != nector_ok) return id_status;
    const auto found = scene->nodes.find(id);
    if (found == scene->nodes.end()) return nectorFail(nector_not_found, "node id was not found");
    *out_node = found->second;
    return nector_ok;
}

nector_status nectorSceneRemoveNode(nector_scene_handle scene, nector_node_handle node) {
    if (scene == nullptr || !sceneNodeValid(node) || node->scene != scene) {
        return nectorFail(nector_invalid_argument, "scene or node is invalid");
    }
    SceneGuard guard(scene);
    if (node == scene->root.get()) return nectorFail(nector_invalid_state, "the root node cannot be removed");
    nector_node_handle parent = node->parent;
    if (parent == nullptr) return nectorFail(nector_invalid_state, "node has no parent");
    nector_api_try {
        auto found = std::find_if(parent->children.begin(), parent->children.end(),
                                  [node](const auto& child) { return child.get() == node; });
        if (found == parent->children.end()) return nectorFail(nector_invalid_state, "node is detached from its parent");
        markRemoved(scene, node);
        std::unique_ptr<nector_node> removed = std::move(*found);
        parent->children.erase(found);
        scene->retired_nodes.push_back(std::move(removed));
        return nector_ok;
    } nector_api_catch
}

nector_status nectorSceneSetNodeString(nector_node_handle node, nector_node_property property, const char* value) {
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    nector_api_try {
        return sceneSetNodeString(node, property, value);
    } nector_api_catch
}

nector_status nectorSceneSetNodeNumber(nector_node_handle node, nector_node_property property, double value) {
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    return sceneSetNodeNumber(node, property, value);
}

nector_status nectorSceneSetNodeBool(nector_node_handle node, nector_node_property property, int32_t value) {
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (value != 0 && value != 1) return nectorFail(nector_invalid_argument, "boolean value must be zero or one");
    if (property == nector_property_checked && node->type != nector_node_check_box) {
        return nectorFail(nector_invalid_argument, "checked is only valid for check boxes");
    }
    if (property == nector_property_word_wrap && node->type != nector_node_label) {
        return nectorFail(nector_invalid_argument, "word wrap is only valid for labels");
    }
    if (property == nector_property_no_shrink && !sceneNodeContainer(node->type)) {
        return nectorFail(nector_invalid_argument, "no shrink is only valid for containers");
    }
    if (property == nector_property_glass && node->type != nector_node_button) {
        return nectorFail(nector_invalid_argument, "glass is only valid for buttons");
    }
    switch (property) {
        case nector_property_visible: node->visible = value; break;
        case nector_property_enabled: node->enabled = value; break;
        case nector_property_checked: node->checked = value; break;
        case nector_property_word_wrap: node->word_wrap = value; break;
        case nector_property_no_shrink: node->no_shrink = value; break;
        case nector_property_glass: node->glass = value; break;
        default: return nectorFail(nector_invalid_argument, "property is not boolean");
    }
    return nector_ok;
}

nector_status nectorSceneSetNodeColor(nector_node_handle node, nector_node_property property, nector_color value) {
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (!colorSupported(property) || !validColor(value)) return nectorFail(nector_invalid_argument, "color or property is invalid");
    switch (property) {
        case nector_property_background: node->background = value; break;
        case nector_property_foreground: node->foreground = value; break;
        case nector_property_border: node->border = value; break;
        case nector_property_accent: node->accent = value; break;
        default: return nectorFail(nector_invalid_argument, "property is not a color");
    }
    return nector_ok;
}

nector_status nectorSceneGetNodeString(nector_node_handle node, nector_node_property property, const char** out_value) {
    if (out_value == nullptr) return nectorFail(nector_invalid_argument, "out_value is null");
    *out_value = nullptr;
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (!stringSupported(node, property)) return nectorFail(nector_invalid_argument, "string property is not supported by this node");
    *out_value = stringValue(node, property).c_str();
    return nector_ok;
}

nector_status nectorSceneGetNodeNumber(nector_node_handle node, nector_node_property property, double* out_value) {
    if (out_value == nullptr) return nectorFail(nector_invalid_argument, "out_value is null");
    *out_value = 0;
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (!numberSupported(node, property)) return nectorFail(nector_invalid_argument, "numeric property is not supported by this node");
    switch (property) {
        case nector_property_width: *out_value = node->width; break;
        case nector_property_height: *out_value = node->height; break;
        case nector_property_padding: *out_value = node->padding; break;
        case nector_property_gap: *out_value = node->gap; break;
        case nector_property_flex: *out_value = node->flex; break;
        case nector_property_font_size: *out_value = node->font_size; break;
        case nector_property_value: *out_value = node->value; break;
        case nector_property_minimum: *out_value = node->minimum; break;
        case nector_property_maximum: *out_value = node->maximum; break;
        case nector_property_step: *out_value = node->step; break;
        case nector_property_caret: *out_value = node->caret; break;
        case nector_property_corner_radius: *out_value = node->corner_radius; break;
        case nector_property_content_offset_x: *out_value = node->content_offset_x; break;
        case nector_property_content_offset_y: *out_value = node->content_offset_y; break;
        case nector_property_scroll_step: *out_value = node->scroll_step; break;
        case nector_property_layout: *out_value = node->layout; break;
        default: return nectorFail(nector_invalid_argument, "property is not numeric");
    }
    return nector_ok;
}

nector_status nectorSceneGetNodeBool(nector_node_handle node, nector_node_property property, int32_t* out_value) {
    if (out_value == nullptr) return nectorFail(nector_invalid_argument, "out_value is null");
    *out_value = 0;
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (property == nector_property_checked && node->type != nector_node_check_box) {
        return nectorFail(nector_invalid_argument, "checked is only valid for check boxes");
    }
    if (property == nector_property_word_wrap && node->type != nector_node_label) {
        return nectorFail(nector_invalid_argument, "word wrap is only valid for labels");
    }
    if (property == nector_property_no_shrink && !sceneNodeContainer(node->type)) {
        return nectorFail(nector_invalid_argument, "no shrink is only valid for containers");
    }
    if (property == nector_property_glass && node->type != nector_node_button) {
        return nectorFail(nector_invalid_argument, "glass is only valid for buttons");
    }
    switch (property) {
        case nector_property_visible: *out_value = node->visible; break;
        case nector_property_enabled: *out_value = node->enabled; break;
        case nector_property_checked: *out_value = node->checked; break;
        case nector_property_word_wrap: *out_value = node->word_wrap; break;
        case nector_property_no_shrink: *out_value = node->no_shrink; break;
        case nector_property_glass: *out_value = node->glass; break;
        default: return nectorFail(nector_invalid_argument, "property is not boolean");
    }
    return nector_ok;
}

nector_status nectorSceneGetNodeColor(nector_node_handle node, nector_node_property property, nector_color* out_value) {
    if (out_value == nullptr) return nectorFail(nector_invalid_argument, "out_value is null");
    *out_value = {0, 0, 0, 0};
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (!colorSupported(property)) return nectorFail(nector_invalid_argument, "property is not a color");
    switch (property) {
        case nector_property_background: *out_value = node->background; break;
        case nector_property_foreground: *out_value = node->foreground; break;
        case nector_property_border: *out_value = node->border; break;
        case nector_property_accent: *out_value = node->accent; break;
        default: return nectorFail(nector_invalid_argument, "property is not a color");
    }
    return nector_ok;
}

nector_status nectorSceneGetNodeType(nector_node_handle node, nector_node_type* out_type) {
    if (out_type == nullptr) return nectorFail(nector_invalid_argument, "out_type is null");
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    *out_type = node->type;
    return nector_ok;
}

nector_status nectorSceneGetNodeId(nector_node_handle node, const char** out_id) {
    if (out_id == nullptr) return nectorFail(nector_invalid_argument, "out_id is null");
    *out_id = nullptr;
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    *out_id = node->id.c_str();
    return nector_ok;
}

nector_status nectorSceneGetNodeBounds(nector_node_handle node, nector_rect* out_bounds) {
    if (out_bounds == nullptr) return nectorFail(nector_invalid_argument, "out_bounds is null");
    *out_bounds = {0, 0, 0, 0};
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    *out_bounds = node->bounds;
    return nector_ok;
}

nector_status nectorSceneSetNodeImage(nector_node_handle node, nector_image_handle image) {
    if (!sceneNodeValid(node)) return nectorFail(nector_invalid_argument, "node is invalid");
    SceneGuard guard(node->scene);
    if (node->type != nector_node_image) return nectorFail(nector_invalid_argument, "images can only be bound to image nodes");
    node->image = image;
    return nector_ok;
}

nector_status nectorSceneSetFont(nector_scene_handle scene, nector_font_handle font) {
    if (scene == nullptr) return nectorFail(nector_invalid_argument, "scene is null");
    SceneGuard guard(scene);
    scene->font = font;
    return nector_ok;
}

nector_status nectorSceneSetEventCallback(nector_scene_handle scene, nector_scene_callback callback, void* user_data) {
    if (scene == nullptr) return nectorFail(nector_invalid_argument, "scene is null");
    SceneGuard guard(scene);
    scene->callback = callback;
    scene->user_data = user_data;
    return nector_ok;
}

nector_status nectorSceneEmit(nector_scene_handle scene, const char* name, const char* text, double value) {
    if (scene == nullptr || !std::isfinite(value)) return nectorFail(nector_invalid_argument, "scene or event value is invalid");
    SceneGuard guard(scene);
    const nector_status name_status = validateText(name, max_scene_id_size, false);
    if (name_status != nector_ok) return name_status;
    if (text != nullptr) {
        const nector_status text_status = validateText(text, max_scene_text_size, true);
        if (text_status != nector_ok) return text_status;
    }
    nector_api_try {
        return sceneEmitNodeEvent(scene, nector_ui_custom, nullptr, text, name, value, nullptr);
    } nector_api_catch
}

nector_status nectorSceneGetSize(nector_scene_handle scene, int32_t* out_width, int32_t* out_height) {
    if (scene == nullptr || out_width == nullptr || out_height == nullptr) {
        return nectorFail(nector_invalid_argument, "scene or size output is null");
    }
    SceneGuard guard(scene);
    *out_width = scene->width;
    *out_height = scene->height;
    return nector_ok;
}

nector_status nectorSceneGetTitle(nector_scene_handle scene, const char** out_title) {
    if (out_title == nullptr) return nectorFail(nector_invalid_argument, "out_title is null");
    *out_title = nullptr;
    if (scene == nullptr) return nectorFail(nector_invalid_argument, "scene is null");
    SceneGuard guard(scene);
    *out_title = scene->title.c_str();
    return nector_ok;
}
