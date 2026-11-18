#ifndef nector_interface_operation_guard_h
#define nector_interface_operation_guard_h

#include "interface_state.h"

namespace interface_internal {

class InterfaceGuard {
public:
    explicit InterfaceGuard(nector_interface_context_handle context);
    InterfaceGuard(const InterfaceGuard&) = delete;
    InterfaceGuard& operator=(const InterfaceGuard&) = delete;
    ~InterfaceGuard();

private:
    nector_interface_context_handle context_;
};

nector_status destroyControl(nector_interface_control_handle control);

} // namespace interface_internal

#endif
