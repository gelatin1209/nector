#ifndef nector_nimation_easing_curve_h
#define nector_nimation_easing_curve_h

#include "nimation.h"

namespace nimation_internal {

bool validEasing(nector_easing easing, const nector_easing_parameters* parameters);
double applyEasing(double value, nector_easing easing);
double applyEasing(double value, nector_easing easing, const nector_easing_parameters& parameters);

}

#endif
