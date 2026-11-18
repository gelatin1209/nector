#ifndef nector_nimation_loop_counter_h
#define nector_nimation_loop_counter_h

#include "animation_state.h"

namespace nimation_internal {

nector_status setLoopCount(nector_animation& animation, int32_t loop_count);
uint64_t configuredLoops(const nector_animation& animation);
uint64_t remainingLoops(const nector_animation& animation);
bool crossingsWithinLimit(const nector_animation& animation, long double crossings);
nector_status recordFastPathLoops(nector_animation& animation, long double crossings);
void finishLoopCount(nector_animation& animation);
bool loopLimitReached(const nector_animation& animation);
void recordEventLoop(nector_animation& animation);

}

#endif
