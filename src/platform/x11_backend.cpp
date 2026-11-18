#if defined(__linux__)

#include "backend.h"

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

uint32_t x11Modifiers(unsigned int state) {
    uint32_t modifiers = 0;
    if ((state & ShiftMask) != 0) modifiers |= platform_shift_modifier;
    if ((state & ControlMask) != 0) modifiers |= platform_control_modifier;
    if ((state & Mod1Mask) != 0) modifiers |= platform_alt_modifier;
    return modifiers;
}

int32_t x11Key(KeySym key) {
    switch (key) {
        case XK_BackSpace: return nector_key_backspace;
        case XK_Tab:
        case XK_ISO_Left_Tab: return nector_key_tab;
        case XK_Return:
        case XK_KP_Enter: return nector_key_enter;
        case XK_Escape: return nector_key_escape;
        case XK_space:
        case XK_KP_Space: return nector_key_space;
        case XK_Left:
        case XK_KP_Left: return nector_key_left;
        case XK_Right:
        case XK_KP_Right: return nector_key_right;
        case XK_Up:
        case XK_KP_Up: return nector_key_up;
        case XK_Down:
        case XK_KP_Down: return nector_key_down;
        case XK_Home:
        case XK_KP_Home: return nector_key_home;
        case XK_End:
        case XK_KP_End: return nector_key_end;
        case XK_Delete:
        case XK_KP_Delete: return nector_key_delete;
        case XK_Shift_L:
        case XK_Shift_R: return nector_key_shift;
        case XK_Control_L:
        case XK_Control_R: return nector_key_control;
        case XK_Alt_L:
        case XK_Alt_R:
        case XK_Meta_L:
        case XK_Meta_R: return nector_key_alt;
        default:
            if (key >= XK_a && key <= XK_z) return static_cast<int32_t>('A' + (key - XK_a));
            if (key >= XK_A && key <= XK_Z) return static_cast<int32_t>(key);
            if (key >= XK_0 && key <= XK_9) return static_cast<int32_t>(key);
            if (key >= XK_KP_0 && key <= XK_KP_9) return static_cast<int32_t>('0' + (key - XK_KP_0));
            return nector_key_unknown;
    }
}

unsigned long x11Channel(uint8_t value, unsigned long mask) {
    if (mask == 0) return 0;
    unsigned int shift = 0;
    while ((mask & 1ul) == 0ul) {
        mask >>= 1;
        ++shift;
    }
    const unsigned long scaled = (static_cast<unsigned long>(value) * mask + 127ul) / 255ul;
    return scaled << shift;
}

class X11Window;

class X11Backend final : public PlatformBackend {
public:
    explicit X11Backend(nector_application_handle application) : application_(application) {
        XInitThreads();
        display_ = XOpenDisplay(nullptr);
        if (display_ == nullptr) throw std::runtime_error("XOpenDisplay failed");
        root_ = DefaultRootWindow(display_);
        wm_protocols_ = XInternAtom(display_, "WM_PROTOCOLS", False);
        wm_delete_window_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        net_wm_name_ = XInternAtom(display_, "_NET_WM_NAME", False);
        utf8_string_ = XInternAtom(display_, "UTF8_STRING", False);
        wake_atom_ = XInternAtom(display_, "_NECTOR_WAKE", False);
        Bool supported = False;
        XkbSetDetectableAutoRepeat(display_, True, &supported);
    }

    ~X11Backend() override {
        if (display_ != nullptr) XCloseDisplay(display_);
    }

    nector_status createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) override;

    nector_status processEvents(bool wait) override {
        if (wait && application_->running && XPending(display_) == 0) {
            XEvent event{};
            XNextEvent(display_, &event);
            dispatchEvent(event);
        }
        while (XPending(display_) > 0) {
            XEvent event{};
            XNextEvent(display_, &event);
            dispatchEvent(event);
        }
        return nector_ok;
    }

    void wakeUp() override {
        if (display_ == nullptr || windows_.empty()) return;
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.display = display_;
        event.xclient.window = windows_.begin()->first;
        event.xclient.message_type = wake_atom_;
        event.xclient.format = 32;
        XSendEvent(display_, event.xclient.window, False, NoEventMask, &event);
        XFlush(display_);
    }

    Display* display() const { return display_; }
    Window root() const { return root_; }
    Atom wmProtocols() const { return wm_protocols_; }
    Atom wmDeleteWindow() const { return wm_delete_window_; }
    Atom netWmName() const { return net_wm_name_; }
    Atom utf8String() const { return utf8_string_; }

    void addWindow(Window identifier, X11Window* window) {
        windows_[identifier] = window;
    }

    void removeWindow(Window identifier) {
        windows_.erase(identifier);
    }

