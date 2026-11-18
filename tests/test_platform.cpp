#include "platform.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        nector_application_handle application = nullptr;
        check(nectorCreateApplication(&application));
        std::unique_ptr<nector_application, decltype(&nectorDestroyApplication)> application_guard(
            application, nectorDestroyApplication);
        nector_scene_handle scene = nullptr;
        check(nectorCreateScene("platform test", 160, 120, &scene));
        std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
        nector_window_handle window = nullptr;
        check(nectorCreateWindow(application, scene, &window));
        std::unique_ptr<nector_window, decltype(&nectorDestroyWindow)> window_guard(window, nectorDestroyWindow);

        int32_t width = 0;
        int32_t height = 0;
        check(nectorWindowGetSize(window, &width, &height));
        require(width == 160 && height == 120, "native window size mismatch");
        nector_scene_handle borrowed_scene = nullptr;
        check(nectorWindowGetScene(window, &borrowed_scene));
        require(borrowed_scene == scene, "window returned the wrong scene");
        nector_input_handle input = nullptr;
        check(nectorWindowGetInput(window, &input));
        require(input != nullptr, "window input driver is missing");
        check(nectorWindowSetTitle(window, "Nector \xe7\xaa\x97\xe5\x8f\xa3"));
        check(nectorWindowShow(window));
        check(nectorWindowRender(window));
        check(nectorWindowHide(window));
        check(nectorProcessEvents(application));
        check(nectorWindowRequestClose(window));
        check(nectorProcessEvents(application));
        int32_t running = 1;
        check(nectorApplicationIsRunning(application, &running));
        require(running == 0, "window close did not stop the application");
        require(std::strcmp(nectorPlatformName(), "win32") == 0, "wrong Windows backend name");
        check(nectorQuit(application));
        std::cout << "test_platform: PASS; Win32 create/show/render/hide/input/close/quit\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_platform: FAIL: " << error.what() << '\n';
        return 1;
    }
}
