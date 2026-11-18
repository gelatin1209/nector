#ifndef nector_nimation_animation_factory_h
#define nector_nimation_animation_factory_h

#include "animation_state.h"

namespace nimation_internal {

nector_status createAnimationObject(AnimationKind kind, double duration_ms,
                                    nector_animation_handle* out_animation);
nector_status createFrameTimeline(AnimationKind kind, int32_t frame_count, double frame_duration_ms,
                                  nector_animation_handle* out_animation);
nector_status createBehaviourAnimation(nector_behaviour behaviour, double duration_ms, double amount,
                                       nector_animation_handle* out_animation);

}

#endif
