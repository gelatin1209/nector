#include "row_factory.h"

#include "control_factory.h"
#include "row_layout.h"

#include <cstddef>
#include <string>

namespace interface_internal {

bool compositeIdValid(const char* id) {
    if (id == nullptr) return false;
    size_t length = 0;
    while (length <= 200 && id[length] != '\0') ++length;
    return length > 0 && length <= 200;
}

nector_status createRow(nector_interface_context_handle context,
                        nector_interface_control_handle parent,
                        const char* id, nector_interface_row_handle* out_row) {
    nector_node_handle scene_parent = nullptr;
    nector_status status = parentNode(context, parent, &scene_parent);
    if (status != nector_ok) return status;

    const std::string base(id);
    nector_node_handle outer = nullptr;
    nector_node_handle viewport = nullptr;
    nector_node_handle content = nullptr;
    nector_node_handle backward = nullptr;
    nector_node_handle forward = nullptr;
    status = nectorSceneAddNode(context->scene, scene_parent, nector_node_row, id, &outer);
    if (status != nector_ok) return status;
    SceneNodeCleanup cleanup(context, outer);
    status = nectorSceneAddNode(context->scene, outer, nector_node_button,
                                (base + ".backward").c_str(), &backward);
    if (status == nector_ok) {
        status = nectorSceneAddNode(context->scene, outer, nector_node_row,
                                    (base + ".viewport").c_str(), &viewport);
    }
    if (status == nector_ok) {
        status = nectorSceneAddNode(context->scene, viewport, nector_node_row,
                                    (base + ".content").c_str(), &content);
    }
    if (status == nector_ok) {
        status = nectorSceneAddNode(context->scene, outer, nector_node_button,
                                    (base + ".forward").c_str(), &forward);
    }
    if (status == nector_ok) status = nectorSceneSetNodeString(backward, nector_property_text, "<");
    if (status == nector_ok) status = nectorSceneSetNodeString(forward, nector_property_text, ">");
    if (status == nector_ok) status = nectorSceneSetNodeNumber(backward, nector_property_width, 28);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(forward, nector_property_width, 28);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(viewport, nector_property_flex, 1);
    if (status == nector_ok) status = nectorSceneSetNodeBool(viewport, nector_property_no_shrink, 1);
    if (status == nector_ok) status = nectorSceneSetNodeBool(content, nector_property_no_shrink, 1);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(viewport, nector_property_scroll_step, 48);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(outer, nector_property_layout, 0);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(viewport, nector_property_layout, 0);
    if (status == nector_ok) status = nectorSceneSetNodeNumber(content, nector_property_layout, 0);
    if (status != nector_ok) return status;
    status = attachControl(context, parent, outer, ControlKind::row, out_row);
    if (status != nector_ok) return status;
    cleanup.release();
    nector_interface_row_handle row = *out_row;
    row->viewport = viewport;
    row->content = content;
    row->backward = backward;
    row->forward = forward;
    status = nectorSceneSetNodeBool(backward, nector_property_visible, 0);
    if (status == nector_ok) status = nectorSceneSetNodeBool(forward, nector_property_visible, 0);
    if (status == nector_ok) status = nectorSceneSetNodeBool(backward, nector_property_enabled, 0);
    if (status == nector_ok) status = nectorSceneSetNodeBool(forward, nector_property_enabled, 0);
    if (status != nector_ok) return status;
    return refreshControls(context);
}

} // namespace interface_internal
