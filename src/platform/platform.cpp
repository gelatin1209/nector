#include "backend.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <utility>

namespace {

void retainApplication(nector_application_handle application) {
    ++application->references;
}

void releaseApplication(nector_application_handle application) {
    if (application == nullptr || application->references == 0) return;
    --application->references;
    if (application->references == 0 && application->operation_depth == 0 && !application->deleting) {
        application->deleting = true;
        delete application;
    }
}

void releaseWindow(nector_window_handle window) {
    if (window == nullptr || window->references == 0) return;
    --window->references;
    if (window->references == 0 && !window->deleting) {
        window->deleting = true;
        delete window;
    }
}

nector_status createSurfaceAndPainter(nector_window_handle window, int32_t width, int32_t height) {
    nector_surface_handle surface = nullptr;
    nector_status status = nectorCreateSurface(width, height, &surface);
    if (status != nector_ok) return status;
    nector_painter_handle painter = nullptr;
    status = nectorCreatePainter(surface, &painter);
    if (status != nector_ok) {
        nectorDestroySurface(surface);
        return status;
    }
    nectorDestroyPainter(window->painter);
    nectorDestroySurface(window->surface);
    window->surface = surface;
    window->painter = painter;
    window->width = width;
    window->height = height;
    return nector_ok;
}

nector_status processApplicationEvents(nector_application_handle application, bool wait) {
    if (application->backend == nullptr) return nectorFail(nector_invalid_state, "application backend is unavailable");
    const nector_status native_status = application->backend->processEvents(wait);
    const nector_status event_status = application->event_status;
    application->event_status = nector_ok;
    return event_status != nector_ok ? event_status : native_status;
}

} // namespace

#if !defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__)
nector_status createPlatformBackend(nector_application_handle, std::unique_ptr<PlatformBackend>&) {
    return nectorFail(nector_unsupported, "no native platform backend is available");
}
#endif

nector_window::~nector_window() {
    alive = false;
    native.reset();
    nectorDestroyPainter(painter);
    nectorDestroySurface(surface);
    nectorDestroyInput(input);
    painter = nullptr;
    surface = nullptr;
    input = nullptr;
    if (application != nullptr) {
        auto& windows = application->windows;
        windows.erase(std::remove(windows.begin(), windows.end(), this), windows.end());
        nector_application_handle owner = application;
        application = nullptr;
        releaseApplication(owner);
    }
}

WindowGuard::WindowGuard(nector_window_handle window) : window_(window) {
    if (window_ != nullptr && !window_->deleting) {
        ++window_->references;
        retained_ = true;
    }
}

WindowGuard::~WindowGuard() {
    if (retained_) releaseWindow(window_);
}

ApplicationGuard::ApplicationGuard(nector_application_handle application) : application_(application) {
    if (application_ != nullptr) ++application_->operation_depth;
}

ApplicationGuard::~ApplicationGuard() {
    if (application_ == nullptr || application_->operation_depth == 0) return;
    --application_->operation_depth;
    if (application_->references == 0 && application_->operation_depth == 0 && !application_->deleting) {
        application_->deleting = true;
        delete application_;
    }
}

nector_status platformDispatchInput(nector_window_handle window, const nector_input_event& event, bool key_repeat) {
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive) return nectorFail(nector_invalid_state, "window is being destroyed");
    const nector_status scene_status = nectorSceneHandleInput(window->scene, &event);
    const nector_status input_status = nectorInputPushPlatform(window->input, &event, key_repeat ? 1 : 0);
    return scene_status != nector_ok ? scene_status : input_status;
}

nector_status platformDispatchText(nector_window_handle window, const char* utf8_text, size_t length) {
    if (window == nullptr || utf8_text == nullptr) return nectorFail(nector_invalid_argument, "window or text is null");
    WindowGuard guard(window);
    if (!window->alive) return nectorFail(nector_invalid_state, "window is being destroyed");
    const nector_status validation = nectorCoreValidateUtf8(utf8_text, length);
    if (validation != nector_ok) return validation;
    size_t offset = 0;
    while (offset < length) {
        size_t chunk_end = std::min(length, offset + 31);
        while (chunk_end > offset && chunk_end < length &&
               (static_cast<unsigned char>(utf8_text[chunk_end]) & 0xc0u) == 0x80u) {
            --chunk_end;
        }
        if (chunk_end == offset) return nectorFail(nector_invalid_format, "UTF-8 text chunk is invalid");
        nector_input_event event{};
        event.type = nector_event_text_input;
        std::memcpy(event.text, utf8_text + offset, chunk_end - offset);
        event.text[chunk_end - offset] = '\0';
        const nector_status status = platformDispatchInput(window, event);
        if (status != nector_ok) return status;
        offset = chunk_end;
    }
    return nector_ok;
}

