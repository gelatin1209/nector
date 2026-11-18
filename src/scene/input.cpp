#include "internal.h"

#include <cstring>

namespace {

bool primaryButton(int32_t button) {
    return button == 0 || button == 1;
}

nector_node_handle hitNode(nector_node_handle node, float x, float y) {
    if (!sceneNodeVisible(node) || !sceneNodeEnabled(node) || !sceneRectContains(node->clip_bounds, x, y)) {
        return nullptr;
    }
    for (auto child = node->children.rbegin(); child != node->children.rend(); ++child) {
        nector_node_handle hit = hitNode(child->get(), x, y);
        if (hit != nullptr) return hit;
    }
    return node;
}

bool interactiveNode(nector_node_handle node) {
    if (!sceneNodeEnabled(node) || !sceneNodeVisible(node)) return false;
    return node->type == nector_node_button || node->type == nector_node_line_edit ||
           node->type == nector_node_check_box || node->type == nector_node_slider;
}

nector_status updateSlider(nector_scene_handle scene, nector_node_handle node, float x) {
    if (!sceneNodeValid(node) || node->type != nector_node_slider || !sceneNodeEnabled(node)) return nector_ok;
    const float left = node->bounds.x + 8;
    const float width = std::max(0.0f, node->bounds.width - 16);
    const double fraction = width > 0 ? std::max(0.0, std::min(1.0, static_cast<double>(x - left) / width)) : 0.0;
    double value = node->minimum + (node->maximum - node->minimum) * fraction;
    if (node->step > 0) value = node->minimum + std::round((value - node->minimum) / node->step) * node->step;
    value = std::max(node->minimum, std::min(node->maximum, value));
    if (value == node->value) return nector_ok;
    node->value = value;
    return sceneEmitNodeEvent(scene, nector_ui_value_changed, node, nullptr, nullptr, value, nullptr);
}

nector_status changeSliderValue(nector_scene_handle scene, nector_node_handle node, double direction) {
    if (!sceneNodeValid(node) || node->type != nector_node_slider) return nector_ok;
    const double value = std::max(node->minimum, std::min(node->maximum, node->value + direction * node->step));
    if (value == node->value) return nector_ok;
    node->value = value;
    return sceneEmitNodeEvent(scene, nector_ui_value_changed, node, nullptr, nullptr, value, nullptr);
}

nector_status activateNode(nector_scene_handle scene, nector_node_handle node) {
    if (!sceneNodeValid(node) || !sceneNodeEnabled(node) || !sceneNodeVisible(node)) return nector_ok;
    if (node->type != nector_node_button && node->type != nector_node_check_box &&
        node->type != nector_node_line_edit) {
        return nector_ok;
    }
    nector_status status = sceneEmitNodeEvent(scene, nector_ui_click, node, nullptr, nullptr, 0, nullptr);
    if (status != nector_ok || !sceneNodeValid(node) || node->type != nector_node_check_box) return status;
    node->checked = node->checked ? 0 : 1;
    return sceneEmitNodeEvent(scene, nector_ui_toggled, node, nullptr, nullptr, node->checked, nullptr);
}

void collectFocusable(nector_node_handle node, std::vector<nector_node_handle>* output) {
    if (!sceneNodeVisible(node) || !sceneNodeEnabled(node)) return;
    if (sceneNodeFocusable(node)) output->push_back(node);
    for (const auto& child : node->children) collectFocusable(child.get(), output);
}

nector_status navigateFocus(nector_scene_handle scene, bool reverse) {
    std::vector<nector_node_handle> nodes;
    collectFocusable(scene->root.get(), &nodes);
    if (nodes.empty()) return sceneSetFocus(scene, nullptr);
    size_t index = nodes.size();
    for (size_t current = 0; current < nodes.size(); ++current) {
        if (nodes[current] == scene->focused_node) {
            index = current;
            break;
        }
    }
    size_t next = 0;
    if (index == nodes.size()) next = reverse ? nodes.size() - 1 : 0;
    else if (reverse) next = index == 0 ? nodes.size() - 1 : index - 1;
    else next = (index + 1) % nodes.size();
    return sceneSetFocus(scene, nodes[next]);
}

void setCaretFromPoint(nector_scene_handle scene, nector_node_handle node, float x) {
    if (!sceneNodeValid(node) || node->type != nector_node_line_edit) return;
    const float padding = static_cast<float>(node->padding);
    const float available = std::max(0.0f, node->bounds.width - 2.0f * padding);
    const std::vector<size_t> offsets = sceneUtf8Offsets(node->text);
    const size_t old_offset = sceneByteOffsetForCaret(node->text, node->caret);
    const float old_width = sceneTextWidth(scene, node, node->text.substr(0, old_offset));
    float origin = node->bounds.x + padding;
    if (old_width > available) origin -= old_width - available;
    size_t best_index = 0;
    double best_distance = std::numeric_limits<double>::max();
    for (size_t index = 0; index < offsets.size(); ++index) {
        const float width = sceneTextWidth(scene, node, node->text.substr(0, offsets[index]));
        const double distance = std::fabs(static_cast<double>(x) - (origin + width));
        if (distance < best_distance) {
            best_distance = distance;
            best_index = index;
        }
    }
    node->caret = static_cast<double>(best_index);
}

nector_status mouseDown(nector_scene_handle scene, float x, float y, int32_t button) {
    if (!primaryButton(button)) return nector_ok;
    nector_node_handle hit = hitNode(scene->root.get(), x, y);
    if (hit == nullptr || !interactiveNode(hit)) {
        scene->pressed_node = nullptr;
        return sceneSetFocus(scene, nullptr);
    }
    nector_status status = sceneSetFocus(scene, hit);
    if (status != nector_ok || !sceneNodeValid(hit)) return status;
    scene->pressed_node = hit;
    if (hit->type == nector_node_line_edit) setCaretFromPoint(scene, hit, x);
    if (hit->type == nector_node_slider) status = updateSlider(scene, hit, x);
    return status;
}

nector_status mouseMove(nector_scene_handle scene, float x, float y) {
    (void)y;
    nector_node_handle pressed = scene->pressed_node;
    if (pressed != nullptr && sceneNodeValid(pressed) && pressed->type == nector_node_slider) {
        return updateSlider(scene, pressed, x);
    }
    return nector_ok;
}

nector_status mouseUp(nector_scene_handle scene, float x, float y, int32_t button) {
    if (!primaryButton(button)) return nector_ok;
    nector_node_handle pressed = scene->pressed_node;
    scene->pressed_node = nullptr;
    if (pressed == nullptr || !sceneNodeValid(pressed)) return nector_ok;
    if (pressed->type == nector_node_slider) return updateSlider(scene, pressed, x);
    if ((pressed->type == nector_node_button || pressed->type == nector_node_check_box) &&
        hitNode(scene->root.get(), x, y) == pressed) {
        return activateNode(scene, pressed);
    }
    return nector_ok;
}

nector_status editText(nector_scene_handle scene, const char* text) {
    nector_node_handle node = scene->focused_node;
    if (!sceneNodeValid(node) || node->type != nector_node_line_edit || !sceneNodeEnabled(node)) return nector_ok;
    if (std::strchr(text, '\n') != nullptr || std::strchr(text, '\r') != nullptr) return nector_ok;
    const size_t insertion_size = std::strlen(text);
    if (insertion_size > max_scene_text_size - node->text.size()) {
        return nectorFail(nector_limit_exceeded, "line edit text limit exceeded");
    }
    const size_t caret_offset = sceneByteOffsetForCaret(node->text, node->caret);
    std::string inserted(text);
    const size_t inserted_carets = sceneUtf8Length(inserted);
    node->text.insert(caret_offset, inserted);
    node->caret += static_cast<double>(inserted_carets);
    return sceneEmitNodeEvent(scene, nector_ui_text_changed, node, node->text.c_str(), nullptr, 0, nullptr);
}

nector_status editKey(nector_scene_handle scene, nector_node_handle node, int32_t key) {
    if (!sceneNodeValid(node) || node->type != nector_node_line_edit) return nector_ok;
    std::vector<size_t> offsets = sceneUtf8Offsets(node->text);
    size_t caret = static_cast<size_t>(std::max(0.0, std::min(node->caret, static_cast<double>(offsets.size() - 1))));
    bool changed = false;
    switch (key) {
        case nector_key_left:
            if (caret > 0) --caret;
            break;
        case nector_key_right:
            if (caret + 1 < offsets.size()) ++caret;
            break;
        case nector_key_home:
            caret = 0;
            break;
        case nector_key_end:
            caret = offsets.size() - 1;
            break;
        case nector_key_backspace:
            if (caret > 0) {
                node->text.erase(offsets[caret - 1], offsets[caret] - offsets[caret - 1]);
                --caret;
                changed = true;
            }
            break;
        case nector_key_delete:
            if (caret + 1 < offsets.size()) {
                node->text.erase(offsets[caret], offsets[caret + 1] - offsets[caret]);
                changed = true;
            }
            break;
        case nector_key_enter:
            return activateNode(scene, node);
        default:
            return nector_ok;
    }
    node->caret = static_cast<double>(caret);
    if (changed) {
        return sceneEmitNodeEvent(scene, nector_ui_text_changed, node, node->text.c_str(), nullptr, 0, nullptr);
    }
    return nector_ok;
}

nector_status keyDown(nector_scene_handle scene, const nector_input_event* event) {
    if (event->key == nector_key_shift) {
        scene->shift_down = true;
        return nector_ok;
    }
    if (event->key == nector_key_tab) {
        return navigateFocus(scene, scene->shift_down || (event->modifiers & 1u) != 0);
    }
    nector_node_handle focused = scene->focused_node;
    if (!sceneNodeValid(focused) || !sceneNodeEnabled(focused)) return nector_ok;
    if (focused->type == nector_node_line_edit) return editKey(scene, focused, event->key);
    if (focused->type == nector_node_slider) {
        if (event->key == nector_key_left || event->key == nector_key_down) return changeSliderValue(scene, focused, -1);
        if (event->key == nector_key_right || event->key == nector_key_up) return changeSliderValue(scene, focused, 1);
        if (event->key == nector_key_home) {
            const double old_value = focused->value;
            focused->value = focused->minimum;
            if (old_value != focused->value) {
                return sceneEmitNodeEvent(scene, nector_ui_value_changed, focused, nullptr, nullptr, focused->value, nullptr);
            }
        }
        if (event->key == nector_key_end) {
            const double old_value = focused->value;
            focused->value = focused->maximum;
            if (old_value != focused->value) {
                return sceneEmitNodeEvent(scene, nector_ui_value_changed, focused, nullptr, nullptr, focused->value, nullptr);
            }
        }
        return nector_ok;
    }
    if (event->key == nector_key_enter || event->key == nector_key_space) return activateNode(scene, focused);
    return nector_ok;
}

nector_status scrollWheel(nector_scene_handle scene, nector_node_handle hit,
                          const nector_input_event* event) {
    nector_node_handle container = hit;
    while (container != nullptr && (!sceneNodeContainer(container->type) || container->scroll_step <= 0)) {
        container = container->parent;
    }
    if (container == nullptr || !sceneNodeEnabled(container)) return nector_ok;
    const bool horizontal = sceneNodeHorizontal(container);
    const double delta = horizontal && event->dy == 0 ? event->dx : event->dy;
    if (delta == 0) return nector_ok;

    const float inner_start = horizontal ? container->bounds.x + static_cast<float>(container->padding)
                                         : container->bounds.y + static_cast<float>(container->padding);
    const float inner_size = horizontal
        ? std::max(0.0f, container->bounds.width - 2.0f * static_cast<float>(container->padding))
        : std::max(0.0f, container->bounds.height - 2.0f * static_cast<float>(container->padding));
    const double offset = horizontal ? container->content_offset_x : container->content_offset_y;
    float content_size = 0;
    for (const auto& child : container->children) {
        if (!child->visible) continue;
        const float end = horizontal ? child->bounds.x + child->bounds.width
                                     : child->bounds.y + child->bounds.height;
        content_size = std::max(content_size, end - inner_start - static_cast<float>(offset));
    }
    const double maximum = std::max(0.0, static_cast<double>(content_size - inner_size));
    const double current = std::max(0.0, std::min(maximum, -offset));
    const double next = std::max(0.0, std::min(maximum, current - delta * container->scroll_step));
    if (next == current) return nector_ok;
    if (horizontal) container->content_offset_x = -next;
    else container->content_offset_y = -next;
    const nector_status layout_status = sceneLayoutInternal(scene);
    if (layout_status != nector_ok) return layout_status;
    return sceneEmitNodeEvent(scene, nector_ui_scroll, container, nullptr,
                              horizontal ? "x" : "y", next, nullptr);
}

nector_status touchInput(nector_scene_handle scene, const nector_input_event* event) {
    if (event->type == nector_event_touch_begin) {
        if (scene->has_active_touch) return nector_ok;
        scene->has_active_touch = true;
        scene->active_touch_id = event->touch_id;
        return mouseDown(scene, event->x, event->y, 0);
    }
    if (!scene->has_active_touch || scene->active_touch_id != event->touch_id) return nector_ok;
    if (event->type == nector_event_touch_move) return mouseMove(scene, event->x, event->y);
    scene->has_active_touch = false;
    if (event->type == nector_event_touch_end) return mouseUp(scene, event->x, event->y, 0);
    scene->pressed_node = nullptr;
    return nector_ok;
}

} // namespace

