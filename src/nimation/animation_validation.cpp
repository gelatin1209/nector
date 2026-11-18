#include "animation_validation.h"

#include <cmath>

namespace nimation_internal {

bool validAnimation(nector_animation_handle animation) {
    return animation != nullptr;
}

bool validDuration(double duration_ms) {
    return std::isfinite(duration_ms) && duration_ms > 0 && duration_ms <= max_duration_ms;
}

bool validFrameCount(int32_t frame_count) {
    return frame_count >= 1 && frame_count <= max_frame_count;
}

bool validFrameDuration(int32_t frame_count, double frame_duration_ms) {
    return std::isfinite(frame_duration_ms) && frame_duration_ms > 0 &&
           frame_duration_ms <= max_duration_ms / static_cast<double>(frame_count);
}

bool hasFrameTimeline(AnimationKind kind) {
    return kind == AnimationKind::frame || kind == AnimationKind::flipbook || kind == AnimationKind::hybrid;
}

bool hasImageClips(AnimationKind kind) {
    return kind == AnimationKind::flipbook || kind == AnimationKind::hybrid;
}

bool acceptsKeyframes(AnimationKind kind) {
    return kind == AnimationKind::keyframe || kind == AnimationKind::hybrid;
}

bool acceptsBehaviourSpans(AnimationKind kind) {
    return kind == AnimationKind::frame || kind == AnimationKind::hybrid;
}

}
