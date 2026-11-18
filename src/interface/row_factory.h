#ifndef nector_interface_row_factory_h
#define nector_interface_row_factory_h

#include "interface_state.h"

namespace interface_internal {

bool compositeIdValid(const char* id);
nector_status createRow(nector_interface_context_handle context,
                        nector_interface_control_handle parent,
                        const char* id, nector_interface_row_handle* out_row);

} // namespace interface_internal

#endif