nector_status platformResizeWindow(nector_window_handle window, int32_t width, int32_t height) {
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive) return nectorFail(nector_invalid_state, "window is being destroyed");
    if (width <= 0 || height <= 0 || (window->width == width && window->height == height)) return nector_ok;
    const nector_status surface_status = createSurfaceAndPainter(window, width, height);
    if (surface_status != nector_ok) return surface_status;
    nector_input_event event{};
    event.type = nector_event_window_resize;
    event.width = width;
    event.height = height;
    return platformDispatchInput(window, event);
}

nector_status platformRenderWindow(nector_window_handle window) {
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive || window->native == nullptr || window->surface == nullptr || window->painter == nullptr) {
        return nectorFail(nector_invalid_state, "window is not renderable");
    }
    if (window->width <= 0 || window->height <= 0) return nector_ok;
    nector_status status = nectorSurfaceClear(window->surface, {0, 0, 0, 0});
    if (status != nector_ok) return status;
    status = nectorSceneDraw(window->scene, window->painter);
    if (status != nector_ok || !window->alive) return status;
    const size_t pixel_size = static_cast<size_t>(window->width) * static_cast<size_t>(window->height) * 4;
    window->pixels.resize(pixel_size);
    status = nectorSurfaceReadPixels(window->surface, window->pixels.data(), window->pixels.size());
    if (status != nector_ok) return status;
    return window->native->present(window->pixels, window->width, window->height);
}

void platformRecordEventStatus(nector_window_handle window, nector_status status) {
    if (window != nullptr && window->application != nullptr && window->application->event_status == nector_ok) {
        window->application->event_status = status;
    }
}

void platformCloseWindow(nector_window_handle window) {
    if (window == nullptr || window->closing || window->deleting) return;
    WindowGuard guard(window);
    if (!window->alive) return;
    window->closing = true;
    nector_input_event event{};
    event.type = nector_event_window_close;
    platformRecordEventStatus(window, platformDispatchInput(window, event));
    if (window->application != nullptr) window->application->running = false;
    if (window->alive && window->native != nullptr) window->native->closeNative();
}

nector_status nectorCreateApplication(nector_application_handle* out_application) {
    if (out_application == nullptr) return nectorFail(nector_invalid_argument, "out_application is null");
    *out_application = nullptr;
    nector_api_try {
        std::unique_ptr<nector_application> application(new nector_application);
        const nector_status status = createPlatformBackend(application.get(), application->backend);
        if (status != nector_ok) return status;
        *out_application = application.release();
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyApplication(nector_application_handle application) {
    if (application == nullptr || application->deleting) return;
    ApplicationGuard guard(application);
    if (application->alive) {
        application->alive = false;
        application->running = false;
        size_t index = 0;
        while (index < application->windows.size()) {
            nector_window_handle window = application->windows[index];
            nectorDestroyWindow(window);
            if (index < application->windows.size() && application->windows[index] == window) ++index;
        }
        if (application->backend != nullptr) application->backend->wakeUp();
    }
    releaseApplication(application);
}

nector_status nectorCreateWindow(nector_application_handle application, nector_scene_handle scene,
                                 nector_window_handle* out_window) {
    if (out_window == nullptr) return nectorFail(nector_invalid_argument, "out_window is null");
    *out_window = nullptr;
    if (application == nullptr || scene == nullptr) return nectorFail(nector_invalid_argument, "application or scene is null");
    if (!application->alive || application->backend == nullptr) {
        return nectorFail(nector_invalid_state, "application is not active");
    }
    ApplicationGuard application_guard(application);
    nector_api_try {
        int32_t width = 0;
        int32_t height = 0;
        nector_status status = nectorSceneGetSize(scene, &width, &height);
        if (status != nector_ok) return status;
        std::unique_ptr<nector_window> window(new nector_window);
        window->application = application;
        window->scene = scene;
        retainApplication(application);
        status = nectorCreateInput(&window->input);
        if (status != nector_ok) return status;
        status = createSurfaceAndPainter(window.get(), width, height);
        if (status != nector_ok) return status;
        application->windows.push_back(window.get());
        status = application->backend->createWindow(window.get(), window->native);
        if (status != nector_ok) return status;
        *out_window = window.release();
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyWindow(nector_window_handle window) {
    if (window == nullptr || window->deleting || !window->alive) return;
    window->alive = false;
    releaseWindow(window);
}

nector_status nectorWindowShow(nector_window_handle window) {
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive || window->native == nullptr) return nectorFail(nector_invalid_state, "window is not active");
    return window->native->show();
}

nector_status nectorWindowHide(nector_window_handle window) {
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive || window->native == nullptr) return nectorFail(nector_invalid_state, "window is not active");
    return window->native->hide();
}

nector_status nectorWindowRequestClose(nector_window_handle window) {
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive || window->native == nullptr) return nectorFail(nector_invalid_state, "window is not active");
    return window->native->requestClose();
}

nector_status nectorWindowGetInput(nector_window_handle window, nector_input_handle* out_input) {
    if (out_input == nullptr) return nectorFail(nector_invalid_argument, "out_input is null");
    *out_input = nullptr;
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive) return nectorFail(nector_invalid_state, "window is not active");
    *out_input = window->input;
    return nector_ok;
}

