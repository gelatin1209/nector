#include "internal.h"

nector_status nectorCreateAnimation(double duration_ms, nector_animation_handle* out_animation) {
    return nimation_internal::createAnimationObject(nimation_internal::AnimationKind::keyframe,
                                                    duration_ms, out_animation);
}

void nectorDestroyAnimation(nector_animation_handle animation) {
    delete animation;
}

nector_status nectorAnimationSetLoop(nector_animation_handle animation, int32_t loop_count) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::setLoopCount(*animation, loop_count);
}

nector_status nectorAnimationSetDirection(nector_animation_handle animation, nector_playback_direction direction) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::setPlaybackDirection(*animation, direction);
}

nector_status nectorAnimationGetDirection(nector_animation_handle animation, nector_playback_direction* out_direction) {
    if (!nimation_internal::validAnimation(animation) || out_direction == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_direction = animation->direction;
    return nector_ok;
}

nector_status nectorAnimationSetRate(nector_animation_handle animation, double rate) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::setPlaybackRate(*animation, rate);
}

nector_status nectorAnimationGetRate(nector_animation_handle animation, double* out_rate) {
    if (!nimation_internal::validAnimation(animation) || out_rate == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_rate = animation->playback_rate;
    return nector_ok;
}

nector_status nectorAnimationSetEventCallback(nector_animation_handle animation,
                                               nector_animation_event_callback callback, void* user_data) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    nimation_internal::setEventCallback(*animation, callback, user_data);
    return nector_ok;
}

nector_status nectorAnimationGetCompletedLoops(nector_animation_handle animation, uint64_t* out_completed_loops) {
    if (!nimation_internal::validAnimation(animation) || out_completed_loops == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_completed_loops = animation->completed_loops;
    return nector_ok;
}

nector_status nectorAnimationStart(nector_animation_handle animation) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::startAnimation(*animation);
}

nector_status nectorAnimationPause(nector_animation_handle animation) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    nimation_internal::pauseAnimation(*animation);
    return nector_ok;
}

nector_status nectorAnimationStop(nector_animation_handle animation) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    nimation_internal::stopAnimation(*animation);
    return nector_ok;
}

nector_status nectorAnimationGetPosition(nector_animation_handle animation, double* out_time_ms) {
    if (!nimation_internal::validAnimation(animation) || out_time_ms == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_time_ms = animation->position;
    return nector_ok;
}

nector_status nectorAnimationIsPlaying(nector_animation_handle animation, int32_t* out_playing) {
    if (!nimation_internal::validAnimation(animation) || out_playing == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_playing = animation->playing ? 1 : 0;
    return nector_ok;
}

nector_status nectorAnimationIsFinished(nector_animation_handle animation, int32_t* out_finished) {
    if (!nimation_internal::validAnimation(animation) || out_finished == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_finished = animation->finished ? 1 : 0;
    return nector_ok;
}
