#include "event_dispatch.h"

#include "operation_guard.h"
#include "row_layout.h"
#include "row_scroll.h"

#include <cmath>

namespace interface_internal {
namespace {

nector_interface_control_handle findControlByNode(nector_interface_context_handle context,
                                                   nector_node_handle node) {
    if (node == nullptr) return nullptr;
    for (const auto& control : context->controls) {
        if (!control->alive) continue;
        if (control->node == node) return control.get();
        if (control->kind == ControlKind::row &&
            (control->viewport == node || control->content == node ||
             control->backward == node || control->forward == node)) {
            return control.get();
        }
    }
    return nullptr;
}

nector_interface_row_handle findRowByPart(nector_interface_context_handle context,
                                           nector_node_handle node) {
    nector_interface_control_handle control = findControlByNode(context, node);
    return control != nullptr && control->kind == ControlKind::row ? control : nullptr;
}

} // namespace

void dispatchSceneEvent(const nector_ui_event* event, void* user_data) {
    nector_interface_context_handle context = static_cast<nector_interface_context_handle>(user_data);
    if (!contextValid(context) || event == nullptr) return;
    InterfaceGuard guard(context);
    if (event->type == nector_ui_click && event->node != nullptr) {
        nector_interface_row_handle row = findRowByPart(context, event->node);
        if (row != nullptr && (row->backward == event->node || row->forward == event->node)) {
            double step = 0;
            if (nectorSceneGetNodeNumber(row->viewport, nector_property_scroll_step, &step) == nector_ok) {
                const double delta = row->backward == event->node ? -step : step;
                const double target = delta < 0 && -delta > row->scroll ? 0 : row->scroll + delta;
                (void)setRowScroll(row, target);
            }
        }
    } else if (event->type == nector_ui_scroll && event->node != nullptr) {
        nector_interface_row_handle row = findRowByPart(context, event->node);
        if (row != nullptr && row->viewport == event->node && std::isfinite(event->value)) {
            row->scroll = event->value;
            (void)refreshControls(context);
        }
    }
    if (!contextValid(context)) return;

    nector_interface_control_handle control = findControlByNode(context, event->node);
    if (control != nullptr && control->callback != nullptr) {
        const nector_interface_event_callback callback = control->callback;
        void* const callback_data = control->user_data;
        callback(event, callback_data);
    }
    if (!contextValid(context)) return;
    if (context->callback != nullptr) {
        const nector_interface_event_callback callback = context->callback;
        void* const callback_data = context->user_data;
        callback(event, callback_data);
    }
}

} // namespace interface_internal
