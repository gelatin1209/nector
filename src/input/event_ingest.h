#ifndef nector_input_event_ingest_h
#define nector_input_event_ingest_h

#include "input_state.h"

namespace input_internal {

nector_status pushAtNs(nector_input_handle input, const nector_input_event* event,
                       int64_t timestamp_ns, bool key_repeat);

}

#endif
