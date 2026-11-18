#include "nector.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expectStatus(nector_status actual, nector_status expected, const char* message) {
    if (actual != expected) throw std::runtime_error(message);
}

int32_t stateAt(nector_input_handle input, int32_t key, int64_t timestamp_ms) {
    int32_t state = -1;
    check(nectorInputGetKeyStateAt(input, key, timestamp_ms, &state));
    return state;
}

void pushKeyAt(nector_input_handle input, nector_input_event_type type, int32_t key, int64_t timestamp_ms) {
    nector_input_event event{};
    event.type = type;
    event.key = key;
    check(nectorInputPushAt(input, &event, timestamp_ms));
}

void testKeyTiming(nector_input_handle input) {
    static_assert(nector_key_0 == '0' && nector_key_9 == '9', "digit key values changed");
    static_assert(nector_key_a == 'A' && nector_key_z == 'Z', "letter key values changed");

    require(stateAt(input, nector_key_a, 0) == 0, "a new input must have no pressed keys");
    pushKeyAt(input, nector_event_key_down, nector_key_a, 1000);
    require(stateAt(input, nector_key_a, 1000) == 1, "initial key down was not recorded");
    pushKeyAt(input, nector_event_key_down, nector_key_a, 1400);
    require(stateAt(input, nector_key_a, 1500) == 1, "500ms must remain in state 1");
    require(stateAt(input, nector_key_a, 1501) == 2, "501ms must enter state 2");
    require(stateAt(input, nector_key_a, 1900) == 2, "repeat down reset the first press timestamp");

    int32_t down = 0;
    check(nectorInputIsKeyDown(input, nector_key_a, &down));
    require(down == 1, "key is not down after repeated key down events");
    pushKeyAt(input, nector_event_key_up, nector_key_a, 2000);
    require(stateAt(input, nector_key_a, 2000) == 0, "key up did not clear key state");
    pushKeyAt(input, nector_event_key_up, nector_key_a, 2001);
    require(stateAt(input, nector_key_a, 5000) == 0, "duplicate key up resurrected key state");

    check(nectorInputInjectKeyAt(input, nector_key_z, 1, 7, 3000));
    require(stateAt(input, nector_key_z, 3500) == 1, "injected key 500ms boundary is wrong");
    require(stateAt(input, nector_key_z, 3501) == 2, "injected key 501ms boundary is wrong");
    check(nectorInputInjectKeyAt(input, nector_key_z, 0, 0, 3502));
    require(stateAt(input, nector_key_z, 3502) == 0, "injected key up did not clear state");

    check(nectorInputInjectKeyAt(input, nector_key_0, 1, 0, 4000));
    check(nectorInputClear(input));
    require(stateAt(input, nector_key_0, 4001) == 0, "clear did not clear key timing");
    nector_input_event event{};
    expectStatus(nectorInputPoll(input, &event), nector_not_found, "clear did not empty the queue");

    pushKeyAt(input, nector_event_key_down, nector_key_b, 5000);
    nector_input_event focus_lost{};
    focus_lost.type = nector_event_focus_lost;
    check(nectorInputPushAt(input, &focus_lost, 6000));
    require(stateAt(input, nector_key_b, 6000) == 0, "focus loss did not clear key timing");
}

void testQueueOrder(nector_input_handle input) {
    check(nectorInputClear(input));
    check(nectorInputInjectKeyAt(input, nector_key_c, 1, 11, 10));
    check(nectorInputInjectKeyAt(input, nector_key_c, 1, 12, 20));
    check(nectorInputInjectKeyAt(input, nector_key_c, 0, 13, 30));

    nector_input_event event{};
    check(nectorInputPeek(input, &event));
    require(event.type == nector_event_key_down && event.key == nector_key_c && event.modifiers == 11,
            "peek did not preserve the first queued event");
    const uint32_t expected_modifiers[] = {11, 12, 13};
    const nector_input_event_type expected_types[] = {
        nector_event_key_down, nector_event_key_down, nector_event_key_up
    };
    for (size_t index = 0; index < 3; ++index) {
        check(nectorInputPoll(input, &event));
        require(event.type == expected_types[index] && event.key == nector_key_c &&
                event.modifiers == expected_modifiers[index], "input queue order or event contents changed");
    }
    expectStatus(nectorInputPoll(input, &event), nector_not_found, "drained queue did not report not found");
}

