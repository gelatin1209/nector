#include "operation_guard.h"

#include "context_registry.h"

#include <utility>

namespace interface_internal {

InterfaceGuard::InterfaceGuard(nector_interface_context_handle context) : context_(context) {
    if (context_ != nullptr) ++context_->operation_depth;
}

InterfaceGuard::~InterfaceGuard() {
    if (context_ == nullptr) return;
    --context_->operation_depth;
    if (context_->operation_depth != 0) return;
    if (context_->destroy_pending) destroyContextNow(context_);
    else context_->retired_controls.clear();
}

nector_status destroyControl(nector_interface_control_handle control) {
    nector_interface_context_handle context = control->context;
    nector_node_type type{};
    const nector_status node_status = nectorSceneGetNodeType(control->node, &type);
    if (node_status == nector_ok) {
        const nector_status remove_status = nectorSceneRemoveNode(context->scene, control->node);
        if (remove_status != nector_ok) return remove_status;
    }
    for (auto& candidate : context->controls) {
        if (!candidate->alive) continue;
        nector_interface_control_handle ancestor = candidate.get();
        while (ancestor != nullptr) {
            if (ancestor == control) {
                candidate->alive = false;
                candidate->callback = nullptr;
                candidate->user_data = nullptr;
                break;
            }
            ancestor = ancestor->parent;
        }
    }
    for (auto it = context->controls.begin(); it != context->controls.end();) {
        if (!(*it)->alive) {
            context->retired_controls.push_back(std::move(*it));
            it = context->controls.erase(it);
        } else {
            ++it;
        }
    }
    return nector_ok;
}

} // namespace interface_internal
