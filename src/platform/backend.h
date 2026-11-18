#ifndef nector_platform_backend_h
#define nector_platform_backend_h

#include "platform.h"
#include "../core/internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class PlatformWindow {
public:
    virtual ~PlatformWindow() = default;
    virtual nector_status show() = 0;
    virtual nector_status hide() = 0;
    virtual nector_status requestClose() = 0;
    virtual nector_status setTitle(const char* title) = 0;
    virtual nector_status present(const std::vector<uint8_t>& rgba, int32_t width, int32_t height) = 0;
    virtual void closeNative() = 0;
};

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;
    virtual nector_status createWindow(nector_window_handle window, std::unique_ptr<PlatformWindow>& out_window) = 0;
    virtual nector_status processEvents(bool wait) = 0;
    virtual void wakeUp() = 0;
};

struct nector_application {
    std::unique_ptr<PlatformBackend> backend;
    std::vector<nector_window_handle> windows;
    size_t references = 1;
    size_t operation_depth = 0;
    bool alive = true;
    bool running = true;
    bool deleting = false;
    nector_status event_status = nector_ok;
};

struct nector_window {
    ~nector_window();

    nector_application_handle application = nullptr;
    nector_scene_handle scene = nullptr;
    nector_input_handle input = nullptr;
    nector_surface_handle surface = nullptr;
    nector_painter_handle painter = nullptr;
    std::unique_ptr<PlatformWindow> native;
    std::vector<uint8_t> pixels;
    int32_t width = 0;
    int32_t height = 0;
    size_t references = 1;
    bool alive = true;
    bool closing = false;
    bool deleting = false;
};

class WindowGuard {
public:
    explicit WindowGuard(nector_window_handle window);
    ~WindowGuard();
    WindowGuard(const WindowGuard&) = delete;
    WindowGuard& operator=(const WindowGuard&) = delete;
private:
    nector_window_handle window_;
    bool retained_ = false;
};

class ApplicationGuard {
public:
    explicit ApplicationGuard(nector_application_handle application);
    ~ApplicationGuard();
    ApplicationGuard(const ApplicationGuard&) = delete;
    ApplicationGuard& operator=(const ApplicationGuard&) = delete;
private:
    nector_application_handle application_;
};

nector_status createPlatformBackend(nector_application_handle application, std::unique_ptr<PlatformBackend>& out_backend);
extern "C" nector_input_api nector_status nectorInputPushPlatform(
    nector_input_handle input, const nector_input_event* event, int32_t key_repeat);
nector_status platformDispatchInput(nector_window_handle window, const nector_input_event& event, bool key_repeat = false);
nector_status platformDispatchText(nector_window_handle window, const char* utf8_text, size_t length);
nector_status platformResizeWindow(nector_window_handle window, int32_t width, int32_t height);
nector_status platformRenderWindow(nector_window_handle window);
void platformCloseWindow(nector_window_handle window);
void platformRecordEventStatus(nector_window_handle window, nector_status status);

constexpr uint32_t platform_shift_modifier = 1u;
constexpr uint32_t platform_control_modifier = 2u;
constexpr uint32_t platform_alt_modifier = 4u;

#endif
