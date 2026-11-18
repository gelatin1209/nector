#ifndef nector_interface_row_layout_h
#define nector_interface_row_layout_h

#include "interface_state.h"

namespace interface_internal {

nector_status setRowLayout(nector_interface_row_handle row, int32_t layout);
nector_status refreshRow(nector_interface_row_handle row);
nector_status refreshControls(nector_interface_context_handle context);

} // namespace interface_internal

#endif
