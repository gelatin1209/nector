#include "internal.h"

nector_status nectorAnimationSeek(nector_animation_handle animation, double time_ms) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::seekAnimation(*animation, time_ms);
}

nector_status nectorAnimationSeekFrame(nector_animation_handle animation, int32_t frame_index) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    if (!nimation_internal::hasFrameTimeline(animation->kind)) {
        return nectorFail(nector_invalid_state, "animation has no frame timeline");
    }
    if (frame_index < 0 || frame_index >= animation->frame_count) {
        return nectorFail(nector_invalid_argument, "frame index is outside the animation");
    }
    return nimation_internal::seekAnimation(*animation,
        nimation_internal::framePosition(*animation, frame_index));
}

nector_status nectorAnimationTick(nector_animation_handle animation, double delta_ms) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::tickAnimation(*animation, delta_ms);
}

nector_status nectorAnimationGetFrameCount(nector_animation_handle animation, int32_t* out_frame_count) {
    if (!nimation_internal::validAnimation(animation) || out_frame_count == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_frame_count = 0;
    if (!nimation_internal::hasFrameTimeline(animation->kind)) {
        return nectorFail(nector_invalid_state, "animation has no frame timeline");
    }
    *out_frame_count = animation->frame_count;
    return nector_ok;
}

nector_status nectorAnimationGetFrameDuration(nector_animation_handle animation, double* out_frame_duration_ms) {
    if (!nimation_internal::validAnimation(animation) || out_frame_duration_ms == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_frame_duration_ms = 0;
    if (!nimation_internal::hasFrameTimeline(animation->kind)) {
        return nectorFail(nector_invalid_state, "animation has no frame timeline");
    }
    *out_frame_duration_ms = animation->frame_duration;
    return nector_ok;
}

nector_status nectorAnimationGetCurrentFrame(nector_animation_handle animation, int32_t* out_frame_index) {
    if (!nimation_internal::validAnimation(animation) || out_frame_index == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_frame_index = 0;
    if (!nimation_internal::hasFrameTimeline(animation->kind)) {
        return nectorFail(nector_invalid_state, "animation has no frame timeline");
    }
    *out_frame_index = nimation_internal::currentFrame(*animation);
    return nector_ok;
}
