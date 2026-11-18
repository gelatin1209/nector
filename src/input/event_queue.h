#ifndef nector_input_event_queue_h
#define nector_input_event_queue_h

#include "input_state.h"

namespace input_internal {

// Caller must hold input->mutex.
void clearQueueLocked(nector_input_handle input);

}

#endif
