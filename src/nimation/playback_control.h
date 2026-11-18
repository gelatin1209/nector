#ifndef nector_nimation_playback_control_h
#define nector_nimation_playback_control_h

#include "animation_state.h"

namespace nimation_internal {

nector_status startAnimation(nector_animation& animation);
void pauseAnimation(nector_animation& animation);
void stopAnimation(nector_animation& animation);
nector_status seekAnimation(nector_animation& animation, double time_ms);
nector_status setPlaybackRate(nector_animation& animation, double rate);

}

#endif
