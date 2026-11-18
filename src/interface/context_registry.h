#ifndef nector_interface_context_registry_h
#define nector_interface_context_registry_h

#include "interface_state.h"

namespace interface_internal {

nector_status createContext(nector_scene_handle scene, nector_scene_callback event_callback,
                            nector_interface_context_handle* out_context);
void destroyContext(nector_interface_context_handle context);
void destroyContextNow(nector_interface_context_handle context);

} // namespace interface_internal

#endif
