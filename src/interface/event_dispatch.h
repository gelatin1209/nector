#ifndef nector_interface_event_dispatch_h
#define nector_interface_event_dispatch_h

#include "interface_state.h"

namespace interface_internal {

void dispatchSceneEvent(const nector_ui_event* event, void* user_data);

} // namespace interface_internal

#endif
