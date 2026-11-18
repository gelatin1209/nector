#ifndef nector_nimation_frame_position_h
#define nector_nimation_frame_position_h

#include "animation_state.h"

namespace nimation_internal {

int32_t currentFrame(const nector_animation& animation);
double nextFrameBoundary(const nector_animation& animation, int direction);
double framePosition(const nector_animation& animation, int32_t frame_index);

}

#endif
