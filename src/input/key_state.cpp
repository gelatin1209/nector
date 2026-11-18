#include "key_state.h"
#include "../core/internal.h"
#include "event_validation.h"
#include "input_clock.h"

#include <cstddef>
#include <mutex>

namespace input_internal {

constexpr int64_t held_threshold_ns = 500 * nanoseconds_per_millisecond;

void updateKeyStateLocked(nector_input_handle input, const nector_input_event& event,
                          int64_t timestamp_ns, bool key_repeat) {
    const size_t key = static_cast<size_t>(event.key);
    if (event.type == nector_event_key_down) {
        if (input->keys[key] && key_repeat) return;
        if (!input->keys[key]) input->key_pressed_since_ns[key] = timestamp_ns;
        input->keys[key] = true;
    } else if (event.type == nector_event_key_up) {
        input->keys[key] = false;
        input->key_pressed_since_ns[key] = 0;
    }
}

void clearKeyStateLocked(nector_input_handle input) {
    input->keys.fill(false);
    input->key_pressed_since_ns.fill(0);
}

nector_status keyStateAtNs(nector_input_handle input, int32_t key, int64_t timestamp_ns, int32_t* out_state) {
    if (input == nullptr || out_state == nullptr) return nectorFail(nector_invalid_argument, "input or state output is null");
    *out_state = 0;
    if (!validKey(key)) return nectorFail(nector_invalid_argument, "key state query is invalid");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        const size_t index = static_cast<size_t>(key);
        if (!input->keys[index]) return nector_ok;
        const int64_t pressed_since_ns = input->key_pressed_since_ns[index];
        if (timestamp_ns < pressed_since_ns) return nectorFail(nector_invalid_argument, "key state timestamp predates key press");
        *out_state = timestamp_ns - pressed_since_ns > held_threshold_ns ? 2 : 1;
        return nector_ok;
    } nector_api_catch
}

}

nector_status nectorInputIsKeyDown(nector_input_handle input, int32_t key, int32_t* out_down) {
    if (input == nullptr || out_down == nullptr) return nectorFail(nector_invalid_argument, "key query is invalid");
    *out_down = 0;
    if (!input_internal::validKey(key)) return nectorFail(nector_invalid_argument, "key query is invalid");
    nector_api_try {
        std::lock_guard<std::mutex> lock(input->mutex);
        *out_down = input->keys[static_cast<size_t>(key)] ? 1 : 0;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorInputGetKeyState(nector_input_handle input, int32_t key, int32_t* out_state) {
    return input_internal::keyStateAtNs(input, key, input_internal::monotonicTimeNs(), out_state);
}

nector_status nectorInputGetKeyStateAt(nector_input_handle input, int32_t key, int64_t timestamp_ms, int32_t* out_state) {
    int64_t timestamp_ns = 0;
    if (!input_internal::timestampToNs(timestamp_ms, timestamp_ns)) {
        if (out_state != nullptr) *out_state = 0;
        return nectorFail(nector_invalid_argument, "key state timestamp is invalid");
    }
    return input_internal::keyStateAtNs(input, key, timestamp_ns, out_state);
}