void testInvalidInputs(nector_input_handle input) {
    check(nectorInputClear(input));
    int32_t state = 99;
    expectStatus(nectorInputGetKeyStateAt(input, -1, 0, &state), nector_invalid_argument,
                 "negative key was accepted");
    require(state == 0, "failed key query did not reset its output");
    expectStatus(nectorInputGetKeyStateAt(input, nector_key_unknown, 0, &state), nector_invalid_argument,
                 "unknown key was accepted");
    expectStatus(nectorInputGetKeyStateAt(input, 1, 0, &state), nector_invalid_argument,
                 "unassigned key was accepted");
    expectStatus(nectorInputGetKeyStateAt(input, 4096, 0, &state), nector_invalid_argument,
                 "out-of-range key was accepted");
    expectStatus(nectorInputGetKeyStateAt(input, nector_key_a, -1, &state), nector_invalid_argument,
                 "negative query timestamp was accepted");
    expectStatus(nectorInputGetKeyStateAt(nullptr, nector_key_a, 0, &state), nector_invalid_argument,
                 "null input was accepted");
    expectStatus(nectorInputGetKeyStateAt(input, nector_key_a, 0, nullptr), nector_invalid_argument,
                 "null state output was accepted");

    expectStatus(nectorInputInjectKeyAt(input, nector_key_a, 2, 0, 0), nector_invalid_argument,
                 "invalid down flag was accepted");
    expectStatus(nectorInputInjectKeyAt(input, 1, 1, 0, 0), nector_invalid_argument,
                 "invalid injected key was accepted");
    expectStatus(nectorInputInjectKeyAt(input, nector_key_a, 1, 0, -1), nector_invalid_argument,
                 "negative injected timestamp was accepted");
    expectStatus(nectorInputInjectKeyAt(input, nector_key_a, 1, 0, std::numeric_limits<int64_t>::max()),
                 nector_invalid_argument, "overflowing timestamp was accepted");

    pushKeyAt(input, nector_event_key_down, nector_key_d, 100);
    expectStatus(nectorInputGetKeyStateAt(input, nector_key_d, 99, &state), nector_invalid_argument,
                 "query before key press was accepted");
    require(state == 0, "failed backwards-time query did not reset its output");
    check(nectorInputClear(input));

    nector_input_event event{};
    event.type = nector_event_key_down;
    event.key = nector_key_unknown;
    expectStatus(nectorInputPushAt(input, &event, 0), nector_invalid_argument, "unknown key event was accepted");
    event = {};
    event.type = static_cast<nector_input_event_type>(99);
    expectStatus(nectorInputPush(input, &event), nector_invalid_argument, "invalid event type was accepted");
    event = {};
    event.type = nector_event_mouse_down;
    event.button = 8;
    expectStatus(nectorInputPush(input, &event), nector_invalid_argument, "invalid mouse button was accepted");
    event = {};
    event.type = nector_event_touch_begin;
    event.pressure = 1.1F;
    expectStatus(nectorInputPush(input, &event), nector_invalid_argument, "invalid touch pressure was accepted");
    event = {};
    event.type = nector_event_text_input;
    event.text[0] = static_cast<char>(0xc0);
    event.text[1] = static_cast<char>(0xaf);
    expectStatus(nectorInputPush(input, &event), nector_invalid_format, "invalid UTF-8 text was accepted");
    event = {};
    event.type = nector_event_text_input;
    std::memset(event.text, 'x', sizeof(event.text));
    expectStatus(nectorInputPush(input, &event), nector_invalid_argument, "unterminated text was accepted");
    expectStatus(nectorInputPoll(input, &event), nector_not_found, "rejected events entered the queue");
}

