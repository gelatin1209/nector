#ifndef nector_nimation_cubic_bezier_h
#define nector_nimation_cubic_bezier_h

#include "nimation.h"

namespace nimation_internal {

double bezierCoordinate(double t, double first, double second);
double bezierDerivative(double t, double first, double second);
double evaluateCubicBezier(double value, const nector_easing_parameters& parameters);

}

#endif
