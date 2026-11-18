#include "platform.h"
#include "dsl_comp.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void usage() {
    std::cerr << "usage: ndvwr <ui.dll-nector> [font-file]\n"
              << "       ndvwr --check <ui.dll-nector>\n";
}

}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--debug") {
        std::cerr << "ndvwr: --debug was removed; use nddbg for static package inspection\n";
        usage();
        return 2;
    }
    const bool check_only = argc == 3 && std::string(argv[1]) == "--check";
    if (!check_only && (argc < 2 || argc > 3 ||
        (argc >= 2 && std::string(argv[1]).compare(0, 1, "-") == 0) ||
        (argc == 3 && std::string(argv[2]).compare(0, 1, "-") == 0))) {
        usage();
        return 2;
    }
    const char* const package_path = check_only ? argv[2] : argv[1];
    try {
        nector_package_handle package = nullptr;
        check(nectorCdslLoadFile(package_path, &package));
        std::unique_ptr<nector_package, decltype(&nectorCdslDestroyPackage)> package_guard(package, nectorCdslDestroyPackage);
        if (check_only) {
            std::cout << "valid: " << package_path << '\n';
            return 0;
        }

        nector_scene_handle scene = nullptr;
        check(nectorCdslGetScene(package, &scene));
        nector_font_handle font = nullptr;
        const char* const environment_font = std::getenv("NECTOR_FONT");
        const std::filesystem::path font_path = argc == 3
            ? std::filesystem::path(argv[2])
            : environment_font != nullptr && *environment_font != '\0'
                ? std::filesystem::path(environment_font)
                : std::filesystem::path(argv[0]).parent_path() / "assets" / "deja_vu_sans.ttf";
        check(nectorLoadFont(font_path.string().c_str(), 0, &font));
        std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> font_guard(font, nectorDestroyFont);
        check(nectorSceneSetFont(scene, font));

        nector_application_handle application = nullptr;
        check(nectorCreateApplication(&application));
        std::unique_ptr<nector_application, decltype(&nectorDestroyApplication)> app_guard(application, nectorDestroyApplication);
        nector_window_handle window = nullptr;
        check(nectorCreateWindow(application, scene, &window));
        std::unique_ptr<nector_window, decltype(&nectorDestroyWindow)> window_guard(window, nectorDestroyWindow);
        check(nectorWindowShow(window));
        check(nectorRun(application));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ndvwr: " << error.what() << '\n';
        return 1;
    }
}