void testPointerState(nector_input_handle input) {
    check(nectorInputClear(input));
    check(nectorInputInjectMouse(input, nector_event_mouse_down, 1, 2, 1));
    check(nectorInputInjectMouse(input, nector_event_mouse_move, 3, 4, 0));
    nector_input_event wheel{};
    wheel.type = nector_event_mouse_wheel;
    wheel.x = 5;
    wheel.y = 6;
    wheel.dx = 0.25F;
    wheel.dy = -1.5F;
    check(nectorInputPushAt(input, &wheel, 10));

    float x = 0;
    float y = 0;
    check(nectorInputGetMousePosition(input, &x, &y));
    require(x == 5 && y == 6, "wheel event did not synchronize mouse position");
    int32_t down = 0;
    check(nectorInputIsMouseDown(input, 1, &down));
    require(down == 1, "mouse button state is wrong");
    check(nectorInputGetWheelDelta(input, &x, &y));
    require(x == 0.25F && y == -1.5F, "wheel accumulation is wrong");
    check(nectorInputGetWheelDelta(input, &x, &y));
    require(x == 0 && y == 0, "wheel delta was not consumed");

    check(nectorInputInjectTouch(input, nector_event_touch_begin, 7, 8, 9, 0.5F));
    check(nectorInputInjectTouch(input, nector_event_touch_move, 7, 10, 11, 0.75F));
    check(nectorInputInjectTouch(input, nector_event_touch_move, 8, 20, 21, 1.0F));
    int32_t count = 0;
    check(nectorInputGetTouchCount(input, &count));
    require(count == 1, "unknown touch move created a phantom touch");
    nector_input_event touch{};
    check(nectorInputGetTouch(input, 0, &touch));
    require(touch.touch_id == 7 && touch.x == 10 && touch.y == 11 && touch.pressure == 0.75F,
            "active touch was not updated");
    check(nectorInputInjectTouch(input, nector_event_touch_end, 7, 10, 11, 0));
    check(nectorInputGetTouchCount(input, &count));
    require(count == 0, "touch end did not remove active touch");

    check(nectorInputInjectMouse(input, nector_event_mouse_up, 12, 13, 1));
    check(nectorInputIsMouseDown(input, 1, &down));
    check(nectorInputGetMousePosition(input, &x, &y));
    require(down == 0 && x == 12 && y == 13, "mouse up state is wrong");

    check(nectorInputInjectTouch(input, nector_event_touch_begin, 9, 1, 2, 1));
    check(nectorInputPushAt(input, &wheel, 20));
    nector_input_event focus_lost{};
    focus_lost.type = nector_event_focus_lost;
    check(nectorInputPushAt(input, &focus_lost, 30));
    check(nectorInputGetTouchCount(input, &count));
    check(nectorInputGetWheelDelta(input, &x, &y));
    require(count == 0 && x == 0 && y == 0, "focus loss left transient pointer state behind");
}

void testQueueCapacity(nector_input_handle input) {
    check(nectorInputClear(input));
    nector_input_event event{};
    for (int32_t index = 0; index < 4096; ++index) check(nectorInputPushAt(input, &event, index));
    event.type = nector_event_key_down;
    event.key = nector_key_e;
    expectStatus(nectorInputPushAt(input, &event, 5000), nector_limit_exceeded, "queue overflow was accepted");
    require(stateAt(input, nector_key_e, 5000) == 0, "rejected overflow event changed key state");
    event = {};
    check(nectorInputPeek(input, &event));
    require(event.type == nector_event_none, "queue overflow changed the first queued event");
    check(nectorInputClear(input));
}

void testCppWrapper() {
    nector::Input input;
    ::NKeystroke keystroke(input);
    const ::NKeystroke::signal key = ::NKeystroke::signal::A;
    input.injectKeyAt(nector_key_a, true, 0, 1000);
    require(keystroke.stateAt(key, 1500) == 1, "C++ 500ms boundary is wrong");
    require(keystroke.stateAt(key, 1501) == 2, "C++ 501ms boundary is wrong");
    input.injectKeyAt(nector_key_a, false, 0, 1502);
    require(keystroke.stateAt(key, 1502) == 0, "C++ key release is wrong");
}

} // namespace

int main() {
    try {
        nector_input_handle input = nullptr;
        check(nectorCreateInput(&input));
        std::unique_ptr<nector_input, decltype(&nectorDestroyInput)> input_guard(input, nectorDestroyInput);
        testKeyTiming(input);
        testQueueOrder(input);
        testInvalidInputs(input);
        testPointerState(input);
        testQueueCapacity(input);
        testCppWrapper();
        std::cout << "test_input_keystroke: PASS; C/C++ input, repeat, 500/501ms, release, focus, validation, pointers, queue\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_input_keystroke: FAIL: " << error.what() << '\n';
        return 1;
    }
}