private:
    void dispatchEvent(XEvent& event) {
        if (XFilterEvent(&event, None)) return;
        const Window identifier = event.xany.window;
        auto found = windows_.find(identifier);
        if (found == windows_.end()) return;
        X11Window* native = found->second;
        WindowGuard guard(native->window());
        native->handleEvent(event);
    }

    nector_application_handle application_;
    Display* display_ = nullptr;
    Window root_ = 0;
    Atom wm_protocols_ = 0;
    Atom wm_delete_window_ = 0;
    Atom net_wm_name_ = 0;
    Atom utf8_string_ = 0;
    Atom wake_atom_ = 0;
    std::unordered_map<Window, X11Window*> windows_;
};

class X11Window final : public PlatformWindow {
public:
    X11Window(X11Backend& backend, nector_window_handle window) : backend_(backend), window_(window) {}

    ~X11Window() override {
        closeNative();
    }

    nector_window_handle window() const { return window_; }

    nector_status create() {
        Display* display = backend_.display();
        identifier_ = XCreateSimpleWindow(display, backend_.root(), 0, 0,
                                          static_cast<unsigned int>(window_->width),
                                          static_cast<unsigned int>(window_->height), 0, 0, 0);
        if (identifier_ == 0) return nectorFail(nector_backend_error, "XCreateSimpleWindow failed");
        backend_.addWindow(identifier_, this);
        XSelectInput(display, identifier_, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                                          ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask);
        Atom protocols[] = {backend_.wmDeleteWindow()};
        XSetWMProtocols(display, identifier_, protocols, 1);
        graphics_context_ = XCreateGC(display, identifier_, 0, nullptr);
        if (graphics_context_ == nullptr) return nectorFail(nector_backend_error, "XCreateGC failed");
        input_method_ = XOpenIM(display, nullptr, nullptr, nullptr);
        if (input_method_ != nullptr) {
            input_context_ = XCreateIC(input_method_, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                       XNClientWindow, identifier_, XNFocusWindow, identifier_, nullptr);
        }
        const char* title = nullptr;
        const nector_status status = nectorSceneGetTitle(window_->scene, &title);
        if (status != nector_ok) return status;
        return setTitle(title);
    }

    nector_status show() override {
        if (identifier_ == 0) return nectorFail(nector_invalid_state, "native window is closed");
        XMapWindow(backend_.display(), identifier_);
        XFlush(backend_.display());
        return nector_ok;
    }

    nector_status hide() override {
        if (identifier_ == 0) return nectorFail(nector_invalid_state, "native window is closed");
        XUnmapWindow(backend_.display(), identifier_);
        XFlush(backend_.display());
        return nector_ok;
    }

    nector_status requestClose() override {
        if (identifier_ == 0) return nectorFail(nector_invalid_state, "native window is closed");
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.display = backend_.display();
        event.xclient.window = identifier_;
        event.xclient.message_type = backend_.wmProtocols();
        event.xclient.format = 32;
        event.xclient.data.l[0] = static_cast<long>(backend_.wmDeleteWindow());
        event.xclient.data.l[1] = CurrentTime;
        if (XSendEvent(backend_.display(), identifier_, False, NoEventMask, &event) == 0) {
            return nectorFail(nector_backend_error, "XSendEvent failed");
        }
        XFlush(backend_.display());
        return nector_ok;
    }

    nector_status setTitle(const char* title) override {
        if (identifier_ == 0) return nectorFail(nector_invalid_state, "native window is closed");
        Display* display = backend_.display();
        XStoreName(display, identifier_, title);
        const auto* data = reinterpret_cast<const unsigned char*>(title);
        XChangeProperty(display, identifier_, backend_.netWmName(), backend_.utf8String(), 8,
                        PropModeReplace, data, static_cast<int>(std::strlen(title)));
        XFlush(display);
        return nector_ok;
    }

