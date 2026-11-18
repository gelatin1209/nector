#include "tick_engine.h"

#include "animation_event.h"
#include "animation_validation.h"
#include "frame_position.h"
#include "loop_counter.h"

#include <cmath>

namespace {

nector_status advanceWithoutEvents(nector_animation& animation, long double distance) {
    const long double duration = static_cast<long double>(animation.duration);
    if (animation.direction == nector_playback_ping_pong) {
        const long double cycle = duration * 2.0L;
        const long double phase = animation.travel_direction > 0
            ? static_cast<long double>(animation.position)
            : cycle - static_cast<long double>(animation.position);
        const long double target = phase + distance;
        const long double cycles = std::floor(target / cycle);
        if (nimation_internal::crossingsWithinLimit(animation, cycles)) {
            const nector_status status = nimation_internal::recordFastPathLoops(animation, cycles);
            if (status != nector_ok) return status;
            long double wrapped = std::fmod(target, cycle);
            if (wrapped < 0.0L) wrapped += cycle;
            animation.travel_direction = wrapped < duration ? 1 : -1;
            animation.position = static_cast<double>(wrapped <= duration ? wrapped : cycle - wrapped);
            return nector_ok;
        }
        nimation_internal::finishLoopCount(animation);
        animation.position = 0.0;
        animation.travel_direction = 1;
        animation.playing = false;
        animation.finished = true;
        return nector_ok;
    }

    const int direction = animation.direction == nector_playback_reverse ? -1 : 1;
    const long double offset = direction > 0 ? static_cast<long double>(animation.position)
        : duration - static_cast<long double>(animation.position);
    const long double target = offset + distance;
    if (target < duration) {
        const long double position = direction > 0 ? target : duration - target;
        animation.position = static_cast<double>(position);
        return nector_ok;
    }
    const long double crossings = std::floor(target / duration);
    if (nimation_internal::crossingsWithinLimit(animation, crossings)) {
        const nector_status status = nimation_internal::recordFastPathLoops(animation, crossings);
        if (status != nector_ok) return status;
        long double wrapped = std::fmod(target, duration);
        if (wrapped < 0.0L) wrapped += duration;
        animation.position = static_cast<double>(direction > 0 ? wrapped : duration - wrapped);
        return nector_ok;
    }
    nimation_internal::finishLoopCount(animation);
    animation.position = direction > 0 ? animation.duration : 0.0;
    animation.playing = false;
    animation.finished = true;
    return nector_ok;
}

nector_status advanceWithEvents(nector_animation& animation, long double distance) {
    long double remaining = distance;
    for (;;) {
        const int direction = animation.travel_direction;
        const double endpoint = direction > 0 ? animation.duration : 0.0;
        double boundary = endpoint;
        bool frame_boundary = false;
        if (nimation_internal::hasFrameTimeline(animation.kind)) {
            const double candidate = nimation_internal::nextFrameBoundary(animation, direction);
            if ((static_cast<long double>(candidate) - animation.position) * direction <
                (static_cast<long double>(endpoint) - animation.position) * direction) {
                boundary = candidate;
                frame_boundary = true;
            }
        }
        long double step = (static_cast<long double>(boundary) - animation.position) * direction;
        if (step < 0.0L) step = 0.0L;
        if (remaining < step) {
            animation.position = static_cast<double>(static_cast<long double>(animation.position) +
                                                     remaining * direction);
            return nector_ok;
        }
        animation.position = boundary;
        remaining -= step;
        if (remaining < 0.0L) remaining = 0.0L;

        if (frame_boundary) {
            const nector_status status = nimation_internal::emitEvent(animation, nector_animation_frame_event);
            if (status != nector_ok) {
                animation.playing = false;
                return status;
            }
            if (remaining <= 0.0L) return nector_ok;
            continue;
        }

        if (animation.direction == nector_playback_ping_pong && direction > 0) {
            animation.travel_direction = -1;
            if (remaining <= 0.0L) return nector_ok;
            continue;
        }

        if (nimation_internal::loopLimitReached(animation)) {
            if (animation.direction == nector_playback_ping_pong) animation.travel_direction = 1;
            return nimation_internal::finishAnimation(animation);
        }

        nimation_internal::recordEventLoop(animation);
        if (animation.direction == nector_playback_ping_pong) {
            animation.travel_direction = 1;
        } else {
            animation.position = direction > 0 ? 0.0 : animation.duration;
        }
        nector_status status = nimation_internal::emitEvent(animation, nector_animation_loop_event);
        if (status == nector_ok && animation.direction != nector_playback_ping_pong &&
            nimation_internal::hasFrameTimeline(animation.kind)) {
            status = nimation_internal::emitEvent(animation, nector_animation_frame_event);
        }
        if (status != nector_ok) {
            animation.playing = false;
            return status;
        }
        if (remaining <= 0.0L) return nector_ok;
    }
}

}

namespace nimation_internal {

nector_status tickAnimation(nector_animation& animation, double delta_ms) {
    if (!std::isfinite(delta_ms) || delta_ms < 0 || delta_ms > 1.0e9) {
        return nectorFail(nector_invalid_argument, "animation delta is invalid");
    }
    if (animation.dispatching_event) {
        return nectorFail(nector_invalid_state, "animation Tick cannot be nested inside an event callback");
    }
    if (!animation.playing || animation.finished || delta_ms == 0.0) return nector_ok;
    const long double distance = static_cast<long double>(delta_ms) *
                                 static_cast<long double>(animation.playback_rate);
    if (animation.event_callback == nullptr) return advanceWithoutEvents(animation, distance);
    return advanceWithEvents(animation, distance);
}

}
