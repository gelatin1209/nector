#include "loop_counter.h"

#include <limits>

namespace nimation_internal {

nector_status setLoopCount(nector_animation& animation, int32_t loop_count) {
    if (loop_count < -1 || loop_count > 1000000) {
        return nectorFail(nector_invalid_argument, "loop count is invalid");
    }
    animation.loop_count = loop_count;
    return nector_ok;
}

uint64_t configuredLoops(const nector_animation& animation) {
    return animation.loop_count < 0 ? 0 : static_cast<uint64_t>(animation.loop_count);
}

uint64_t remainingLoops(const nector_animation& animation) {
    const uint64_t configured_loops = configuredLoops(animation);
    return animation.loop_count >= 0 && animation.completed_loops < configured_loops
        ? configured_loops - animation.completed_loops : 0;
}

bool crossingsWithinLimit(const nector_animation& animation, long double crossings) {
    return animation.loop_count == -1 || crossings <= static_cast<long double>(remainingLoops(animation));
}

nector_status recordFastPathLoops(nector_animation& animation, long double crossings) {
    if (animation.loop_count == -1 && crossings > static_cast<long double>(
            std::numeric_limits<uint64_t>::max() - animation.completed_loops)) {
        return nectorFail(nector_limit_exceeded, "animation loop counter overflow");
    }
    if (animation.loop_count >= 0) animation.completed_loops += static_cast<uint64_t>(crossings);
    return nector_ok;
}

void finishLoopCount(nector_animation& animation) {
    animation.completed_loops = configuredLoops(animation);
}

bool loopLimitReached(const nector_animation& animation) {
    return animation.loop_count >= 0 && animation.completed_loops >= configuredLoops(animation);
}

void recordEventLoop(nector_animation& animation) {
    ++animation.completed_loops;
}

}
