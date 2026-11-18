#include "behaviour_track.h"

#include "animation_validation.h"

#include <algorithm>
#include <cmath>

namespace nimation_internal {

bool validBehaviour(nector_behaviour behaviour) {
    return behaviour >= nector_behaviour_forward && behaviour <= nector_behaviour_fade;
}

nector_status addBehaviourSpan(nector_animation& animation, nector_behaviour behaviour,
                               int32_t start_frame, int32_t end_frame,
                               double start_value, double end_value) {
    if (!acceptsBehaviourSpans(animation.kind)) {
        return nectorFail(nector_invalid_state, "animation does not accept behaviour spans");
    }
    if (!validBehaviour(behaviour)) return nectorFail(nector_invalid_argument, "behaviour is invalid");
    if (start_frame < 0 || end_frame < start_frame || end_frame >= animation.frame_count) {
        return nectorFail(nector_invalid_argument, "behaviour frame range is invalid");
    }
    if (!std::isfinite(start_value) || !std::isfinite(end_value) ||
        (behaviour == nector_behaviour_scale && (start_value < 0.0 || end_value < 0.0)) ||
        (behaviour == nector_behaviour_fade &&
         (start_value < 0.0 || start_value > 1.0 || end_value < 0.0 || end_value > 1.0))) {
        return nectorFail(nector_invalid_argument, "behaviour values are invalid");
    }
    nector_api_try {
        animation.behaviours.push_back({behaviour, start_frame, end_frame, start_value, end_value});
        std::stable_sort(animation.behaviours.begin(), animation.behaviours.end(),
                         [](const BehaviourSpan& first, const BehaviourSpan& second) {
                             return first.start_frame < second.start_frame;
                         });
        return nector_ok;
    } nector_api_catch
}

}
