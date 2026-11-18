#include "internal.h"

nector_status nectorCreateBehaviourAnimation(nector_behaviour behaviour, double duration_ms, double amount,
                                             nector_animation_handle* out_animation) {
    return nimation_internal::createBehaviourAnimation(behaviour, duration_ms, amount, out_animation);
}

nector_status nectorAnimationAddBehaviour(nector_animation_handle animation, nector_behaviour behaviour,
                                          int32_t start_frame, int32_t end_frame,
                                          double start_value, double end_value) {
    if (!nimation_internal::validAnimation(animation)) {
        return nectorFail(nector_invalid_argument, "animation is null");
    }
    return nimation_internal::addBehaviourSpan(*animation, behaviour, start_frame, end_frame,
                                               start_value, end_value);
}

nector_status nectorAnimationSampleBehaviourEx(nector_animation_handle animation,
                                                nector_behaviour_sample_ex* out_sample) {
    if (!nimation_internal::validAnimation(animation) || out_sample == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    return nimation_internal::sampleBehaviour(*animation, out_sample);
}

nector_status nectorAnimationSampleBehaviour(nector_animation_handle animation, nector_behaviour_sample* out_sample) {
    if (out_sample == nullptr) return nectorFail(nector_invalid_argument, "output sample is null");
    if (!nimation_internal::validAnimation(animation)) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    return nimation_internal::sampleLegacyBehaviour(*animation, out_sample);
}
