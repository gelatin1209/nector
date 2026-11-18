#include "event_queue.h"
#include "../core/internal.h"

#include <mutex>

namespace input_internal {

void clearQueueLocked(nector_input_handle input) {
    input->queue.clear();
}

}

nector_status nectorInputPoll(nector_input_handle input, nector_input_event* out_event) {
    if (input == nullptr || out_event == nullptr) return nectorFail(nector_invalid_argument, "input or out_event is null");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        if (input->queue.empty()) return nectorFail(nector_not_found, "input event queue is empty");
        *out_event = input->queue.front();
        input->queue.pop_front();
        return nector_ok;
    } nector_api_catch
}

nector_status nectorInputPeek(nector_input_handle input, nector_input_event* out_event) {
    if (input == nullptr || out_event == nullptr) return nectorFail(nector_invalid_argument, "input or out_event is null");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        if (input->queue.empty()) return nectorFail(nector_not_found, "input event queue is empty");
        *out_event = input->queue.front();
        return nector_ok;
    } nector_api_catch
}
