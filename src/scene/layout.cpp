#include "internal.h"

namespace {

struct Size {
    float width = 0;
    float height = 0;
};

struct ChildMeasure {
    nector_node_handle node = nullptr;
    float main_size = 0;
    float cross_size = 0;
    double flex = 0;
};

float boundedDimension(double value) {
    if (!std::isfinite(value) || value <= 0) return 0;
    return static_cast<float>(std::min(value, max_scene_dimension));
}

float addDimension(float first, float second) {
    return boundedDimension(static_cast<double>(first) + static_cast<double>(second));
}

float nodeTextWidth(nector_scene_handle scene, nector_node_handle node) {
    return sceneTextWidth(scene, node, node->text);
}

Size measureNode(nector_scene_handle scene, nector_node_handle node, float cross_hint);

Size measureChildren(nector_scene_handle scene, nector_node_handle node) {
    Size result;
    size_t visible_count = 0;
    const bool horizontal = sceneNodeHorizontal(node);
    for (const auto& child : node->children) {
        if (!child->visible) continue;
        const Size child_size = measureNode(scene, child.get(), 0);
        if (horizontal) {
            result.width = addDimension(result.width, child_size.width);
            result.height = std::max(result.height, child_size.height);
        } else {
            result.height = addDimension(result.height, child_size.height);
            result.width = std::max(result.width, child_size.width);
        }
        ++visible_count;
    }
    if (visible_count > 1) {
        const float gaps = boundedDimension(node->gap * static_cast<double>(visible_count - 1));
        if (horizontal) result.width = addDimension(result.width, gaps);
        else result.height = addDimension(result.height, gaps);
    }
    return result;
}

Size measureNode(nector_scene_handle scene, nector_node_handle node, float cross_hint) {
    Size size;
    const float padding = boundedDimension(node->padding) * 2.0f;
    const float line_height = sceneLineHeight(scene, node);
    switch (node->type) {
        case nector_node_window:
        case nector_node_column:
        case nector_node_row:
            size = measureChildren(scene, node);
            break;
        case nector_node_label: {
            const float text_width = nodeTextWidth(scene, node);
            size.width = addDimension(text_width, padding);
            float lines = 1;
            for (char character : node->text) {
                if (character == '\n') lines += 1;
            }
            if (node->word_wrap && cross_hint > padding && text_width > cross_hint - padding) {
                const float available = cross_hint - padding;
                lines = std::max(lines, std::ceil(text_width / std::max(1.0f, available)));
            }
            size.height = addDimension(line_height * lines, padding);
            break;
        }
        case nector_node_button:
            size.width = addDimension(std::max(80.0f, nodeTextWidth(scene, node)), padding);
            size.height = addDimension(std::max(30.0f, line_height), padding);
            break;
        case nector_node_line_edit:
            size.width = addDimension(180.0f, padding);
            size.height = addDimension(std::max(30.0f, line_height), padding);
            break;
        case nector_node_check_box:
            size.width = addDimension(28.0f + nodeTextWidth(scene, node), padding);
            size.height = addDimension(std::max(22.0f, line_height), padding);
            break;
        case nector_node_slider:
            size = {180, 28};
            break;
        case nector_node_progress_bar:
            size = {180, 18};
            break;
        case nector_node_spacer:
            break;
        case nector_node_canvas:
            size = {160, 120};
            break;
        case nector_node_image: {
            if (node->image != nullptr) {
                int32_t image_width = 0;
                int32_t image_height = 0;
                if (nectorImageGetSize(node->image, &image_width, &image_height) == nector_ok) {
                    size.width = boundedDimension(image_width);
                    size.height = boundedDimension(image_height);
                }
            }
            break;
        }
        default:
            break;
    }
    size.width = addDimension(size.width, sceneNodeContainer(node->type) ? padding : 0);
    size.height = addDimension(size.height, sceneNodeContainer(node->type) ? padding : 0);
    if (node->width > 0) size.width = boundedDimension(node->width);
    if (node->height > 0) size.height = boundedDimension(node->height);
    return size;
}

void clearLayout(nector_node_handle node) {
    node->bounds = {0, 0, 0, 0};
    node->clip_bounds = {0, 0, 0, 0};
    for (const auto& child : node->children) clearLayout(child.get());
}

void layoutNode(nector_scene_handle scene, nector_node_handle node, nector_rect bounds,
                nector_rect parent_clip) {
    if (!node->visible) {
        clearLayout(node);
        return;
    }
    bounds.x = sceneClampFloat(bounds.x, -max_scene_dimension, max_scene_dimension);
    bounds.y = sceneClampFloat(bounds.y, -max_scene_dimension, max_scene_dimension);
    bounds.width = boundedDimension(bounds.width);
    bounds.height = boundedDimension(bounds.height);
    node->bounds = bounds;
    node->clip_bounds = sceneIntersectRect(bounds, parent_clip);
    if (!sceneNodeContainer(node->type) || node->clip_bounds.width <= 0 || node->clip_bounds.height <= 0) {
        if (!sceneNodeContainer(node->type)) return;
    }

    const float padding = boundedDimension(node->padding);
    const float inner_x = bounds.x + padding;
    const float inner_y = bounds.y + padding;
    const float inner_width = boundedDimension(bounds.width - 2.0f * padding);
    const float inner_height = boundedDimension(bounds.height - 2.0f * padding);
    const bool horizontal = sceneNodeHorizontal(node);
    const float inner_main = horizontal ? inner_width : inner_height;
    const float inner_cross = horizontal ? inner_height : inner_width;

    std::vector<ChildMeasure> measurements;
    measurements.reserve(node->children.size());
    double total_flex = 0;
    float used_main = 0;
    size_t visible_count = 0;
    for (const auto& child : node->children) {
        if (!child->visible) continue;
        const float cross_hint = horizontal ? 0.0f : (child->width > 0 ? boundedDimension(child->width) : inner_cross);
        const Size natural = measureNode(scene, child.get(), cross_hint);
        ChildMeasure measure;
        measure.node = child.get();
        measure.main_size = horizontal ? natural.width : natural.height;
        measure.cross_size = horizontal ? natural.height : natural.width;
        measure.flex = child->flex;
        total_flex += child->flex;
        used_main = addDimension(used_main, measure.main_size);
        measurements.push_back(measure);
        ++visible_count;
    }
    if (measurements.empty()) return;
    const float total_gap = visible_count > 1 ? boundedDimension(node->gap * static_cast<double>(visible_count - 1)) : 0.0f;
    const float available_main = boundedDimension(inner_main - total_gap);
    float remaining = available_main - used_main;
    float shrink_scale = 1.0f;
    if (!node->no_shrink && remaining < 0 && used_main > 0) {
        shrink_scale = available_main / used_main;
        remaining = 0;
    }

    float main_position = horizontal ? inner_x : inner_y;
    for (ChildMeasure& measure : measurements) {
        float main_size = measure.main_size * shrink_scale;
        if (shrink_scale == 1.0f && total_flex > 0 && measure.flex > 0 && remaining > 0) {
            main_size = addDimension(main_size, remaining * measure.flex / total_flex);
        }
        float cross_size = horizontal ? measure.node->height : measure.node->width;
        if (cross_size <= 0) cross_size = inner_cross;
        cross_size = boundedDimension(cross_size);
        nector_rect child_bounds;
        if (horizontal) {
            child_bounds = {main_position + static_cast<float>(node->content_offset_x),
                            inner_y + static_cast<float>(node->content_offset_y), main_size, cross_size};
        } else {
            child_bounds = {inner_x + static_cast<float>(node->content_offset_x),
                            main_position + static_cast<float>(node->content_offset_y), cross_size, main_size};
        }
        layoutNode(scene, measure.node, child_bounds, node->clip_bounds);
        main_position += main_size + boundedDimension(measure.node == measurements.back().node ? 0.0 : node->gap);
    }
}

} // namespace

