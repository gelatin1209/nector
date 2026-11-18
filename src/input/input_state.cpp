#include "input_state.h"
#include "../core/internal.h"
#include "event_queue.h"
#include "key_state.h"
#include "pointer_state.h"

#include <mutex>

namespace input_internal {

void clearInputStateLocked(nector_input_handle input) {
    clearQueueLocked(input);
    clearKeyStateLocked(input);
    clearPointerStateLocked(input);
    input->mouse_x = 0;
    input->mouse_y = 0;
}

}

nector_status nectorInputClear(nector_input_handle input) {
    if (input == nullptr) return nectorFail(nector_invalid_argument, "input is null");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        input_internal::clearInputStateLocked(input);
        return nector_ok;
    } nector_api_catch
}
