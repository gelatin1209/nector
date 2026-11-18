#include "frame_position.h"

#include "playback_direction.h"

#include <algorithm>
#include <cmath>

namespace nimation_internal {

int32_t currentFrame(const nector_animation& animation) {
    if (animation.frame_count <= 0) return 0;
    const double frame = reversing(animation)
        ? std::ceil(animation.position / animation.frame_duration) - 1.0
        : std::floor(animation.position / animation.frame_duration);
    const int32_t index = frame <= 0 ? 0 : static_cast<int32_t>(frame);
    return std::max(0, std::min(animation.frame_count - 1, index));
}

double nextFrameBoundary(const nector_animation& animation, int direction) {
    const long double position = static_cast<long double>(animation.position);
    const long double frame_duration = static_cast<long double>(animation.frame_duration);
    const long double frame = position / frame_duration;
    const long double index = direction > 0 ? std::floor(frame) + 1.0L : std::ceil(frame) - 1.0L;
    if (index <= 0.0L || index >= static_cast<long double>(animation.frame_count)) {
        return direction > 0 ? animation.duration : 0.0;
    }
    return static_cast<double>(index * frame_duration);
}

double framePosition(const nector_animation& animation, int32_t frame_index) {
    return static_cast<double>(frame_index) * animation.frame_duration;
}

}
