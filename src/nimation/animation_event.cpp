#include "animation_event.h"

#include "frame_position.h"

namespace nimation_internal {

void setEventCallback(nector_animation& animation, nector_animation_event_callback callback, void* user_data) {
    animation.event_callback = callback;
    animation.event_user_data = callback != nullptr ? user_data : nullptr;
}

nector_status emitEvent(nector_animation& animation, nector_animation_event_type type) {
    if (animation.event_callback == nullptr) return nector_ok;
    const nector_animation_event event{type, currentFrame(animation), animation.completed_loops, animation.position};
    const nector_animation_event_callback callback = animation.event_callback;
    void* const user_data = animation.event_user_data;
    animation.dispatching_event = true;
    try {
        callback(&event, user_data);
    } catch (...) {
        animation.dispatching_event = false;
        return nectorFail(nector_callback_error, "animation event callback threw an exception");
    }
    animation.dispatching_event = false;
    return nector_ok;
}

nector_status finishAnimation(nector_animation& animation) {
    animation.playing = false;
    animation.finished = true;
    return emitEvent(animation, nector_animation_finish_event);
}

}
