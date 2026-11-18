#include "behaviour_sampler.h"

#include "animation_validation.h"
#include "behaviour_track.h"

#include <algorithm>

namespace {

void applyLegacyBehaviour(const nector_animation& animation, nector_behaviour_sample_ex* out_sample) {
    const double progress = animation.position / animation.duration;
    const double amount = animation.behaviour_amount * progress;
    switch (animation.behaviour) {
        case nector_behaviour_forward:
            out_sample->y = -amount;
            break;
        case nector_behaviour_backward:
            out_sample->y = amount;
            break;
        case nector_behaviour_turn_left:
            out_sample->rotation_degrees = -amount;
            break;
        case nector_behaviour_turn_right:
            out_sample->rotation_degrees = amount;
            break;
        case nector_behaviour_hide:
            out_sample->visible = animation.position < animation.duration ? 1 : 0;
            break;
        case nector_behaviour_scale:
            out_sample->scale = 1.0 + (animation.behaviour_amount - 1.0) * progress;
            break;
        case nector_behaviour_fade:
            out_sample->opacity = std::max(0.0, std::min(1.0,
                1.0 + (animation.behaviour_amount - 1.0) * progress));
            break;
        default:
            break;
    }
}

bool applySpan(const nimation_internal::BehaviourSpan& span, double frame_duration,
               double position, nector_behaviour_sample_ex* out_sample) {
    const double start = static_cast<double>(span.start_frame) * frame_duration;
    const double end = static_cast<double>(span.end_frame + 1) * frame_duration;
    if (position < start) return false;

    const double progress = position >= end ? 1.0 : (position - start) / (end - start);
    const long double interpolated = static_cast<long double>(span.start_value) * (1.0 - progress) +
                                     static_cast<long double>(span.end_value) * progress;
    const double value = static_cast<double>(interpolated);
    switch (span.behaviour) {
        case nector_behaviour_forward:
            out_sample->y = -value;
            break;
        case nector_behaviour_backward:
            out_sample->y = value;
            break;
        case nector_behaviour_turn_left:
            out_sample->rotation_degrees = -value;
            break;
        case nector_behaviour_turn_right:
            out_sample->rotation_degrees = value;
            break;
        case nector_behaviour_hide:
            out_sample->visible = position < end ? 1 : 0;
            break;
        case nector_behaviour_scale:
            out_sample->scale = value;
            break;
        case nector_behaviour_fade:
            out_sample->opacity = value;
            break;
        default:
            break;
    }
    return true;
}

}

namespace nimation_internal {

nector_status sampleBehaviour(const nector_animation& animation, nector_behaviour_sample_ex* out_sample) {
    *out_sample = {0, 0, 0, 1, 1, 1};
    if (animation.kind == AnimationKind::behaviour) {
        if (!validBehaviour(animation.behaviour)) {
            return nectorFail(nector_invalid_state, "stored behaviour is invalid");
        }
        applyLegacyBehaviour(animation, out_sample);
        return nector_ok;
    }
    if (!acceptsBehaviourSpans(animation.kind)) {
        return nectorFail(nector_invalid_state, "animation does not contain behaviour spans");
    }
    for (const BehaviourSpan& span : animation.behaviours) {
        applySpan(span, animation.frame_duration, animation.position, out_sample);
    }
    return nector_ok;
}

nector_status sampleLegacyBehaviour(const nector_animation& animation, nector_behaviour_sample* out_sample) {
    nector_behaviour_sample_ex extended{};
    const nector_status status = sampleBehaviour(animation, &extended);
    if (status != nector_ok) return status;
    *out_sample = {extended.x, extended.y, extended.rotation_degrees, extended.visible};
    return nector_ok;
}

}
