#include "event_validation.h"
#include "../core/internal.h"

#include <cstddef>

namespace input_internal {

size_t boundedTextLength(const char* text) {
    size_t length = 0;
    while (length < 32 && text[length] != '\0') ++length;
    return length;
}

bool validEventType(nector_input_event_type type) {
    return type >= nector_event_none && type <= nector_event_window_resize;
}

bool validKey(int32_t key) {
    switch (key) {
        case nector_key_backspace:
        case nector_key_tab:
        case nector_key_enter:
        case nector_key_escape:
        case nector_key_space:
        case nector_key_left:
        case nector_key_right:
        case nector_key_up:
        case nector_key_down:
        case nector_key_home:
        case nector_key_end:
        case nector_key_delete:
        case nector_key_shift:
        case nector_key_control:
        case nector_key_alt:
            return true;
        default:
            return (key >= nector_key_0 && key <= nector_key_9) ||
                   (key >= nector_key_a && key <= nector_key_z);
    }
}

bool validButton(int32_t button) {
    return button >= 0 && button < 8;
}

nector_status validateEvent(const nector_input_event& event) {
    if (!validEventType(event.type)) return nectorFail(nector_invalid_argument, "input event type is invalid");
    if (!nectorFinite(event.x) || !nectorFinite(event.y) || !nectorFinite(event.dx) || !nectorFinite(event.dy) ||
        !nectorFinite(event.pressure)) return nectorFail(nector_invalid_argument, "input event coordinate is invalid");
    if ((event.type == nector_event_key_down || event.type == nector_event_key_up) && !validKey(event.key)) {
        return nectorFail(nector_invalid_argument, "input event key is invalid");
    }
    if (event.type >= nector_event_mouse_move && event.type <= nector_event_mouse_wheel && !validButton(event.button)) {
        return nectorFail(nector_invalid_argument, "input event mouse button is invalid");
    }
    if (event.type >= nector_event_touch_begin && event.type <= nector_event_touch_cancel &&
        (event.pressure < 0 || event.pressure > 1)) {
        return nectorFail(nector_invalid_argument, "touch pressure must be between zero and one");
    }
    if (event.type == nector_event_text_input) {
        const size_t length = boundedTextLength(event.text);
        if (length == 32) return nectorFail(nector_invalid_argument, "text input event is not null terminated");
        const nector_status utf8_status = nectorCoreValidateUtf8(event.text, length);
        if (utf8_status != nector_ok) return utf8_status;
    }
    return nector_ok;
}

}