    nector_status present(const std::vector<uint8_t>& rgba, int32_t width, int32_t height) override {
        if (identifier_ == 0 || width <= 0 || height <= 0) return nector_ok;
        const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
        if (rgba.size() < pixel_count * 4) return nectorFail(nector_invalid_argument, "RGBA pixel buffer is too small");
        Display* display = backend_.display();
        const int screen = DefaultScreen(display);
        Visual* visual = DefaultVisual(display, screen);
        const unsigned int depth = static_cast<unsigned int>(DefaultDepth(display, screen));
        XImage* image = XCreateImage(display, visual, depth, ZPixmap, 0, nullptr,
                                     static_cast<unsigned int>(width), static_cast<unsigned int>(height), 32, 0);
        if (image == nullptr) return nectorFail(nector_backend_error, "XCreateImage failed");
        std::vector<uint8_t> native_pixels(static_cast<size_t>(image->bytes_per_line) * static_cast<size_t>(height), 0);
        image->data = reinterpret_cast<char*>(native_pixels.data());
        for (int32_t y = 0; y < height; ++y) {
            for (int32_t x = 0; x < width; ++x) {
                const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
                const unsigned long pixel = x11Channel(rgba[offset], visual->red_mask) |
                                            x11Channel(rgba[offset + 1], visual->green_mask) |
                                            x11Channel(rgba[offset + 2], visual->blue_mask);
                XPutPixel(image, x, y, pixel);
            }
        }
        XPutImage(display, identifier_, graphics_context_, image, 0, 0, 0, 0,
                  static_cast<unsigned int>(width), static_cast<unsigned int>(height));
        XFlush(display);
        image->data = nullptr;
        XDestroyImage(image);
        return nector_ok;
    }

    void closeNative() override {
        if (identifier_ == 0) return;
        Display* display = backend_.display();
        const Window identifier = identifier_;
        identifier_ = 0;
        pressed_keycodes_.clear();
        backend_.removeWindow(identifier);
        if (input_context_ != nullptr) {
            XDestroyIC(input_context_);
            input_context_ = nullptr;
        }
        if (input_method_ != nullptr) {
            XCloseIM(input_method_);
            input_method_ = nullptr;
        }
        if (graphics_context_ != nullptr) {
            XFreeGC(display, graphics_context_);
            graphics_context_ = nullptr;
        }
        XDestroyWindow(display, identifier);
        XFlush(display);
    }

    void handleEvent(XEvent& event) {
        switch (event.type) {
            case Expose:
                if (event.xexpose.count == 0) record(platformRenderWindow(window_));
                break;
            case ConfigureNotify:
                record(platformResizeWindow(window_, event.xconfigure.width, event.xconfigure.height));
                break;
            case ClientMessage:
                if (event.xclient.message_type == backend_.wmProtocols() && event.xclient.format == 32 &&
                    static_cast<Atom>(event.xclient.data.l[0]) == backend_.wmDeleteWindow()) {
                    platformCloseWindow(window_);
                }
                break;
            case DestroyNotify:
                identifier_ = 0;
                break;
            case FocusIn:
                if (input_context_ != nullptr) XSetICFocus(input_context_);
                break;
            case FocusOut:
                if (input_context_ != nullptr) XUnsetICFocus(input_context_);
                pressed_keycodes_.clear();
                dispatchSimple(nector_event_focus_lost);
                break;
            case KeyPress:
                keyPress(event.xkey);
                break;
            case KeyRelease:
                keyRelease(event.xkey);
                break;
            case MotionNotify:
                mouseEvent(nector_event_mouse_move, event.xmotion.x, event.xmotion.y, 0, event.xmotion.state);
                break;
            case ButtonPress:
                buttonPress(event.xbutton);
                break;
            case ButtonRelease:
                if (event.xbutton.button <= Button3) {
                    mouseEvent(nector_event_mouse_up, event.xbutton.x, event.xbutton.y,
                               mouseButton(event.xbutton.button), event.xbutton.state);
                }
                break;
            default:
                break;
        }
    }

private:
    void record(nector_status status) {
        platformRecordEventStatus(window_, status);
    }

    void dispatch(nector_input_event& event, unsigned int state, bool key_repeat = false) {
        event.modifiers = x11Modifiers(state);
        record(platformDispatchInput(window_, event, key_repeat));
    }

    void dispatchSimple(nector_input_event_type type) {
        nector_input_event event{};
        event.type = type;
        dispatch(event, 0);
    }

