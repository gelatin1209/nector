#ifndef nector_nimation_animation_event_h
#define nector_nimation_animation_event_h

#include "animation_state.h"

namespace nimation_internal {

void setEventCallback(nector_animation& animation, nector_animation_event_callback callback, void* user_data);
nector_status emitEvent(nector_animation& animation, nector_animation_event_type type);
nector_status finishAnimation(nector_animation& animation);

}

#endif
