#ifndef nector_nimation_playback_direction_h
#define nector_nimation_playback_direction_h

#include "animation_state.h"

namespace nimation_internal {

bool validPlaybackDirection(nector_playback_direction direction);
int initialTravelDirection(const nector_animation& animation, double position);
double initialPosition(const nector_animation& animation);
bool reversing(const nector_animation& animation);
nector_status setPlaybackDirection(nector_animation& animation, nector_playback_direction direction);

}

#endif
