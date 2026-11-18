#include "internal.h"

nector_status nectorCreateHybrid(int32_t frame_count, double frame_duration_ms,
                                 nector_animation_handle* out_animation) {
    return nimation_internal::createFrameTimeline(nimation_internal::AnimationKind::hybrid,
                                                  frame_count, frame_duration_ms, out_animation);
}
