#include "context_registry.h"

#include <memory>
#include <unordered_set>

namespace interface_internal {
namespace {

std::unordered_set<nector_scene_handle> bound_scenes;

} // namespace

nector_status createContext(nector_scene_handle scene, nector_scene_callback event_callback,
                            nector_interface_context_handle* out_context) {
    if (bound_scenes.find(scene) != bound_scenes.end()) {
        return interfaceFail(nector_invalid_state, "scene already has an interface context");
    }
    std::unique_ptr<nector_interface_context> context(new nector_interface_context);
    context->scene = scene;
    const nector_status status = nectorSceneSetEventCallback(scene, event_callback, context.get());
    if (status != nector_ok) return status;
    try {
        bound_scenes.insert(scene);
    } catch (...) {
        (void)nectorSceneSetEventCallback(scene, nullptr, nullptr);
        throw;
    }
    *out_context = context.release();
    return nector_ok;
}

void destroyContext(nector_interface_context_handle context) {
    context->destroy_pending = true;
    context->callback = nullptr;
    context->user_data = nullptr;
    (void)nectorSceneSetEventCallback(context->scene, nullptr, nullptr);
    if (context->operation_depth == 0) destroyContextNow(context);
}

void destroyContextNow(nector_interface_context_handle context) {
    if (context == nullptr || context->operation_depth != 0) return;
    context->destroy_pending = true;
    context->callback = nullptr;
    context->user_data = nullptr;
    (void)nectorSceneSetEventCallback(context->scene, nullptr, nullptr);

    for (const auto& control : context->controls) {
        if (!control->alive || (control->parent != nullptr && control->parent->alive)) continue;
        nector_node_type type{};
        if (nectorSceneGetNodeType(control->node, &type) == nector_ok) {
            (void)nectorSceneRemoveNode(context->scene, control->node);
        }
    }
    for (auto& control : context->controls) {
        control->alive = false;
        control->callback = nullptr;
        control->user_data = nullptr;
    }
    context->controls.clear();
    context->retired_controls.clear();
    bound_scenes.erase(context->scene);
    delete context;
}

} // namespace interface_internal
