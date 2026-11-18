#if defined(_WIN32)

#ifndef WINVER
#define WINVER 0x0601
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "backend.h"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02e0
#endif

namespace {

constexpr UINT window_message_dpi_changed = WM_DPICHANGED;

std::wstring utf8ToWide(const char* text, bool& valid) {
    valid = false;
    if (text == nullptr) return {};
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (length <= 0) return {};
    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result.data(), length) != length) return {};
    valid = true;
    return result;
}

uint32_t currentModifiers() {
    uint32_t modifiers = 0;
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0) modifiers |= platform_shift_modifier;
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) modifiers |= platform_control_modifier;
    if ((GetKeyState(VK_MENU) & 0x8000) != 0) modifiers |= platform_alt_modifier;
    return modifiers;
}

int32_t mapKey(WPARAM key) {
    switch (key) {
        case VK_BACK: return nector_key_backspace;
        case VK_TAB: return nector_key_tab;
        case VK_RETURN: return nector_key_enter;
        case VK_ESCAPE: return nector_key_escape;
        case VK_SPACE: return nector_key_space;
        case VK_LEFT: return nector_key_left;
        case VK_RIGHT: return nector_key_right;
        case VK_UP: return nector_key_up;
        case VK_DOWN: return nector_key_down;
        case VK_HOME: return nector_key_home;
        case VK_END: return nector_key_end;
        case VK_DELETE: return nector_key_delete;
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT: return nector_key_shift;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL: return nector_key_control;
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU: return nector_key_alt;
        default:
            if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')) return static_cast<int32_t>(key);
            return nector_key_unknown;
    }
}

void encodeUtf8(uint32_t codepoint, char* output, size_t& length) {
    if (codepoint <= 0x7fu) {
        output[0] = static_cast<char>(codepoint);
        length = 1;
    } else if (codepoint <= 0x7ffu) {
        output[0] = static_cast<char>(0xc0u | (codepoint >> 6));
        output[1] = static_cast<char>(0x80u | (codepoint & 0x3fu));
        length = 2;
    } else if (codepoint <= 0xffffu) {
        output[0] = static_cast<char>(0xe0u | (codepoint >> 12));
        output[1] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu));
        output[2] = static_cast<char>(0x80u | (codepoint & 0x3fu));
        length = 3;
    } else {
        output[0] = static_cast<char>(0xf0u | (codepoint >> 18));
        output[1] = static_cast<char>(0x80u | ((codepoint >> 12) & 0x3fu));
        output[2] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3fu));
        output[3] = static_cast<char>(0x80u | (codepoint & 0x3fu));
        length = 4;
    }
}

class Win32Window;

