#ifndef nector_nimation_animation_validation_h
#define nector_nimation_animation_validation_h

#include "animation_state.h"

namespace nimation_internal {

constexpr int32_t max_frame_count = 60;
constexpr double max_duration_ms = 1.0e12;

bool validAnimation(nector_animation_handle animation);
bool validDuration(double duration_ms);
bool validFrameCount(int32_t frame_count);
bool validFrameDuration(int32_t frame_count, double frame_duration_ms);
bool hasFrameTimeline(AnimationKind kind);
bool hasImageClips(AnimationKind kind);
bool acceptsKeyframes(AnimationKind kind);
bool acceptsBehaviourSpans(AnimationKind kind);

}

#endif
