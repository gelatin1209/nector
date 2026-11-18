#include "event_ingest.h"
#include "../core/internal.h"
#include "event_validation.h"
#include "key_state.h"
#include "pointer_state.h"

#include <mutex>

namespace input_internal {

// Caller must hold input->mutex.
void updateStateLocked(nector_input_handle input, const nector_input_event& event,
                       int64_t timestamp_ns, bool key_repeat) {
    if (event.type == nector_event_key_down || event.type == nector_event_key_up) {
        updateKeyStateLocked(input, event, timestamp_ns, key_repeat);
    } else if ((event.type >= nector_event_mouse_move && event.type <= nector_event_touch_cancel)) {
        updatePointerStateLocked(input, event);
    } else if (event.type == nector_event_focus_lost) {
        clearKeyStateLocked(input);
        clearPointerStateLocked(input);
    }
}

nector_status pushAtNs(nector_input_handle input, const nector_input_event* event,
                       int64_t timestamp_ns, bool key_repeat) {
    if (input == nullptr || event == nullptr) return nectorFail(nector_invalid_argument, "input or event is null");
    const nector_status validation = validateEvent(*event);
    if (validation != nector_ok) return validation;
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        if (input->queue.size() >= input_queue_capacity) return nectorFail(nector_limit_exceeded, "input event queue is full");
        input->queue.push_back(*event);
        updateStateLocked(input, *event, timestamp_ns, key_repeat);
        return nector_ok;
    } nector_api_catch
}

}
