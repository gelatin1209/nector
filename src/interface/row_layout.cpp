#include "row_layout.h"

#include <algorithm>
#include <exception>
#include <new>
#include <vector>

namespace interface_internal {
namespace {

nector_status layoutScene(nector_interface_context_handle context) {
    int32_t width = 0;
    int32_t height = 0;
    nector_status status = nectorSceneGetSize(context->scene, &width, &height);
    if (status != nector_ok) return status;
    return nectorSceneLayout(context->scene, width, height);
}

nector_status nodeBounds(nector_node_handle node, nector_rect* bounds) {
    return nectorSceneGetNodeBounds(node, bounds);
}

double mainSize(nector_rect bounds, int32_t layout) {
    return layout == 0 ? bounds.width : bounds.height;
}

} // namespace

nector_status setRowLayout(nector_interface_row_handle row, int32_t layout) {
    nector_status status = nectorSceneSetNodeNumber(row->node, nector_property_layout, layout);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(row->viewport, nector_property_layout, layout);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(row->content, nector_property_layout, layout);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(row->backward, nector_property_width, layout == 0 ? 28 : 0);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(row->forward, nector_property_width, layout == 0 ? 28 : 0);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(row->backward, nector_property_height, layout == 1 ? 28 : 0);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(row->forward, nector_property_height, layout == 1 ? 28 : 0);
    if (status == nector_ok) status = nectorSceneSetNodeString(row->backward, nector_property_text, layout == 0 ? "<" : "^");
    if (status == nector_ok) status = nectorSceneSetNodeString(row->forward, nector_property_text, layout == 0 ? ">" : "v");
    if (status != nector_ok) return status;
    row->layout = layout;
    return refreshRow(row);
}

nector_status refreshRow(nector_interface_row_handle row) {
    nector_interface_context_handle context = row->context;
    nector_status status = nectorSceneSetNodeBool(row->backward, nector_property_visible, 0);
    if (status == nector_ok) status = nectorSceneSetNodeBool(row->forward, nector_property_visible, 0);
    if (status == nector_ok) status = nectorSceneSetNodeBool(row->backward, nector_property_enabled, 0);
    if (status == nector_ok) status = nectorSceneSetNodeBool(row->forward, nector_property_enabled, 0);
    if (status == nector_ok) status = layoutScene(context);
    if (status != nector_ok) return status;

    nector_rect outer_bounds{};
    nector_rect content_bounds{};
    status = nodeBounds(row->node, &outer_bounds);
    if (status == nector_ok) status = nodeBounds(row->content, &content_bounds);
    double outer_padding = 0;
    if (status == nector_ok) status = nectorSceneGetNodeNumber(row->node, nector_property_padding, &outer_padding);
    if (status != nector_ok) return status;
    const double natural_size = mainSize(content_bounds, row->layout);
    const double full_viewport = std::max(0.0, mainSize(outer_bounds, row->layout) - 2.0 * outer_padding);
    const int32_t overflow = natural_size > full_viewport + 0.001 ? 1 : 0;
    status = nectorSceneSetNodeBool(row->backward, nector_property_visible, overflow);
    if (status == nector_ok) status = nectorSceneSetNodeBool(row->forward, nector_property_visible, overflow);
    if (status == nector_ok) status = layoutScene(context);
    if (status != nector_ok) return status;

    nector_rect viewport_bounds{};
    status = nodeBounds(row->viewport, &viewport_bounds);
    if (status == nector_ok) status = nodeBounds(row->content, &content_bounds);
    double viewport_padding = 0;
    if (status == nector_ok) status = nectorSceneGetNodeNumber(row->viewport, nector_property_padding, &viewport_padding);
    if (status != nector_ok) return status;
    const double viewport_size = std::max(0.0, mainSize(viewport_bounds, row->layout) - 2.0 * viewport_padding);
    row->max_scroll = std::max(0.0, mainSize(content_bounds, row->layout) - viewport_size);
    row->scroll = std::max(0.0, std::min(row->max_scroll, row->scroll));
    status = nectorSceneSetNodeNumber(row->viewport, nector_property_content_offset_x,
                                      row->layout == 0 ? -row->scroll : 0);
    if (status == nector_ok) {
        status = nectorSceneSetNodeNumber(row->viewport, nector_property_content_offset_y,
                                          row->layout == 1 ? -row->scroll : 0);
    }
    if (status == nector_ok) status = layoutScene(context);
    if (status == nector_ok) {
        status = nectorSceneSetNodeBool(row->backward, nector_property_enabled,
                                        overflow && row->scroll > 0.001 ? 1 : 0);
    }
    if (status == nector_ok) {
        status = nectorSceneSetNodeBool(row->forward, nector_property_enabled,
                                        overflow && row->scroll + 0.001 < row->max_scroll ? 1 : 0);
    }
    return status;
}

nector_status refreshControls(nector_interface_context_handle context) {
    if (!contextValid(context)) return interfaceFail(nector_invalid_state, "interface context is being destroyed");
    if (context->refreshing) return nector_ok;
    struct ResetRefreshing {
        bool& flag;
        ~ResetRefreshing() { flag = false; }
    } reset{context->refreshing};
    context->refreshing = true;
    std::vector<nector_interface_row_handle> rows;
    nector_status status = nector_ok;
    interface_api_try {
        for (const auto& control : context->controls) {
            if (control->alive && control->kind == ControlKind::row) rows.push_back(control.get());
        }
        for (nector_interface_row_handle row : rows) {
            if (!rowValid(row)) continue;
            status = refreshRow(row);
            if (status != nector_ok) break;
        }
    } interface_api_catch
    return status;
}

} // namespace interface_internal
