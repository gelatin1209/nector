#ifndef nector_input_key_state_h
#define nector_input_key_state_h

#include "input_state.h"

namespace input_internal {

// Caller must hold input->mutex.
void updateKeyStateLocked(nector_input_handle input, const nector_input_event& event,
                          int64_t timestamp_ns, bool key_repeat);
// Caller must hold input->mutex.
void clearKeyStateLocked(nector_input_handle input);

nector_status keyStateAtNs(nector_input_handle input, int32_t key, int64_t timestamp_ns, int32_t* out_state);

}

#endif
