#ifndef nector_input_pointer_state_h
#define nector_input_pointer_state_h

#include "input_state.h"

namespace input_internal {

// Caller must hold input->mutex.
void updatePointerStateLocked(nector_input_handle input, const nector_input_event& event);
// Caller must hold input->mutex.
void clearPointerStateLocked(nector_input_handle input);

}

#endif
