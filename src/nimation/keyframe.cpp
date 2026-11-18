#include "internal.h"

nector_status nectorAnimationAddKeyEx(nector_animation_handle animation, int32_t channel, double time_ms, double value,
                                      nector_easing easing, const nector_easing_parameters* parameters) {
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::addKey(*animation, channel, time_ms, value, easing, parameters);
}

nector_status nectorAnimationAddKey(nector_animation_handle animation, int32_t channel, double time_ms, double value,
                                    nector_easing easing) {
    if (easing < nector_ease_linear || easing > nector_ease_bounce) {
        return nectorFail(nector_invalid_argument, "animation easing is invalid");
    }
    if (!nimation_internal::validAnimation(animation)) return nectorFail(nector_invalid_argument, "animation is null");
    return nimation_internal::addKey(*animation, channel, time_ms, value, easing, nullptr);
}

nector_status nectorAnimationSample(nector_animation_handle animation, int32_t channel, double* out_value) {
    if (!nimation_internal::validAnimation(animation) || out_value == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    return nimation_internal::sampleChannel(*animation, channel, out_value);
}
