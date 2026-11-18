#include "keyframe_track.h"

#include "animation_validation.h"
#include "easing_curve.h"

#include <algorithm>
#include <cmath>

namespace nimation_internal {

nector_status addKey(nector_animation& animation, int32_t channel, double time_ms, double value,
                     nector_easing easing, const nector_easing_parameters* parameters) {
    if (!acceptsKeyframes(animation.kind)) {
        return nectorFail(nector_invalid_state, "animation does not accept keyframes");
    }
    if (channel < 0 || channel >= 10000 || !std::isfinite(time_ms) || !std::isfinite(value) ||
        time_ms < 0 || time_ms > animation.duration || !validEasing(easing, parameters)) {
        return nectorFail(nector_invalid_argument, "animation key or easing parameters are invalid");
    }
    nector_api_try {
        auto& keys = animation.channels[channel];
        if (keys.size() >= 100000) return nectorFail(nector_limit_exceeded, "animation key limit exceeded");
        const auto duplicate = std::find_if(keys.begin(), keys.end(), [&](const auto& key) { return key.time == time_ms; });
        if (duplicate != keys.end()) return nectorFail(nector_invalid_state, "duplicate keyframe time in channel");
        keys.push_back({time_ms, value, easing, parameters != nullptr ? *parameters : nector_easing_parameters{}});
        std::sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
        return nector_ok;
    } nector_api_catch
}

nector_status sampleChannel(const nector_animation& animation, int32_t channel, double* out_value) {
    *out_value = 0;
    const auto found = animation.channels.find(channel);
    if (found == animation.channels.end() || found->second.empty()) {
        return nectorFail(nector_not_found, "animation channel not found");
    }
    const auto& keys = found->second;
    if (animation.position <= keys.front().time) {
        *out_value = keys.front().value;
        return nector_ok;
    }
    if (animation.position >= keys.back().time) {
        *out_value = keys.back().value;
        return nector_ok;
    }
    const auto next = std::upper_bound(keys.begin(), keys.end(), animation.position,
        [](double time, const auto& key) { return time < key.time; });
    const auto previous = next - 1;
    const double interval = next->time - previous->time;
    const double amount = interval > 0 ? (animation.position - previous->time) / interval : 1.0;
    const double eased = applyEasing(amount, next->easing, next->parameters);
    *out_value = previous->value + (next->value - previous->value) * eased;
    return nector_ok;
}

}
