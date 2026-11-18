#include "control_factory.h"

#include "row_layout.h"

#include <memory>
#include <utility>

namespace interface_internal {

SceneNodeCleanup::SceneNodeCleanup(nector_interface_context_handle context, nector_node_handle node)
    : context_(context), node_(node) {}

SceneNodeCleanup::~SceneNodeCleanup() {
    if (node_ != nullptr) (void)nectorSceneRemoveNode(context_->scene, node_);
}

void SceneNodeCleanup::release() {
    node_ = nullptr;
}

nector_status parentNode(nector_interface_context_handle context,
                         nector_interface_control_handle parent,
                         nector_node_handle* out_parent) {
    if (parent == nullptr) return nectorSceneGetRoot(context->scene, out_parent);
    if (!controlValid(parent)) return interfaceFail(nector_invalid_argument, "parent control is invalid");
    *out_parent = parent->kind == ControlKind::row ? parent->content : parent->node;
    return nector_ok;
}

nector_status attachControl(nector_interface_context_handle context,
                            nector_interface_control_handle parent,
                            nector_node_handle node, ControlKind kind,
                            nector_interface_control_handle* out_control) {
    std::unique_ptr<nector_interface_control> control(new nector_interface_control);
    control->context = context;
    control->parent = parent;
    control->kind = kind;
    control->node = node;
    nector_interface_control_handle raw = control.get();
    context->controls.push_back(std::move(control));
    *out_control = raw;
    return nector_ok;
}

nector_status createControl(nector_interface_context_handle context,
                            nector_interface_control_handle parent,
                            nector_node_type type, const char* id,
                            nector_interface_control_handle* out_control) {
    nector_node_handle scene_parent = nullptr;
    nector_status status = parentNode(context, parent, &scene_parent);
    if (status != nector_ok) return status;
    nector_node_handle node = nullptr;
    status = nectorSceneAddNode(context->scene, scene_parent, type, id, &node);
    if (status != nector_ok) return status;
    SceneNodeCleanup cleanup(context, node);
    const ControlKind kind = type == nector_node_button ? ControlKind::button : ControlKind::generic;
    status = attachControl(context, parent, node, kind, out_control);
    if (status != nector_ok) return status;
    cleanup.release();
    return refreshControls(context);
}

nector_status createButton(nector_interface_context_handle context,
                           nector_interface_control_handle parent,
                           const char* id, const char* text,
                           nector_interface_button_handle* out_button) {
    nector_node_handle scene_parent = nullptr;
    nector_status status = parentNode(context, parent, &scene_parent);
    if (status != nector_ok) return status;
    nector_node_handle node = nullptr;
    status = nectorSceneAddNode(context->scene, scene_parent, nector_node_button, id, &node);
    if (status != nector_ok) return status;
    SceneNodeCleanup cleanup(context, node);
    status = nectorSceneSetNodeString(node, nector_property_text, text);
    if (status == nector_ok) status = nectorSceneSetNodeBool(node, nector_property_glass, 1);
    if (status == nector_ok) {
        status = nectorSceneSetNodeColor(node, nector_property_background, {0.64f, 0.78f, 0.96f, 0.52f});
    }
    if (status == nector_ok) {
        status = nectorSceneSetNodeColor(node, nector_property_border, {1, 1, 1, 0.68f});
    }
    if (status != nector_ok) return status;
    status = attachControl(context, parent, node, ControlKind::button, out_button);
    if (status != nector_ok) return status;
    cleanup.release();
    return refreshControls(context);
}

} // namespace interface_internal
