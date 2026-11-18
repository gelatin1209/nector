#ifndef nector_interface_row_scroll_h
#define nector_interface_row_scroll_h

#include "interface_state.h"

namespace interface_internal {

nector_status setRowScroll(nector_interface_row_handle row, double scroll);
nector_status scrollRowBy(nector_interface_row_handle row, double delta);

} // namespace interface_internal

#endif
