#include "easing_curve.h"

#include "cubic_bezier.h"

#include <algorithm>
#include <cmath>

namespace nimation_internal {

bool validEasing(nector_easing easing, const nector_easing_parameters* parameters) {
    if (easing >= nector_ease_linear && easing <= nector_ease_bounce) return true;
    if (parameters == nullptr) return false;
    if (easing == nector_ease_cubic_bezier) {
        return std::isfinite(parameters->x1) && std::isfinite(parameters->y1) &&
               std::isfinite(parameters->x2) && std::isfinite(parameters->y2) &&
               parameters->x1 >= 0.0 && parameters->x1 <= 1.0 &&
               parameters->x2 >= 0.0 && parameters->x2 <= 1.0;
    }
    if (easing == nector_ease_steps) {
        return parameters->steps > 0 && parameters->steps <= 1000000 &&
               (parameters->step_position == nector_step_end ||
                parameters->step_position == nector_step_start);
    }
    return false;
}

double applyEasing(double value, nector_easing easing,
                   const nector_easing_parameters& parameters) {
    value = std::max(0.0, std::min(1.0, value));
    switch (easing) {
        case nector_ease_linear: return value;
        case nector_ease_in: return value * value * value;
        case nector_ease_out: return 1.0 - std::pow(1.0 - value, 3.0);
        case nector_ease_in_out:
            return value < 0.5 ? 4.0 * value * value * value : 1.0 - std::pow(-2.0 * value + 2.0, 3.0) / 2.0;
        case nector_ease_smooth: return value * value * (3.0 - 2.0 * value);
        case nector_ease_bounce: {
            const double n = 7.5625;
            const double d = 2.75;
            if (value < 1.0 / d) return n * value * value;
            if (value < 2.0 / d) {
                value -= 1.5 / d;
                return n * value * value + 0.75;
            }
            if (value < 2.5 / d) {
                value -= 2.25 / d;
                return n * value * value + 0.9375;
            }
            value -= 2.625 / d;
            return n * value * value + 0.984375;
        }
        case nector_ease_cubic_bezier: return evaluateCubicBezier(value, parameters);
        case nector_ease_steps: {
            const double stepped = value * static_cast<double>(parameters.steps);
            const double index = parameters.step_position == nector_step_start
                ? std::min(static_cast<double>(parameters.steps), std::floor(stepped) + 1.0)
                : std::floor(stepped);
            return index / static_cast<double>(parameters.steps);
        }
        default: return value;
    }
}

double applyEasing(double value, nector_easing easing) {
    const nector_easing_parameters parameters{};
    return applyEasing(value, easing, parameters);
}

}
