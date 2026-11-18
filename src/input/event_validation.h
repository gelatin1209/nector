#ifndef nector_input_event_validation_h
#define nector_input_event_validation_h

#include "input.h"

namespace input_internal {

bool validEventType(nector_input_event_type type);
bool validKey(int32_t key);
bool validButton(int32_t button);
nector_status validateEvent(const nector_input_event& event);

}

#endif
