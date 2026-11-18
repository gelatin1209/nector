#include "playback_control.h"

#include "playback_direction.h"

#include <cmath>

namespace nimation_internal {

nector_status startAnimation(nector_animation& animation) {
    if (animation.kind == AnimationKind::keyframe && animation.channels.empty()) {
        return nectorFail(nector_invalid_state, "animation has no keyframes");
    }
    const bool at_reverse_start = animation.direction == nector_playback_reverse && animation.position <= 0.0;
    const bool at_normal_end = animation.direction == nector_playback_normal && animation.position >= animation.duration;
    if (animation.finished || at_reverse_start || at_normal_end) {
        animation.position = initialPosition(animation);
        animation.completed_loops = 0;
        animation.travel_direction = initialTravelDirection(animation, animation.position);
    }
    animation.playing = true;
    animation.finished = false;
    return nector_ok;
}

void pauseAnimation(nector_animation& animation) {
    animation.playing = false;
}

void stopAnimation(nector_animation& animation) {
    animation.playing = false;
    animation.finished = false;
    animation.position = initialPosition(animation);
    animation.completed_loops = 0;
    animation.travel_direction = initialTravelDirection(animation, animation.position);
}

nector_status seekAnimation(nector_animation& animation, double time_ms) {
    if (!std::isfinite(time_ms) || time_ms < 0 || time_ms > animation.duration) {
        return nectorFail(nector_invalid_argument, "seek position is outside the animation");
    }
    animation.position = time_ms;
    animation.completed_loops = 0;
    animation.travel_direction = initialTravelDirection(animation, time_ms);
    const bool at_terminal = animation.direction == nector_playback_normal
        ? time_ms == animation.duration
        : animation.direction == nector_playback_reverse && time_ms == 0.0;
    animation.finished = at_terminal && animation.loop_count == 0;
    if (animation.finished) animation.playing = false;
    return nector_ok;
}

nector_status setPlaybackRate(nector_animation& animation, double rate) {
    if (!std::isfinite(rate) || rate <= 0.0 || rate > 1000000.0) {
        return nectorFail(nector_invalid_argument, "playback rate is invalid");
    }
    animation.playback_rate = rate;
    return nector_ok;
}

}
