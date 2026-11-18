#ifndef nector_nimation_internal_h
#define nector_nimation_internal_h

#include "animation_state.h"
#include "animation_factory.h"
#include "animation_validation.h"
#include "playback_control.h"
#include "playback_direction.h"
#include "loop_counter.h"
#include "frame_position.h"
#include "tick_engine.h"
#include "animation_event.h"
#include "keyframe_track.h"
#include "easing_curve.h"
#include "cubic_bezier.h"
#include "behaviour_track.h"
#include "behaviour_sampler.h"

namespace nimation_internal {

nector_status storeImageClip(nector_animation& animation, int32_t frame_index, nector_image_handle image);

}

#endif
