#include "playback_direction.h"

namespace nimation_internal {

bool validPlaybackDirection(nector_playback_direction direction) {
    return direction >= nector_playback_normal && direction <= nector_playback_ping_pong;
}

int initialTravelDirection(const nector_animation& animation, double position) {
    return animation.direction == nector_playback_reverse ||
        (animation.direction == nector_playback_ping_pong && position >= animation.duration) ? -1 : 1;
}

double initialPosition(const nector_animation& animation) {
    return animation.direction == nector_playback_reverse ? animation.duration : 0.0;
}

bool reversing(const nector_animation& animation) {
    return animation.direction == nector_playback_reverse ||
        (animation.direction == nector_playback_ping_pong && animation.travel_direction < 0);
}

nector_status setPlaybackDirection(nector_animation& animation, nector_playback_direction direction) {
    if (!validPlaybackDirection(direction)) {
        return nectorFail(nector_invalid_argument, "playback direction is invalid");
    }
    if (animation.direction != direction) {
        animation.direction = direction;
        animation.travel_direction = initialTravelDirection(animation, animation.position);
    }
    return nector_ok;
}

}