nector_status sceneInputInternal(nector_scene_handle scene, const nector_input_event* event) {
    switch (event->type) {
        case nector_event_none:
            return nector_ok;
        case nector_event_key_down:
            return keyDown(scene, event);
        case nector_event_key_up:
            if (event->key == nector_key_shift) scene->shift_down = false;
            return nector_ok;
        case nector_event_text_input:
            return editText(scene, event->text);
        case nector_event_mouse_move:
            return mouseMove(scene, event->x, event->y);
        case nector_event_mouse_down:
            return mouseDown(scene, event->x, event->y, event->button);
        case nector_event_mouse_up:
            return mouseUp(scene, event->x, event->y, event->button);
        case nector_event_mouse_wheel: {
            nector_node_handle hit = hitNode(scene->root.get(), event->x, event->y);
            if (hit != nullptr && hit->type == nector_node_slider && event->dy != 0) {
                return changeSliderValue(scene, hit, event->dy > 0 ? 1 : -1);
            }
            return scrollWheel(scene, hit, event);
        }
        case nector_event_touch_begin:
        case nector_event_touch_move:
        case nector_event_touch_end:
        case nector_event_touch_cancel:
            return touchInput(scene, event);
        case nector_event_focus_lost:
            scene->pressed_node = nullptr;
            scene->has_active_touch = false;
            scene->shift_down = false;
            return sceneSetFocus(scene, nullptr);
        case nector_event_window_close:
            return sceneEmitNodeEvent(scene, nector_ui_quit, nullptr, nullptr, nullptr, 0, nullptr);
        case nector_event_window_resize:
            if (event->width <= 0 || event->height <= 0 ||
                event->width > static_cast<int32_t>(max_scene_dimension) ||
                event->height > static_cast<int32_t>(max_scene_dimension)) {
                return nectorFail(nector_invalid_argument, "resize dimensions are invalid");
            }
            scene->width = event->width;
            scene->height = event->height;
            return sceneLayoutInternal(scene);
        default:
            return nectorFail(nector_invalid_argument, "input event type is invalid");
    }
}

nector_status nectorSceneHandleInput(nector_scene_handle scene, const nector_input_event* input_event) {
    if (scene == nullptr || input_event == nullptr) return nectorFail(nector_invalid_argument, "scene or input event is null");
    SceneGuard guard(scene);
    if (input_event->type < nector_event_none || input_event->type > nector_event_window_resize ||
        !nectorFinite(input_event->x) || !nectorFinite(input_event->y) || !nectorFinite(input_event->dx) ||
        !nectorFinite(input_event->dy) || !nectorFinite(input_event->pressure)) {
        return nectorFail(nector_invalid_argument, "input event is invalid");
    }
    if (input_event->type == nector_event_text_input) {
        size_t length = 0;
        while (length < sizeof(input_event->text) && input_event->text[length] != '\0') ++length;
        if (length == sizeof(input_event->text)) return nectorFail(nector_invalid_argument, "text input is not null terminated");
        const nector_status utf8_status = nectorCoreValidateUtf8(input_event->text, length);
        if (utf8_status != nector_ok) return utf8_status;
    }
    nector_api_try {
        return sceneInputInternal(scene, input_event);
    } nector_api_catch
}
