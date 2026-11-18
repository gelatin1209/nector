#include "cubic_bezier.h"

#include <cmath>

namespace nimation_internal {

double bezierCoordinate(double t, double first, double second) {
    const double inverse = 1.0 - t;
    return 3.0 * inverse * inverse * t * first +
           3.0 * inverse * t * t * second + t * t * t;
}

double bezierDerivative(double t, double first, double second) {
    const double inverse = 1.0 - t;
    return 3.0 * inverse * inverse * first +
           6.0 * inverse * t * (second - first) +
           3.0 * t * t * (1.0 - second);
}

double evaluateCubicBezier(double value, const nector_easing_parameters& parameters) {
    if (value <= 0.0 || value >= 1.0) return value;
    double lower = 0.0;
    double upper = 1.0;
    double t = value;
    for (int iteration = 0; iteration < 8; ++iteration) {
        const double x = bezierCoordinate(t, parameters.x1, parameters.x2) - value;
        if (std::fabs(x) < 1.0e-7) return bezierCoordinate(t, parameters.y1, parameters.y2);
        const double derivative = bezierDerivative(t, parameters.x1, parameters.x2);
        if (x > 0.0) upper = t;
        else lower = t;
        if (std::fabs(derivative) < 1.0e-7) break;
        const double candidate = t - x / derivative;
        if (candidate <= lower || candidate >= upper) break;
        t = candidate;
    }
    for (int iteration = 0; iteration < 24; ++iteration) {
        t = (lower + upper) * 0.5;
        const double x = bezierCoordinate(t, parameters.x1, parameters.x2);
        if (std::fabs(x - value) < 1.0e-7) break;
        if (x > value) upper = t;
        else lower = t;
    }
    return bezierCoordinate(t, parameters.y1, parameters.y2);
}

}
