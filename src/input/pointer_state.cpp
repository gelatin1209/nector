#include "pointer_state.h"
#include "../core/internal.h"
#include "event_validation.h"

#include <cstddef>
#include <iterator>
#include <mutex>

namespace input_internal {

void updatePointerStateLocked(nector_input_handle input, const nector_input_event& event) {
    if (event.type == nector_event_mouse_down) {
        input->buttons[static_cast<size_t>(event.button)] = true;
        input->mouse_x = event.x;
        input->mouse_y = event.y;
    } else if (event.type == nector_event_mouse_up) {
        input->buttons[static_cast<size_t>(event.button)] = false;
        input->mouse_x = event.x;
        input->mouse_y = event.y;
    } else if (event.type == nector_event_mouse_move || event.type == nector_event_mouse_wheel) {
        input->mouse_x = event.x;
        input->mouse_y = event.y;
        if (event.type == nector_event_mouse_wheel) {
            input->wheel_x += event.dx;
            input->wheel_y += event.dy;
        }
    } else if (event.type == nector_event_touch_begin) {
        input->touches[event.touch_id] = event;
    } else if (event.type == nector_event_touch_move) {
        const auto touch = input->touches.find(event.touch_id);
        if (touch != input->touches.end()) touch->second = event;
    } else if (event.type == nector_event_touch_end || event.type == nector_event_touch_cancel) {
        input->touches.erase(event.touch_id);
    }
}

void clearPointerStateLocked(nector_input_handle input) {
    input->buttons.fill(false);
    input->touches.clear();
    input->wheel_x = 0;
    input->wheel_y = 0;
}

}

nector_status nectorInputIsMouseDown(nector_input_handle input, int32_t button, int32_t* out_down) {
    if (input == nullptr || out_down == nullptr || !input_internal::validButton(button)) {
        return nectorFail(nector_invalid_argument, "mouse button query is invalid");
    }
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        *out_down = input->buttons[static_cast<size_t>(button)] ? 1 : 0;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorInputGetMousePosition(nector_input_handle input, float* out_x, float* out_y) {
    if (input == nullptr || out_x == nullptr || out_y == nullptr) return nectorFail(nector_invalid_argument, "mouse position output is null");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        *out_x = input->mouse_x;
        *out_y = input->mouse_y;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorInputGetWheelDelta(nector_input_handle input, float* out_x, float* out_y) {
    if (input == nullptr || out_x == nullptr || out_y == nullptr) return nectorFail(nector_invalid_argument, "wheel output is null");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        *out_x = input->wheel_x;
        *out_y = input->wheel_y;
        input->wheel_x = 0;
        input->wheel_y = 0;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorInputGetTouchCount(nector_input_handle input, int32_t* out_count) {
    if (input == nullptr || out_count == nullptr) return nectorFail(nector_invalid_argument, "touch count output is null");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        *out_count = static_cast<int32_t>(input->touches.size());
        return nector_ok;
    } nector_api_catch
}

nector_status nectorInputGetTouch(nector_input_handle input, int32_t index, nector_input_event* out_touch) {
    if (input == nullptr || out_touch == nullptr || index < 0) return nectorFail(nector_invalid_argument, "touch query is invalid");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        if (static_cast<size_t>(index) >= input->touches.size()) return nectorFail(nector_not_found, "touch index not found");
        auto found = input->touches.begin();
        std::advance(found, index);
        *out_touch = found->second;
        return nector_ok;
    } nector_api_catch
}
