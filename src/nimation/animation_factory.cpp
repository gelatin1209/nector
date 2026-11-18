#include "animation_factory.h"

#include "animation_validation.h"
#include "behaviour_track.h"

#include <cmath>
#include <memory>

namespace nimation_internal {

nector_status createAnimationObject(AnimationKind kind, double duration_ms,
                                    nector_animation_handle* out_animation) {
    if (out_animation == nullptr) return nectorFail(nector_invalid_argument, "out_animation is null");
    *out_animation = nullptr;
    if (!validDuration(duration_ms)) return nectorFail(nector_invalid_argument, "animation duration is invalid");
    nector_api_try {
        std::unique_ptr<nector_animation> animation(new nector_animation);
        animation->kind = kind;
        animation->duration = duration_ms;
        *out_animation = animation.release();
        return nector_ok;
    } nector_api_catch
}

nector_status createFrameTimeline(AnimationKind kind, int32_t frame_count, double frame_duration_ms,
                                  nector_animation_handle* out_animation) {
    if (out_animation == nullptr) return nectorFail(nector_invalid_argument, "out_animation is null");
    *out_animation = nullptr;
    if (!validFrameCount(frame_count)) {
        return nectorFail(nector_invalid_argument, "frame count must be between 1 and 60");
    }
    if (!validFrameDuration(frame_count, frame_duration_ms)) {
        return nectorFail(nector_invalid_argument, "frame duration is invalid");
    }
    nector_api_try {
        std::unique_ptr<nector_animation> animation(new nector_animation);
        animation->kind = kind;
        animation->frame_count = frame_count;
        animation->frame_duration = frame_duration_ms;
        animation->duration = frame_duration_ms * static_cast<double>(frame_count);
        if (hasImageClips(kind)) animation->images.resize(static_cast<size_t>(frame_count), nullptr);
        *out_animation = animation.release();
        return nector_ok;
    } nector_api_catch
}

nector_status createBehaviourAnimation(nector_behaviour behaviour, double duration_ms, double amount,
                                       nector_animation_handle* out_animation) {
    if (out_animation == nullptr) return nectorFail(nector_invalid_argument, "out_animation is null");
    *out_animation = nullptr;
    if (!validBehaviour(behaviour)) return nectorFail(nector_invalid_argument, "behaviour is invalid");
    if (!std::isfinite(amount) || amount < 0 ||
        (behaviour == nector_behaviour_fade && amount > 1.0)) {
        return nectorFail(nector_invalid_argument, "behaviour amount is invalid");
    }
    const nector_status status = createAnimationObject(AnimationKind::behaviour, duration_ms, out_animation);
    if (status != nector_ok) return status;
    (*out_animation)->behaviour = behaviour;
    (*out_animation)->behaviour_amount = amount;
    return nector_ok;
}

}
