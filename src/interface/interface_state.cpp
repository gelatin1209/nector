#include "interface_state.h"

namespace interface_internal {

nector_status interfaceFail(nector_status status, const char* message) {
    nectorCoreSetError(status, message);
    return status;
}

bool contextValid(nector_interface_context_handle context) {
    return context != nullptr && !context->destroy_pending;
}

bool controlValid(nector_interface_control_handle control) {
    return control != nullptr && control->alive && contextValid(control->context);
}

bool rowValid(nector_interface_row_handle row) {
    return controlValid(row) && row->kind == ControlKind::row;
}

} // namespace interface_internal