    static int32_t mouseButton(unsigned int button) {
        if (button == Button1) return 0;
        if (button == Button3) return 1;
        return 2;
    }

    void mouseEvent(nector_input_event_type type, int x, int y, int32_t button, unsigned int state) {
        nector_input_event event{};
        event.type = type;
        event.x = static_cast<float>(x);
        event.y = static_cast<float>(y);
        event.button = button;
        dispatch(event, state);
    }

    void buttonPress(XButtonEvent& button) {
        if (button.button >= Button4 && button.button <= 7) {
            nector_input_event event{};
            event.type = nector_event_mouse_wheel;
            event.x = static_cast<float>(button.x);
            event.y = static_cast<float>(button.y);
            if (button.button == Button4) event.dy = 1.0f;
            else if (button.button == Button5) event.dy = -1.0f;
            else if (button.button == 6) event.dx = -1.0f;
            else event.dx = 1.0f;
            dispatch(event, button.state);
            return;
        }
        mouseEvent(nector_event_mouse_down, button.x, button.y, mouseButton(button.button), button.state);
    }

    void keyPress(XKeyEvent& key) {
        const bool repeat = pressed_keycodes_.find(key.keycode) != pressed_keycodes_.end();
        pressed_keycodes_.insert(key.keycode);
        KeySym key_symbol = NoSymbol;
        std::vector<char> text_buffer(32, 0);
        int count = 0;
        Status lookup_status = 0;
        if (input_context_ != nullptr) {
            count = Xutf8LookupString(input_context_, &key, text_buffer.data(), 31, &key_symbol, &lookup_status);
            if (lookup_status == XBufferOverflow && count > 0) {
                text_buffer.resize(static_cast<size_t>(count) + 1, 0);
                count = Xutf8LookupString(input_context_, &key, text_buffer.data(), count, &key_symbol, &lookup_status);
            }
        } else {
            count = XLookupString(&key, text_buffer.data(), 31, &key_symbol, nullptr);
            lookup_status = count > 0 ? XLookupChars : XLookupKeysym;
        }
        const KeySym key_code_symbol = XkbKeycodeToKeysym(backend_.display(), key.keycode, 0, 0);
        nector_input_event event{};
        event.type = nector_event_key_down;
        event.key = x11Key(key_code_symbol);
        if (event.key != nector_key_unknown) dispatch(event, key.state, repeat);
        if (!window_->alive || count <= 0 || (lookup_status != XLookupChars && lookup_status != XLookupBoth)) return;
        if (input_context_ != nullptr) {
            record(platformDispatchText(window_, text_buffer.data(), static_cast<size_t>(count)));
        } else {
            std::string utf8;
            for (int index = 0; index < count; ++index) {
                const unsigned char value = static_cast<unsigned char>(text_buffer[static_cast<size_t>(index)]);
                if (value < 0x80u) {
                    utf8.push_back(static_cast<char>(value));
                } else {
                    utf8.push_back(static_cast<char>(0xc0u | (value >> 6)));
                    utf8.push_back(static_cast<char>(0x80u | (value & 0x3fu)));
                }
            }
            record(platformDispatchText(window_, utf8.data(), utf8.size()));
        }
    }

    void keyRelease(XKeyEvent& key) {
        pressed_keycodes_.erase(key.keycode);
        const KeySym key_symbol = XkbKeycodeToKeysym(backend_.display(), key.keycode, 0, 0);
        nector_input_event event{};
        event.type = nector_event_key_up;
        event.key = x11Key(key_symbol);
        if (event.key != nector_key_unknown) dispatch(event, key.state);
    }

    X11Backend& backend_;
    nector_window_handle window_;
    Window identifier_ = 0;
    GC graphics_context_ = nullptr;
    XIM input_method_ = nullptr;
    XIC input_context_ = nullptr;
    std::unordered_set<unsigned int> pressed_keycodes_;
};

nector_status X11Backend::createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) {
    std::unique_ptr<X11Window> native(new X11Window(*this, window));
    const nector_status status = native->create();
    if (status != nector_ok) return status;
    out_window = std::move(native);
    return nector_ok;
}

} // namespace

nector_status createPlatformBackend(nector_application_handle application, std::unique_ptr<PlatformBackend>& out_backend) {
    nector_api_try {
        std::unique_ptr<X11Backend> backend(new X11Backend(application));
        out_backend = std::move(backend);
        return nector_ok;
    } nector_api_catch
}

#endif
