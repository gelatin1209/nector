#ifndef nector_input_h
#define nector_input_h

#include "core.h"

#if defined(_WIN32)
  #if defined(nector_input_export)
    #define nector_input_api __declspec(dllexport)
  #else
    #define nector_input_api __declspec(dllimport)
  #endif
#else
  #define nector_input_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_input nector_input;
typedef nector_input* nector_input_handle;

typedef enum nector_input_event_type {
    nector_event_none = 0,
    nector_event_key_down = 1,
    nector_event_key_up = 2,
    nector_event_text_input = 3,
    nector_event_mouse_move = 4,
    nector_event_mouse_down = 5,
    nector_event_mouse_up = 6,
    nector_event_mouse_wheel = 7,
    nector_event_touch_begin = 8,
    nector_event_touch_move = 9,
    nector_event_touch_end = 10,
    nector_event_touch_cancel = 11,
    nector_event_focus_lost = 12,
    nector_event_window_close = 13,
    nector_event_window_resize = 14
} nector_input_event_type;

typedef enum nector_key {
    nector_key_unknown = 0,
    nector_key_backspace = 8,
    nector_key_tab = 9,
    nector_key_enter = 13,
    nector_key_escape = 27,
    nector_key_space = 32,
    nector_key_0 = 48,
    nector_key_1 = 49,
    nector_key_2 = 50,
    nector_key_3 = 51,
    nector_key_4 = 52,
    nector_key_5 = 53,
    nector_key_6 = 54,
    nector_key_7 = 55,
    nector_key_8 = 56,
    nector_key_9 = 57,
    nector_key_a = 65,
    nector_key_b = 66,
    nector_key_c = 67,
    nector_key_d = 68,
    nector_key_e = 69,
    nector_key_f = 70,
    nector_key_g = 71,
    nector_key_h = 72,
    nector_key_i = 73,
    nector_key_j = 74,
    nector_key_k = 75,
    nector_key_l = 76,
    nector_key_m = 77,
    nector_key_n = 78,
    nector_key_o = 79,
    nector_key_p = 80,
    nector_key_q = 81,
    nector_key_r = 82,
    nector_key_s = 83,
    nector_key_t = 84,
    nector_key_u = 85,
    nector_key_v = 86,
    nector_key_w = 87,
    nector_key_x = 88,
    nector_key_y = 89,
    nector_key_z = 90,
    nector_key_left = 0x100,
    nector_key_right = 0x101,
    nector_key_up = 0x102,
    nector_key_down = 0x103,
    nector_key_home = 0x104,
    nector_key_end = 0x105,
    nector_key_delete = 0x106,
    nector_key_shift = 0x107,
    nector_key_control = 0x108,
    nector_key_alt = 0x109
} nector_key;

typedef struct nector_input_event {
    nector_input_event_type type;
    int32_t key;
    int32_t button;
    uint32_t modifiers;
    int64_t touch_id;
    float x;
    float y;
    float dx;
    float dy;
    float pressure;
    int32_t width;
    int32_t height;
    char text[32];
} nector_input_event;

nector_input_api nector_status nectorCreateInput(nector_input_handle* out_input);
nector_input_api void nectorDestroyInput(nector_input_handle input);
nector_input_api nector_status nectorInputPush(nector_input_handle input, const nector_input_event* event);
/* Explicit timestamps are milliseconds on the same monotonic timeline used by nectorInputPush. */
nector_input_api nector_status nectorInputPushAt(nector_input_handle input, const nector_input_event* event, int64_t timestamp_ms);
nector_input_api nector_status nectorInputPoll(nector_input_handle input, nector_input_event* out_event);
nector_input_api nector_status nectorInputPeek(nector_input_handle input, nector_input_event* out_event);
nector_input_api nector_status nectorInputClear(nector_input_handle input);
nector_input_api nector_status nectorInputIsKeyDown(nector_input_handle input, int32_t key, int32_t* out_down);
/* Key state is 0 for released, 1 for held <= 500ms, and 2 for held > 500ms. */
nector_input_api nector_status nectorInputGetKeyState(nector_input_handle input, int32_t key, int32_t* out_state);
nector_input_api nector_status nectorInputGetKeyStateAt(nector_input_handle input, int32_t key, int64_t timestamp_ms, int32_t* out_state);
nector_input_api nector_status nectorInputIsMouseDown(nector_input_handle input, int32_t button, int32_t* out_down);
nector_input_api nector_status nectorInputGetMousePosition(nector_input_handle input, float* out_x, float* out_y);
nector_input_api nector_status nectorInputGetWheelDelta(nector_input_handle input, float* out_x, float* out_y);
nector_input_api nector_status nectorInputGetTouchCount(nector_input_handle input, int32_t* out_count);
nector_input_api nector_status nectorInputGetTouch(nector_input_handle input, int32_t index, nector_input_event* out_touch);
nector_input_api nector_status nectorInputInjectKey(nector_input_handle input, int32_t key, int32_t down, uint32_t modifiers);
nector_input_api nector_status nectorInputInjectKeyAt(nector_input_handle input, int32_t key, int32_t down, uint32_t modifiers, int64_t timestamp_ms);
nector_input_api nector_status nectorInputInjectText(nector_input_handle input, const char* utf8_text);
nector_input_api nector_status nectorInputInjectMouse(nector_input_handle input, nector_input_event_type type, float x, float y, int32_t button);
nector_input_api nector_status nectorInputInjectTouch(nector_input_handle input, nector_input_event_type type, int64_t touch_id, float x, float y, float pressure);

#ifdef __cplusplus
}
#endif
#endif