class Win32Backend final : public PlatformBackend {
public:
    explicit Win32Backend(nector_application_handle application)
        : application_(application), instance_(GetModuleHandleW(nullptr)), thread_id_(GetCurrentThreadId()) {
        using SetAwarenessContext = BOOL(WINAPI*)(HANDLE);
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        auto set_awareness = reinterpret_cast<SetAwarenessContext>(
            reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDpiAwarenessContext")));
        if (set_awareness != nullptr) {
            const auto context = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-4));
            set_awareness(context);
        } else {
            using SetAware = BOOL(WINAPI*)();
            auto set_aware = reinterpret_cast<SetAware>(reinterpret_cast<void*>(GetProcAddress(user32, "SetProcessDPIAware")));
            if (set_aware != nullptr) set_aware();
        }
        class_name_ = L"NectorWindow_" + std::to_wstring(GetCurrentProcessId()) + L"_" +
                      std::to_wstring(reinterpret_cast<uintptr_t>(this));
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        window_class.lpfnWndProc = &Win32Backend::windowProcedure;
        window_class.hInstance = instance_;
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.lpszClassName = class_name_.c_str();
        if (RegisterClassExW(&window_class) == 0) throw std::runtime_error("RegisterClassExW failed");
        registered_ = true;
    }

    ~Win32Backend() override {
        if (registered_) UnregisterClassW(class_name_.c_str(), instance_);
    }

    nector_status createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) override;

    nector_status processEvents(bool wait) override {
        MSG message{};
        if (wait && application_->running) {
            const BOOL result = GetMessageW(&message, nullptr, 0, 0);
            if (result < 0) return nectorFail(nector_backend_error, "GetMessageW failed");
            if (result == 0) {
                application_->running = false;
                return nector_ok;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
            if (message.message == WM_QUIT) {
                application_->running = false;
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return nector_ok;
    }

    void wakeUp() override {
        PostThreadMessageW(thread_id_, WM_NULL, 0, 0);
    }

    HINSTANCE instance() const { return instance_; }
    const std::wstring& className() const { return class_name_; }

private:
    static LRESULT CALLBACK windowProcedure(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

    nector_application_handle application_;
    HINSTANCE instance_;
    DWORD thread_id_;
    std::wstring class_name_;
    bool registered_ = false;
};

class Win32Window final : public PlatformWindow {
public:
    Win32Window(Win32Backend& backend, nector_window_handle window) : backend_(backend), window_(window) {}

    ~Win32Window() override {
        closeNative();
    }

    nector_status create() {
        const char* title = nullptr;
        nector_status status = nectorSceneGetTitle(window_->scene, &title);
        if (status != nector_ok) return status;
        bool valid = false;
        const std::wstring wide_title = utf8ToWide(title, valid);
        if (!valid) return nectorFail(nector_invalid_argument, "window title is not valid UTF-8");
        const UINT dpi = systemDpi();
        RECT rectangle{0, 0, window_->width, window_->height};
        adjustRectangle(rectangle, dpi);
        hwnd_ = CreateWindowExW(0, backend_.className().c_str(), wide_title.c_str(), WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left,
                                rectangle.bottom - rectangle.top, nullptr, nullptr, backend_.instance(), this);
        if (hwnd_ == nullptr) return nectorFail(nector_backend_error, "CreateWindowExW failed");
        if (RegisterTouchWindow(hwnd_, 0) == 0) return nectorFail(nector_backend_error, "RegisterTouchWindow failed");
        return nector_ok;
    }

    nector_status show() override {
        if (hwnd_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        return nector_ok;
    }

    nector_status hide() override {
        if (hwnd_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        ShowWindow(hwnd_, SW_HIDE);
        return nector_ok;
    }

    nector_status requestClose() override {
        if (hwnd_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        if (PostMessageW(hwnd_, WM_CLOSE, 0, 0) == 0) return nectorFail(nector_backend_error, "PostMessageW failed");
        return nector_ok;
    }

    nector_status setTitle(const char* title) override {
        if (hwnd_ == nullptr) return nectorFail(nector_invalid_state, "native window is closed");
        bool valid = false;
        const std::wstring wide_title = utf8ToWide(title, valid);
        if (!valid) return nectorFail(nector_invalid_argument, "window title is not valid UTF-8");
        if (SetWindowTextW(hwnd_, wide_title.c_str()) == 0) return nectorFail(nector_backend_error, "SetWindowTextW failed");
        return nector_ok;
    }

    nector_status present(const std::vector<uint8_t>& rgba, int32_t width, int32_t height) override {
        if (hwnd_ == nullptr || width <= 0 || height <= 0) return nector_ok;
        const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (rgba.size() < pixel_count * 4) return nectorFail(nector_invalid_argument, "RGBA pixel buffer is too small");
        native_pixels_.resize(pixel_count * 4);
        for (size_t index = 0; index < pixel_count; ++index) {
            const size_t offset = index * 4;
            native_pixels_[offset] = rgba[offset + 2];
            native_pixels_[offset + 1] = rgba[offset + 1];
            native_pixels_[offset + 2] = rgba[offset];
            native_pixels_[offset + 3] = rgba[offset + 3];
        }
        HDC device_context = GetDC(hwnd_);
        if (device_context == nullptr) return nectorFail(nector_backend_error, "GetDC failed");
        BITMAPINFO bitmap_info{};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = width;
        bitmap_info.bmiHeader.biHeight = -height;
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;
        const int result = StretchDIBits(device_context, 0, 0, width, height, 0, 0, width, height,
                                         native_pixels_.data(), &bitmap_info, DIB_RGB_COLORS, SRCCOPY);
        ReleaseDC(hwnd_, device_context);
        if (result == 0) return nectorFail(nector_backend_error, "StretchDIBits failed");
        return nector_ok;
    }

    void closeNative() override {
        if (hwnd_ != nullptr) {
            HWND closing_window = hwnd_;
            hwnd_ = nullptr;
            if (GetWindowLongPtrW(closing_window, GWLP_USERDATA) == reinterpret_cast<LONG_PTR>(this)) {
                SetWindowLongPtrW(closing_window, GWLP_USERDATA, 0);
            }
            DestroyWindow(closing_window);
        }
        active_touches_.clear();
    }

    LRESULT handleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
        switch (message) {
            case WM_CREATE:
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT paint{};
                BeginPaint(hwnd_, &paint);
                record(platformRenderWindow(window_));
                EndPaint(hwnd_, &paint);
                return 0;
            }
            case WM_SIZE:
                record(platformResizeWindow(window_, static_cast<int32_t>(LOWORD(l_param)),
                                            static_cast<int32_t>(HIWORD(l_param))));
                if (hwnd_ != nullptr) InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case window_message_dpi_changed: {
                const RECT* suggested = reinterpret_cast<const RECT*>(l_param);
                SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
                return 0;
            }
            case WM_CLOSE:
                platformCloseWindow(window_);
                return 0;
            case WM_KILLFOCUS:
                dispatchSimple(nector_event_focus_lost);
                return 0;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
                return keyMessage(message, w_param, l_param);
            case WM_CHAR:
                textMessage(w_param);
                return 0;
            case WM_MOUSEMOVE:
                mouseMessage(nector_event_mouse_move, l_param, 0);
                return 0;
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
                return buttonMessage(message, l_param);
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                xButtonMessage(message, w_param, l_param);
                return TRUE;
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
                wheelMessage(message, w_param, l_param);
                return 0;
            case WM_TOUCH:
                touchMessage(w_param, l_param);
                return 0;
            case WM_CANCELMODE:
                cancelTouches();
                return DefWindowProcW(hwnd_, message, w_param, l_param);
            default:
                return DefWindowProcW(hwnd_, message, w_param, l_param);
        }
    }

private:
    friend class Win32Backend;

    void record(nector_status status) {
        platformRecordEventStatus(window_, status);
    }

    void dispatch(nector_input_event& event, bool key_repeat = false) {
        event.modifiers = currentModifiers();
        record(platformDispatchInput(window_, event, key_repeat));
    }

    void dispatchSimple(nector_input_event_type type) {
        nector_input_event event{};
        event.type = type;
        dispatch(event);
    }

    LRESULT keyMessage(UINT message, WPARAM w_param, LPARAM l_param) {
        const int32_t key = mapKey(w_param);
        if (key == nector_key_unknown) return DefWindowProcW(hwnd_, message, w_param, l_param);
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        const bool repeat = down && (static_cast<uintptr_t>(l_param) & (uintptr_t{1} << 30)) != 0;
        nector_input_event event{};
        event.type = down ? nector_event_key_down : nector_event_key_up;
        event.key = key;
        dispatch(event, repeat);
        return 0;
    }

    void textMessage(WPARAM w_param) {
        uint32_t codepoint = static_cast<uint32_t>(w_param & 0xffffu);
        if (codepoint >= 0xd800u && codepoint <= 0xdbffu) {
            high_surrogate_ = static_cast<uint16_t>(codepoint);
            return;
        }
        if (codepoint >= 0xdc00u && codepoint <= 0xdfffu) {
            if (high_surrogate_ == 0) return;
            codepoint = 0x10000u + ((static_cast<uint32_t>(high_surrogate_) - 0xd800u) << 10) +
                        (codepoint - 0xdc00u);
            high_surrogate_ = 0;
        } else {
            high_surrogate_ = 0;
            if (codepoint < 0x20u || codepoint == 0x7fu) return;
        }
        char utf8[4]{};
        size_t length = 0;
        encodeUtf8(codepoint, utf8, length);
        record(platformDispatchText(window_, utf8, length));
    }

    static float coordinate(LPARAM l_param, bool x_axis) {
        return static_cast<float>(x_axis ? GET_X_LPARAM(l_param) : GET_Y_LPARAM(l_param));
    }

    void mouseMessage(nector_input_event_type type, LPARAM l_param, int32_t button) {
        nector_input_event event{};
        event.type = type;
        event.button = button;
        event.x = coordinate(l_param, true);
        event.y = coordinate(l_param, false);
        dispatch(event);
    }

    LRESULT buttonMessage(UINT message, LPARAM l_param) {
        const bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
        int32_t button = 0;
        if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) button = 1;
        if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP) button = 2;
        if (down) SetCapture(hwnd_);
        mouseMessage(down ? nector_event_mouse_down : nector_event_mouse_up, l_param, button);
        if (!down && hwnd_ != nullptr && GetCapture() == hwnd_) ReleaseCapture();
        return 0;
    }

    void xButtonMessage(UINT message, WPARAM w_param, LPARAM l_param) {
        const bool down = message == WM_XBUTTONDOWN;
        const int32_t button = HIWORD(w_param) == XBUTTON1 ? 3 : 4;
        if (down) SetCapture(hwnd_);
        mouseMessage(down ? nector_event_mouse_down : nector_event_mouse_up, l_param, button);
        if (!down && hwnd_ != nullptr && GetCapture() == hwnd_) ReleaseCapture();
    }

    void wheelMessage(UINT message, WPARAM w_param, LPARAM l_param) {
        POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
        ScreenToClient(hwnd_, &point);
        nector_input_event event{};
        event.type = nector_event_mouse_wheel;
        event.x = static_cast<float>(point.x);
        event.y = static_cast<float>(point.y);
        const float amount = static_cast<float>(GET_WHEEL_DELTA_WPARAM(w_param)) / static_cast<float>(WHEEL_DELTA);
        if (message == WM_MOUSEWHEEL) event.dy = amount;
        else event.dx = amount;
        dispatch(event);
    }

    void touchMessage(WPARAM w_param, LPARAM l_param) {
        const UINT count = static_cast<UINT>(LOWORD(w_param));
        if (count == 0) return;
        HTOUCHINPUT handle = reinterpret_cast<HTOUCHINPUT>(l_param);
        std::vector<TOUCHINPUT> touches(count);
        if (GetTouchInputInfo(handle, count, touches.data(), sizeof(TOUCHINPUT)) == 0) {
            CloseTouchInputHandle(handle);
            record(nectorFail(nector_backend_error, "GetTouchInputInfo failed"));
            return;
        }
        for (const TOUCHINPUT& touch : touches) {
            POINT point{touch.x / 100, touch.y / 100};
            ScreenToClient(hwnd_, &point);
            nector_input_event event{};
            if ((touch.dwFlags & TOUCHEVENTF_DOWN) != 0) {
                event.type = nector_event_touch_begin;
                active_touches_.insert(touch.dwID);
            } else if ((touch.dwFlags & TOUCHEVENTF_UP) != 0) {
                event.type = nector_event_touch_end;
                active_touches_.erase(touch.dwID);
            } else if ((touch.dwFlags & TOUCHEVENTF_MOVE) != 0) {
                event.type = nector_event_touch_move;
            } else {
                continue;
            }
            event.touch_id = touch.dwID;
            event.x = static_cast<float>(point.x);
            event.y = static_cast<float>(point.y);
            event.pressure = event.type == nector_event_touch_end ? 0.0f : 1.0f;
            dispatch(event);
        }
        CloseTouchInputHandle(handle);
    }

    void cancelTouches() {
        const std::unordered_set<DWORD> touches = active_touches_;
        active_touches_.clear();
        for (DWORD identifier : touches) {
            nector_input_event event{};
            event.type = nector_event_touch_cancel;
            event.touch_id = identifier;
            dispatch(event);
        }
    }

    UINT systemDpi() const {
        using GetDpiForSystemFunction = UINT(WINAPI*)();
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        auto get_dpi = reinterpret_cast<GetDpiForSystemFunction>(
            reinterpret_cast<void*>(GetProcAddress(user32, "GetDpiForSystem")));
        if (get_dpi != nullptr) return get_dpi();
        HDC screen = GetDC(nullptr);
        if (screen == nullptr) return 96;
        const UINT dpi = static_cast<UINT>(GetDeviceCaps(screen, LOGPIXELSX));
        ReleaseDC(nullptr, screen);
        return dpi == 0 ? 96 : dpi;
    }

    void adjustRectangle(RECT& rectangle, UINT dpi) const {
        using AdjustForDpi = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        auto adjust = reinterpret_cast<AdjustForDpi>(
            reinterpret_cast<void*>(GetProcAddress(user32, "AdjustWindowRectExForDpi")));
        if (adjust != nullptr) {
            if (adjust(&rectangle, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi) != 0) return;
        }
        AdjustWindowRectEx(&rectangle, WS_OVERLAPPEDWINDOW, FALSE, 0);
    }

    Win32Backend& backend_;
    nector_window_handle window_;
    HWND hwnd_ = nullptr;
    uint16_t high_surrogate_ = 0;
    std::unordered_set<DWORD> active_touches_;
    std::vector<uint8_t> native_pixels_;
};

LRESULT CALLBACK Win32Backend::windowProcedure(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    Win32Window* native = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        native = static_cast<Win32Window*>(creation->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(native));
        native->hwnd_ = hwnd;
    }
    if (native == nullptr) return DefWindowProcW(hwnd, message, w_param, l_param);
    WindowGuard guard(native->window_);
    try {
        if (message == WM_NCDESTROY) {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            native->hwnd_ = nullptr;
            native->active_touches_.clear();
            return DefWindowProcW(hwnd, message, w_param, l_param);
        }
        return native->handleMessage(message, w_param, l_param);
    } catch (...) {
        return DefWindowProcW(hwnd, message, w_param, l_param);
    }
}

nector_status Win32Backend::createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) {
    std::unique_ptr<Win32Window> native(new Win32Window(*this, window));
    const nector_status status = native->create();
    if (status != nector_ok) return status;
    out_window = std::move(native);
    return nector_ok;
}

} // namespace

nector_status createPlatformBackend(nector_application_handle application, std::unique_ptr<PlatformBackend>& out_backend) {
    nector_api_try {
        std::unique_ptr<Win32Backend> backend(new Win32Backend(application));
        out_backend = std::move(backend);
        return nector_ok;
    } nector_api_catch
}

#endif
