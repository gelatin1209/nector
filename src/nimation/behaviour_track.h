#ifndef nector_nimation_behaviour_track_h
#define nector_nimation_behaviour_track_h

#include "animation_state.h"

namespace nimation_internal {

bool validBehaviour(nector_behaviour behaviour);
nector_status addBehaviourSpan(nector_animation& animation, nector_behaviour behaviour,
                               int32_t start_frame, int32_t end_frame,
                               double start_value, double end_value);

}

#endif
