#include "internal.h"

namespace {

class PainterState {
public:
    explicit PainterState(nector_painter_handle painter) : painter_(painter) {
        status_ = nectorPainterSave(painter_);
    }

    ~PainterState() {
        if (status_ == nector_ok) nectorPainterRestore(painter_);
    }

    nector_status status() const { return status_; }

private:
    nector_painter_handle painter_;
    nector_status status_;
};

using PathPointer = std::unique_ptr<nector_path, decltype(&nectorDestroyPath)>;

nector_color effectiveColor(nector_node_handle node, nector_color color) {
    if (!sceneNodeEnabled(node)) color.a *= 0.45f;
    return color;
}

nector_status appendRoundedRectangle(nector_path_handle path, nector_rect rect, double radius) {
    const double maximum_radius = std::min(rect.width, rect.height) * 0.5;
    const float r = sceneClampFloat(radius, 0, maximum_radius);
    if (r <= 0) return nectorPathAddRectangle(path, rect);
    const float left = rect.x;
    const float top = rect.y;
    const float right = rect.x + rect.width;
    const float bottom = rect.y + rect.height;
    nector_status status = nectorPathMoveTo(path, left + r, top);
    if (status == nector_ok) status = nectorPathLineTo(path, right - r, top);
    if (status == nector_ok) status = nectorPathQuadraticTo(path, right, top, right, top + r);
    if (status == nector_ok) status = nectorPathLineTo(path, right, bottom - r);
    if (status == nector_ok) status = nectorPathQuadraticTo(path, right, bottom, right - r, bottom);
    if (status == nector_ok) status = nectorPathLineTo(path, left + r, bottom);
    if (status == nector_ok) status = nectorPathQuadraticTo(path, left, bottom, left, bottom - r);
    if (status == nector_ok) status = nectorPathLineTo(path, left, top + r);
    if (status == nector_ok) status = nectorPathQuadraticTo(path, left, top, left + r, top);
    if (status == nector_ok) status = nectorPathClose(path);
    return status;
}

nector_status roundedRectangle(nector_painter_handle painter, nector_rect rect, double radius,
                               nector_color fill, nector_color border, float border_width) {
    if (rect.width <= 0 || rect.height <= 0) return nector_ok;
    nector_path_handle raw_path = nullptr;
    nector_status status = nectorCreatePath(&raw_path);
    if (status != nector_ok) return status;
    PathPointer path(raw_path, &nectorDestroyPath);
    status = appendRoundedRectangle(path.get(), rect, radius);
    if (status == nector_ok && fill.a > 0) {
        status = nectorPainterSetFillColor(painter, fill);
        if (status == nector_ok) status = nectorPainterFillPath(painter, path.get(), nector_fill_nonzero);
    }
    if (status == nector_ok && border.a > 0 && border_width > 0) {
        status = nectorPainterSetFillColor(painter, border);
        if (status == nector_ok) {
            status = nectorPainterStrokePath(painter, path.get(), border_width, nector_cap_round, nector_join_round);
        }
    }
    return status;
}

nector_status glassRectangle(nector_painter_handle painter, nector_node_handle node, bool pressed,
                             nector_color border, float border_width) {
    const nector_rect rect = node->bounds;
    if (rect.width <= 0 || rect.height <= 0) return nector_ok;
    nector_path_handle raw_path = nullptr;
    nector_status status = nectorCreatePath(&raw_path);
    if (status != nector_ok) return status;
    PathPointer path(raw_path, &nectorDestroyPath);
    status = appendRoundedRectangle(path.get(), rect, node->corner_radius);
    if (status != nector_ok) return status;

    const float pressed_scale = pressed ? 0.80f : 1.0f;
    nector_color top = node->background;
    nector_color bottom = node->background;
    top.r = std::min(1.0f, top.r * 0.78f + 0.22f) * pressed_scale;
    top.g = std::min(1.0f, top.g * 0.78f + 0.22f) * pressed_scale;
    top.b = std::min(1.0f, top.b * 0.78f + 0.22f) * pressed_scale;
    top.a = std::min(0.72f, top.a);
    bottom.r *= 0.68f * pressed_scale;
    bottom.g *= 0.68f * pressed_scale;
    bottom.b *= 0.68f * pressed_scale;
    bottom.a = std::min(0.62f, bottom.a * 0.84f);
    top = effectiveColor(node, top);
    bottom = effectiveColor(node, bottom);
    status = nectorPainterSetLinearGradient(painter, {rect.x, rect.y}, {rect.x, rect.y + rect.height}, top, bottom);
    if (status == nector_ok) status = nectorPainterFillPath(painter, path.get(), nector_fill_nonzero);
    if (status != nector_ok) return status;

    raw_path = nullptr;
    status = nectorCreatePath(&raw_path);
    if (status != nector_ok) return status;
    PathPointer highlight(raw_path, &nectorDestroyPath);
    const nector_rect highlight_rect{rect.x + 1, rect.y + 1, std::max(0.0f, rect.width - 2),
                                     std::max(0.0f, rect.height * 0.48f - 1)};
    if (highlight_rect.width > 0 && highlight_rect.height > 0) {
        status = appendRoundedRectangle(highlight.get(), highlight_rect,
                                        std::max(0.0, node->corner_radius - 1));
        nector_color highlight_top = effectiveColor(node, {1, 1, 1, pressed ? 0.18f : 0.34f});
        nector_color highlight_bottom = effectiveColor(node, {1, 1, 1, 0.01f});
        if (status == nector_ok) {
            status = nectorPainterSetLinearGradient(painter,
                {highlight_rect.x, highlight_rect.y},
                {highlight_rect.x, highlight_rect.y + highlight_rect.height},
                highlight_top, highlight_bottom);
        }
        if (status == nector_ok) status = nectorPainterFillPath(painter, highlight.get(), nector_fill_nonzero);
        if (status != nector_ok) return status;
    }
    if (border.a > 0 && border_width > 0) {
        status = nectorPainterSetFillColor(painter, border);
        if (status == nector_ok) {
            status = nectorPainterStrokePath(painter, path.get(), border_width, nector_cap_round, nector_join_round);
        }
    }
    return status;
}

std::vector<std::string> wrappedLines(nector_scene_handle scene, nector_node_handle node,
                                      const std::string& text, float maximum_width) {
    std::vector<std::string> output;
    if (!node->word_wrap || maximum_width <= 0) {
        size_t start = 0;
        while (true) {
            const size_t newline = text.find('\n', start);
            output.push_back(text.substr(start, newline == std::string::npos ? newline : newline - start));
            if (newline == std::string::npos) break;
            start = newline + 1;
        }
        return output;
    }

    size_t paragraph_start = 0;
    while (true) {
        const size_t newline = text.find('\n', paragraph_start);
        const size_t paragraph_end = newline == std::string::npos ? text.size() : newline;
        const std::string paragraph = text.substr(paragraph_start, paragraph_end - paragraph_start);
        const std::vector<size_t> offsets = sceneUtf8Offsets(paragraph);
        std::string line;
        for (size_t index = 0; index + 1 < offsets.size(); ++index) {
            const std::string character = paragraph.substr(offsets[index], offsets[index + 1] - offsets[index]);
            const std::string candidate = line + character;
            if (!line.empty() && sceneTextWidth(scene, node, candidate) > maximum_width) {
                const size_t space = line.find_last_of(' ');
                if (space != std::string::npos && space > 0) {
                    output.push_back(line.substr(0, space));
                    line = line.substr(space + 1);
                } else {
                    output.push_back(line);
                    line.clear();
                }
            }
            line += character;
            while (line.size() > 1 && line.front() == ' ') line.erase(line.begin());
        }
        output.push_back(line);
        if (newline == std::string::npos) break;
        paragraph_start = newline + 1;
    }
    return output;
}

nector_status drawTextWithPainter(nector_painter_handle painter, nector_scene_handle scene,
                                  nector_node_handle node, const std::string& text, float x,
                                  float baseline, nector_color color) {
    if (scene->font == nullptr || text.empty() || color.a <= 0) return nector_ok;
    return nectorFontDrawText(painter, scene->font, static_cast<float>(node->font_size), x, baseline,
                              text.c_str(), color);
}

nector_status drawLabel(nector_painter_handle painter, nector_scene_handle scene, nector_node_handle node) {
    const float padding = static_cast<float>(node->padding);
    const float line_height = sceneLineHeight(scene, node);
    const float ascent = sceneAscent(scene, node);
    const float maximum_width = std::max(0.0f, node->bounds.width - 2.0f * padding);
    const std::vector<std::string> lines = wrappedLines(scene, node, node->text, maximum_width);
    float baseline = node->bounds.y + padding + ascent;
    for (const std::string& line : lines) {
        const nector_status status = drawTextWithPainter(painter, scene, node, line, node->bounds.x + padding,
                                                         baseline, effectiveColor(node, node->foreground));
        if (status != nector_ok) return status;
        baseline += line_height;
        if (baseline - ascent >= node->bounds.y + node->bounds.height) break;
    }
    return nector_ok;
}

nector_status drawCenteredText(nector_painter_handle painter, nector_scene_handle scene,
                               nector_node_handle node, const std::string& text, float left,
                               float width, nector_color color) {
    const float line_height = sceneLineHeight(scene, node);
    const float baseline = node->bounds.y + (node->bounds.height - line_height) * 0.5f + sceneAscent(scene, node);
    const float text_width = sceneTextWidth(scene, node, text);
    const float x = left + std::max(0.0f, (width - text_width) * 0.5f);
    return drawTextWithPainter(painter, scene, node, text, x, baseline, color);
}

nector_status drawLineEdit(nector_painter_handle painter, nector_scene_handle scene, nector_node_handle node) {
    const float padding = static_cast<float>(node->padding);
    const float left = node->bounds.x + padding;
    const float available = std::max(0.0f, node->bounds.width - 2.0f * padding);
    const bool placeholder = node->text.empty() && !node->placeholder.empty();
    const std::string& text = placeholder ? node->placeholder : node->text;
    nector_color color = effectiveColor(node, node->foreground);
    if (placeholder) color.a *= 0.55f;
    float text_x = left;
    float caret_width = 0;
    if (!placeholder) {
        const size_t caret_offset = sceneByteOffsetForCaret(node->text, node->caret);
        caret_width = sceneTextWidth(scene, node, node->text.substr(0, caret_offset));
        if (scene->focused_node == node && caret_width > available) text_x -= caret_width - available;
    }
    const float baseline = node->bounds.y + (node->bounds.height - sceneLineHeight(scene, node)) * 0.5f +
                           sceneAscent(scene, node);
    nector_status status = drawTextWithPainter(painter, scene, node, text, text_x, baseline, color);
    if (status != nector_ok) return status;
    if (scene->focused_node == node && sceneNodeEnabled(node)) {
        const float caret_x = text_x + caret_width;
        const float inset = std::min(5.0f, node->bounds.height * 0.25f);
        status = nectorPainterSetFillColor(painter, node->accent);
        if (status != nector_ok) return status;
        status = nectorPainterDrawLine(painter, {caret_x, node->bounds.y + inset},
                                       {caret_x, node->bounds.y + node->bounds.height - inset}, 1.0f);
    }
    return status;
}

nector_status drawCheckBox(nector_painter_handle painter, nector_scene_handle scene, nector_node_handle node) {
    const float box_size = std::min(18.0f, std::max(0.0f, node->bounds.height - 4.0f));
    const nector_rect box{node->bounds.x + 2, node->bounds.y + (node->bounds.height - box_size) * 0.5f,
                          box_size, box_size};
    nector_color fill = node->checked ? node->accent : nector_color{1, 1, 1, 1};
    nector_status status = roundedRectangle(painter, box, 3, effectiveColor(node, fill),
                                            effectiveColor(node, node->border), 1.0f);
    if (status != nector_ok) return status;
    if (node->checked && box_size >= 8) {
        status = nectorPainterSetFillColor(painter, effectiveColor(node, {1, 1, 1, 1}));
        if (status == nector_ok) {
            status = nectorPainterDrawLine(painter, {box.x + box.width * 0.22f, box.y + box.height * 0.52f},
                                           {box.x + box.width * 0.43f, box.y + box.height * 0.74f}, 2.0f);
        }
        if (status == nector_ok) {
            status = nectorPainterDrawLine(painter, {box.x + box.width * 0.43f, box.y + box.height * 0.74f},
                                           {box.x + box.width * 0.80f, box.y + box.height * 0.27f}, 2.0f);
        }
        if (status != nector_ok) return status;
    }
    const float text_left = box.x + box.width + 7;
    return drawCenteredText(painter, scene, node, node->text, text_left,
                            std::max(0.0f, node->bounds.x + node->bounds.width - text_left),
                            effectiveColor(node, node->foreground));
}

nector_status drawSlider(nector_painter_handle painter, nector_node_handle node) {
    const float center_y = node->bounds.y + node->bounds.height * 0.5f;
    const float left = node->bounds.x + 8;
    const float width = std::max(0.0f, node->bounds.width - 16);
    const double range = node->maximum - node->minimum;
    const float fraction = range > 0 ? sceneClampFloat((node->value - node->minimum) / range, 0, 1) : 0;
    const nector_rect track{left, center_y - 2, width, 4};
    nector_status status = roundedRectangle(painter, track, 2, effectiveColor(node, node->border), {0, 0, 0, 0}, 0);
    if (status != nector_ok) return status;
    status = roundedRectangle(painter, {left, center_y - 2, width * fraction, 4}, 2,
                              effectiveColor(node, node->accent), {0, 0, 0, 0}, 0);
    if (status != nector_ok) return status;
    status = nectorPainterSetFillColor(painter, effectiveColor(node, node->accent));
    if (status == nector_ok) {
        status = nectorPainterFillEllipse(painter, left + width * fraction, center_y, 7, 7);
    }
    return status;
}

nector_status drawProgress(nector_painter_handle painter, nector_node_handle node) {
    const double range = node->maximum - node->minimum;
    const float fraction = range > 0 ? sceneClampFloat((node->value - node->minimum) / range, 0, 1) : 0;
    const nector_rect fill{node->bounds.x, node->bounds.y, node->bounds.width * fraction, node->bounds.height};
    return roundedRectangle(painter, fill, node->corner_radius, effectiveColor(node, node->accent), {0, 0, 0, 0}, 0);
}

nector_status drawNode(nector_painter_handle painter, nector_scene_handle scene, nector_node_handle node) {
    if (!sceneNodeValid(node) || !node->visible || node->clip_bounds.width <= 0 || node->clip_bounds.height <= 0) {
        return nector_ok;
    }
    PainterState painter_state(painter);
    nector_status status = painter_state.status();
    if (status != nector_ok) return status;
    status = nectorPainterSetClip(painter, node->clip_bounds);
    if (status != nector_ok) return status;

    nector_color background = effectiveColor(node, node->background);
    if (node->type == nector_node_button && scene->pressed_node == node) {
        background.r *= 0.88f;
        background.g *= 0.88f;
        background.b *= 0.88f;
    }
    nector_color border = effectiveColor(node, node->border);
    if (scene->focused_node == node) border = effectiveColor(node, node->accent);
    const bool framed = node->background.a > 0 || node->type == nector_node_button ||
                        node->type == nector_node_line_edit || node->type == nector_node_canvas;
    if (framed) {
        const float border_width = scene->focused_node == node ? 2.0f : 1.0f;
        if (node->type == nector_node_button && node->glass) {
            status = glassRectangle(painter, node, scene->pressed_node == node, border, border_width);
        } else {
            status = roundedRectangle(painter, node->bounds, node->corner_radius, background, border, border_width);
        }
        if (status != nector_ok) return status;
    }

    switch (node->type) {
        case nector_node_label:
            status = drawLabel(painter, scene, node);
            break;
        case nector_node_button:
            status = drawCenteredText(painter, scene, node, node->text, node->bounds.x + node->padding,
                                      std::max(0.0, node->bounds.width - 2.0 * node->padding),
                                      effectiveColor(node, node->foreground));
            break;
        case nector_node_line_edit:
            status = drawLineEdit(painter, scene, node);
            break;
        case nector_node_check_box:
            status = drawCheckBox(painter, scene, node);
            break;
        case nector_node_slider:
            status = drawSlider(painter, node);
            break;
        case nector_node_progress_bar:
            status = drawProgress(painter, node);
            break;
        case nector_node_canvas:
            status = sceneEmitNodeEvent(scene, nector_ui_paint, node, nullptr, nullptr, 0, painter);
            break;
        case nector_node_image:
            if (node->image != nullptr && node->bounds.width > 0 && node->bounds.height > 0) {
                int32_t image_width = 0;
                int32_t image_height = 0;
                status = nectorImageGetSize(node->image, &image_width, &image_height);
                if (status == nector_ok && image_width > 0 && image_height > 0) {
                    status = nectorImageDraw(painter, node->image, node->bounds,
                                             {0, 0, static_cast<float>(image_width), static_cast<float>(image_height)}, 1.0f);
                }
            }
            break;
        default:
            break;
    }
    if (status != nector_ok || !sceneNodeValid(node)) return status;
    if (scene->focused_node == node && (node->type == nector_node_slider || node->type == nector_node_check_box)) {
        status = roundedRectangle(painter, node->bounds, node->corner_radius, {0, 0, 0, 0},
                                  effectiveColor(node, node->accent), 1.0f);
        if (status != nector_ok) return status;
    }

    std::vector<nector_node_handle> children;
    children.reserve(node->children.size());
    for (const auto& child : node->children) children.push_back(child.get());
    for (nector_node_handle child : children) {
        if (!sceneNodeValid(child)) continue;
        status = drawNode(painter, scene, child);
        if (status != nector_ok) return status;
    }
    return nector_ok;
}

} // namespace

nector_status sceneDrawInternal(nector_scene_handle scene, nector_painter_handle painter) {
    if (scene == nullptr || painter == nullptr) return nectorFail(nector_invalid_argument, "scene or painter is null");
    return drawNode(painter, scene, scene->root.get());
}

nector_status nectorSceneDraw(nector_scene_handle scene, nector_painter_handle painter) {
    if (scene == nullptr || painter == nullptr) return nectorFail(nector_invalid_argument, "scene or painter is null");
    SceneGuard guard(scene);
    nector_api_try {
        return sceneDrawInternal(scene, painter);
    } nector_api_catch
}