float sceneTextWidth(nector_scene_handle scene, nector_node_handle node, const std::string& text) {
    if (text.empty()) return 0;
    if (scene->font != nullptr) {
        nector_text_metrics metrics{};
        if (nectorFontMeasureText(scene->font, static_cast<float>(node->font_size), text.c_str(), &metrics) == nector_ok &&
            nectorFinite(metrics.width)) {
            return boundedDimension(metrics.width);
        }
    }
    const double estimated = static_cast<double>(sceneUtf8Length(text)) * node->font_size * 0.6;
    return boundedDimension(estimated);
}

float sceneLineHeight(nector_scene_handle scene, nector_node_handle node) {
    if (scene->font != nullptr) {
        nector_font_metrics metrics{};
        if (nectorFontGetMetrics(scene->font, static_cast<float>(node->font_size), &metrics) == nector_ok &&
            nectorFinite(metrics.line_height) && metrics.line_height > 0) {
            return boundedDimension(metrics.line_height);
        }
    }
    return boundedDimension(node->font_size * 1.25);
}

float sceneAscent(nector_scene_handle scene, nector_node_handle node) {
    if (scene->font != nullptr) {
        nector_font_metrics metrics{};
        if (nectorFontGetMetrics(scene->font, static_cast<float>(node->font_size), &metrics) == nector_ok &&
            nectorFinite(metrics.ascent)) {
            return boundedDimension(metrics.ascent);
        }
    }
    return boundedDimension(node->font_size * 0.8);
}

nector_status sceneLayoutInternal(nector_scene_handle scene) {
    if (scene == nullptr || scene->root == nullptr) return nectorFail(nector_invalid_argument, "scene is invalid");
    const nector_rect root_bounds{0, 0, static_cast<float>(scene->width), static_cast<float>(scene->height)};
    layoutNode(scene, scene->root.get(), root_bounds, root_bounds);
    return nector_ok;
}

nector_status nectorSceneLayout(nector_scene_handle scene, int32_t width, int32_t height) {
    if (scene == nullptr || width <= 0 || height <= 0 ||
        width > static_cast<int32_t>(max_scene_dimension) || height > static_cast<int32_t>(max_scene_dimension)) {
        return nectorFail(nector_invalid_argument, "scene or layout dimensions are invalid");
    }
    SceneGuard guard(scene);
    scene->width = width;
    scene->height = height;
    nector_api_try {
        return sceneLayoutInternal(scene);
    } nector_api_catch
}
