#include "input.h"
#include "../core/internal.h"
#include "event_ingest.h"
#include "input_clock.h"

#include <cstring>

nector_status nectorCreateInput(nector_input_handle* out_input) {
    if (out_input == nullptr) return nectorFail(nector_invalid_argument, "out_input is null");
    *out_input = nullptr;
    nector_api_try {
        *out_input = new nector_input;
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyInput(nector_input_handle input) {
    delete input;
}

nector_status nectorInputPush(nector_input_handle input, const nector_input_event* event) {
    return input_internal::pushAtNs(input, event, input_internal::monotonicTimeNs(), false);
}

nector_status nectorInputPushAt(nector_input_handle input, const nector_input_event* event, int64_t timestamp_ms) {
    int64_t timestamp_ns = 0;
    if (!input_internal::timestampToNs(timestamp_ms, timestamp_ns)) return nectorFail(nector_invalid_argument, "input timestamp is invalid");
    return input_internal::pushAtNs(input, event, timestamp_ns, false);
}

extern "C" nector_input_api nector_status nectorInputPushPlatform(
    nector_input_handle input, const nector_input_event* event, int32_t key_repeat) {
    return input_internal::pushAtNs(input, event, input_internal::monotonicTimeNs(), key_repeat != 0);
}

nector_status nectorInputInjectKey(nector_input_handle input, int32_t key, int32_t down, uint32_t modifiers) {
    if (down != 0 && down != 1) return nectorFail(nector_invalid_argument, "key down flag must be zero or one");
    nector_input_event event{};
    event.type = down != 0 ? nector_event_key_down : nector_event_key_up;
    event.key = key;
    event.modifiers = modifiers;
    return nectorInputPush(input, &event);
}

nector_status nectorInputInjectKeyAt(nector_input_handle input, int32_t key, int32_t down, uint32_t modifiers,
                                     int64_t timestamp_ms) {
    if (down != 0 && down != 1) return nectorFail(nector_invalid_argument, "key down flag must be zero or one");
    nector_input_event event{};
    event.type = down != 0 ? nector_event_key_down : nector_event_key_up;
    event.key = key;
    event.modifiers = modifiers;
    return nectorInputPushAt(input, &event, timestamp_ms);
}

nector_status nectorInputInjectText(nector_input_handle input, const char* utf8_text) {
    if (utf8_text == nullptr) return nectorFail(nector_invalid_argument, "text is null");
    const size_t length = std::strlen(utf8_text);
    if (length >= 32) return nectorFail(nector_limit_exceeded, "one injected text event is limited to 31 UTF-8 bytes");
    nector_input_event event{};
    event.type = nector_event_text_input;
    std::memcpy(event.text, utf8_text, length + 1);
    return nectorInputPush(input, &event);
}

nector_status nectorInputInjectMouse(nector_input_handle input, nector_input_event_type type, float x, float y, int32_t button) {
    if (type != nector_event_mouse_move && type != nector_event_mouse_down && type != nector_event_mouse_up && type != nector_event_mouse_wheel) {
        return nectorFail(nector_invalid_argument, "event type is not a mouse event");
    }
    nector_input_event event{};
    event.type = type;
    event.x = x;
    event.y = y;
    event.button = button;
    return nectorInputPush(input, &event);
}

nector_status nectorInputInjectTouch(nector_input_handle input, nector_input_event_type type, int64_t touch_id,
                                      float x, float y, float pressure) {
    if (type < nector_event_touch_begin || type > nector_event_touch_cancel) return nectorFail(nector_invalid_argument, "event type is not a touch event");
    if (pressure < 0 || pressure > 1) return nectorFail(nector_invalid_argument, "touch pressure must be between zero and one");
    nector_input_event event{};
    event.type = type;
    event.touch_id = touch_id;
    event.x = x;
    event.y = y;
    event.pressure = pressure;
    return nectorInputPush(input, &event);
}
