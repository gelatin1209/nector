#ifndef nector_nimation_behaviour_sampler_h
#define nector_nimation_behaviour_sampler_h

#include "animation_state.h"

namespace nimation_internal {

nector_status sampleBehaviour(const nector_animation& animation, nector_behaviour_sample_ex* out_sample);
nector_status sampleLegacyBehaviour(const nector_animation& animation, nector_behaviour_sample* out_sample);

}

#endif
