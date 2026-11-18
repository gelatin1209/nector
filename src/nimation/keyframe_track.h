#ifndef nector_nimation_keyframe_track_h
#define nector_nimation_keyframe_track_h

#include "animation_state.h"

namespace nimation_internal {

nector_status addKey(nector_animation& animation, int32_t channel, double time_ms, double value,
                     nector_easing easing, const nector_easing_parameters* parameters);
nector_status sampleChannel(const nector_animation& animation, int32_t channel, double* out_value);

}

#endif
