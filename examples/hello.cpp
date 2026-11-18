#include "platform.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

struct UiState {
    nector_node_handle status = nullptr;
    nector_node_handle progress = nullptr;
};

void onEvent(const nector_ui_event* event, void* user_data) {
    auto* state = static_cast<UiState*>(user_data);
    if (event->type == nector_ui_click && std::strcmp(event->node_id, "run") == 0) {
        nectorSceneSetNodeString(state->status, nector_property_text, "Button clicked");
        nectorSceneSetNodeNumber(state->progress, nector_property_value, 100);
    } else if (event->type == nector_ui_value_changed && std::strcmp(event->node_id, "volume") == 0) {
        nectorSceneSetNodeNumber(state->progress, nector_property_value, event->value);
    } else if (event->type == nector_ui_paint && std::strcmp(event->node_id, "canvas") == 0) {
        nectorPainterSetFillColor(event->painter, {0.12f, 0.16f, 0.24f, 1.0f});
        nectorPainterFillRectangle(event->painter, event->bounds);
        nectorPainterSetFillColor(event->painter, {0.25f, 0.8f, 0.65f, 1.0f});
        nectorPainterFillEllipse(event->painter, event->bounds.x + event->bounds.width * 0.5f,
                                 event->bounds.y + event->bounds.height * 0.5f, 42, 42);
    }
}
}

int main(int argc, char** argv) {
    try {
        const char* const environment_font = std::getenv("NECTOR_FONT");
        const std::filesystem::path font_path = argc > 1
            ? std::filesystem::path(argv[1])
            : environment_font != nullptr && *environment_font != '\0'
                ? std::filesystem::path(environment_font)
                : std::filesystem::path(argv[0]).parent_path() / "assets" / "deja_vu_sans.ttf";
        nector_application_handle application = nullptr;
        nector_scene_handle scene = nullptr;
        nector_font_handle font = nullptr;
        nector_window_handle window = nullptr;
        check(nectorCreateApplication(&application));
        std::unique_ptr<nector_application, decltype(&nectorDestroyApplication)> app_guard(application, nectorDestroyApplication);
        check(nectorCreateScene("Nector code UI", 720, 560, &scene));
        std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
        check(nectorLoadFont(font_path.string().c_str(), 0, &font));
        std::unique_ptr<nector_font, decltype(&nectorDestroyFont)> font_guard(font, nectorDestroyFont);
        check(nectorSceneSetFont(scene, font));
        nector_node_handle root = nullptr;
        check(nectorSceneGetRoot(scene, &root));
        check(nectorSceneSetNodeNumber(root, nector_property_padding, 20));
        check(nectorSceneSetNodeNumber(root, nector_property_gap, 12));
        UiState state;
        nector_node_handle node = nullptr;
        check(nectorSceneAddNode(scene, root, nector_node_label, "status", &state.status));
        check(nectorSceneSetNodeString(state.status, nector_property_text, "Nector is ready"));
        check(nectorSceneSetNodeNumber(state.status, nector_property_font_size, 24));
        check(nectorSceneAddNode(scene, root, nector_node_button, "run", &node));
        check(nectorSceneSetNodeString(node, nector_property_text, "Run action"));
        check(nectorSceneSetNodeNumber(node, nector_property_height, 44));
        check(nectorSceneAddNode(scene, root, nector_node_line_edit, "editor", &node));
        check(nectorSceneSetNodeString(node, nector_property_placeholder, "Type here"));
        check(nectorSceneAddNode(scene, root, nector_node_check_box, "check", &node));
        check(nectorSceneSetNodeString(node, nector_property_text, "Enabled"));
        check(nectorSceneAddNode(scene, root, nector_node_slider, "volume", &node));
        check(nectorSceneSetNodeNumber(node, nector_property_maximum, 100));
        check(nectorSceneSetNodeNumber(node, nector_property_value, 35));
        check(nectorSceneAddNode(scene, root, nector_node_progress_bar, "progress", &state.progress));
        check(nectorSceneSetNodeNumber(state.progress, nector_property_maximum, 100));
        check(nectorSceneSetNodeNumber(state.progress, nector_property_value, 35));
        check(nectorSceneAddNode(scene, root, nector_node_canvas, "canvas", &node));
        check(nectorSceneSetNodeNumber(node, nector_property_flex, 1));
        check(nectorSceneSetEventCallback(scene, onEvent, &state));
        check(nectorCreateWindow(application, scene, &window));
        std::unique_ptr<nector_window, decltype(&nectorDestroyWindow)> window_guard(window, nectorDestroyWindow);
        check(nectorWindowShow(window));
        check(nectorRun(application));
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "hello: " << error.what() << '\n';
        return 1;
    }
}