nector_status nectorWindowGetScene(nector_window_handle window, nector_scene_handle* out_scene) {
    if (out_scene == nullptr) return nectorFail(nector_invalid_argument, "out_scene is null");
    *out_scene = nullptr;
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive) return nectorFail(nector_invalid_state, "window is not active");
    *out_scene = window->scene;
    return nector_ok;
}

nector_status nectorWindowGetSize(nector_window_handle window, int32_t* out_width, int32_t* out_height) {
    if (out_width == nullptr || out_height == nullptr) return nectorFail(nector_invalid_argument, "size output is null");
    *out_width = 0;
    *out_height = 0;
    if (window == nullptr) return nectorFail(nector_invalid_argument, "window is null");
    WindowGuard guard(window);
    if (!window->alive) return nectorFail(nector_invalid_state, "window is not active");
    *out_width = window->width;
    *out_height = window->height;
    return nector_ok;
}

nector_status nectorWindowSetTitle(nector_window_handle window, const char* title) {
    if (window == nullptr || title == nullptr) return nectorFail(nector_invalid_argument, "window or title is null");
    const nector_status validation = nectorCoreValidateUtf8(title, std::strlen(title));
    if (validation != nector_ok) return validation;
    WindowGuard guard(window);
    if (!window->alive || window->native == nullptr) return nectorFail(nector_invalid_state, "window is not active");
    nector_api_try {
        return window->native->setTitle(title);
    } nector_api_catch
}

nector_status nectorWindowRender(nector_window_handle window) {
    nector_api_try {
        return platformRenderWindow(window);
    } nector_api_catch
}

nector_status nectorProcessEvents(nector_application_handle application) {
    if (application == nullptr) return nectorFail(nector_invalid_argument, "application is null");
    ApplicationGuard guard(application);
    if (!application->alive) return nectorFail(nector_invalid_state, "application is not active");
    nector_api_try {
        return processApplicationEvents(application, false);
    } nector_api_catch
}

nector_status nectorRun(nector_application_handle application) {
    if (application == nullptr) return nectorFail(nector_invalid_argument, "application is null");
    ApplicationGuard guard(application);
    if (!application->alive) return nectorFail(nector_invalid_state, "application is not active");
    while (application->alive && application->running) {
        nector_status status = nector_ok;
        try {
            status = processApplicationEvents(application, true);
        } catch (const std::bad_alloc&) {
            return nectorFail(nector_out_of_memory, "out of memory");
        } catch (const std::exception& exception) {
            return nectorFail(nector_backend_error, exception.what());
        } catch (...) {
            return nectorFail(nector_backend_error, "unknown native error");
        }
        if (status != nector_ok) return status;
    }
    return nector_ok;
}

nector_status nectorQuit(nector_application_handle application) {
    if (application == nullptr) return nectorFail(nector_invalid_argument, "application is null");
    ApplicationGuard guard(application);
    application->running = false;
    if (application->backend != nullptr) application->backend->wakeUp();
    return nector_ok;
}

nector_status nectorApplicationIsRunning(nector_application_handle application, int32_t* out_running) {
    if (out_running == nullptr) return nectorFail(nector_invalid_argument, "out_running is null");
    *out_running = 0;
    if (application == nullptr) return nectorFail(nector_invalid_argument, "application is null");
    ApplicationGuard guard(application);
    *out_running = application->alive && application->running ? 1 : 0;
    return nector_ok;
}

const char* nectorPlatformName(void) {
#if defined(_WIN32)
    return "win32";
#elif defined(__linux__)
    return "x11";
#elif defined(__APPLE__)
    return "cocoa";
#else
    return "unsupported";
#endif
}
