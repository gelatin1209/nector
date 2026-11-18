#include "scene.h"

#include <cmath>
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
struct Events {
    int clicks = 0;
    int text_changes = 0;
    int animation_finished = 0;
};
void onEvent(const nector_ui_event* event, void* user_data) {
    auto* events = static_cast<Events*>(user_data);
    if (event->type == nector_ui_click) ++events->clicks;
    else if (event->type == nector_ui_text_changed) ++events->text_changes;
    else if (event->type == nector_ui_animation_finished) ++events->animation_finished;
}
void click(nector_scene_handle scene, nector_node_handle node) {
    nector_rect bounds{};
    check(nectorSceneGetNodeBounds(node, &bounds));
    nector_input_event event{};
    event.type = nector_event_mouse_down;
    event.button = 1;
    event.x = bounds.x + bounds.width * 0.5f;
    event.y = bounds.y + bounds.height * 0.5f;
    check(nectorSceneHandleInput(scene, &event));
    event.type = nector_event_mouse_up;
    check(nectorSceneHandleInput(scene, &event));
}
}

int main() {
    try {
        nector_scene_handle scene = nullptr;
        check(nectorCreateScene("scene test", 400, 500, &scene));
        std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
        Events events;
        check(nectorSceneSetEventCallback(scene, onEvent, &events));
        nector_node_handle root = nullptr;
        check(nectorSceneGetRoot(scene, &root));
        nector_node_handle button = nullptr;
        check(nectorSceneAddNode(scene, root, nector_node_button, "button", &button));
        check(nectorSceneSetNodeString(button, nector_property_text, "Button"));
        check(nectorSceneSetNodeNumber(button, nector_property_height, 40));
        nector_node_handle editor = nullptr;
        check(nectorSceneAddNode(scene, root, nector_node_line_edit, "editor", &editor));
        check(nectorSceneSetNodeString(editor, nector_property_text, "A"));
        check(nectorSceneSetNodeNumber(editor, nector_property_height, 40));
        nector_node_handle slider = nullptr;
        check(nectorSceneAddNode(scene, root, nector_node_slider, "slider", &slider));
        check(nectorSceneSetNodeNumber(slider, nector_property_minimum, 0));
        check(nectorSceneSetNodeNumber(slider, nector_property_maximum, 100));
        check(nectorSceneSetNodeNumber(slider, nector_property_step, 1));
        check(nectorSceneSetNodeNumber(slider, nector_property_height, 40));
        check(nectorSceneLayout(scene, 400, 500));
        click(scene, button);
        require(events.clicks == 1, "button click was not delivered");
        click(scene, editor);
        nector_input_event input{};
        input.type = nector_event_text_input;
        std::strcpy(input.text, "\xe6\xb1\x89");
        check(nectorSceneHandleInput(scene, &input));
        const char* text = nullptr;
        check(nectorSceneGetNodeString(editor, nector_property_text, &text));
        require(std::strcmp(text, "A\xe6\xb1\x89") == 0 && events.text_changes == 1, "UTF-8 text input failed");
        input = {};
        input.type = nector_event_key_down;
        input.key = nector_key_backspace;
        check(nectorSceneHandleInput(scene, &input));
        check(nectorSceneGetNodeString(editor, nector_property_text, &text));
        require(std::strcmp(text, "A") == 0, "UTF-8 backspace failed");

        nector_animation_handle animation = nullptr;
        check(nectorCreateAnimation(100, &animation));
        std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)> animation_guard(animation, nectorDestroyAnimation);
        check(nectorAnimationAddKey(animation, 3, 0, 0, nector_ease_linear));
        check(nectorAnimationAddKey(animation, 3, 100, 100, nector_ease_linear));
        check(nectorSceneBindAnimation(scene, animation, 3, slider, nector_property_value, 9));
        check(nectorAnimationStart(animation));
        check(nectorSceneTick(scene, 50));
        double value = 0;
        check(nectorSceneGetNodeNumber(slider, nector_property_value, &value));
        require(std::fabs(value - 50) < 0.01, "scene animation midpoint is wrong");
        check(nectorSceneTick(scene, 50));
        require(events.animation_finished == 1, "scene animation completion event is missing");
        check(nectorSceneRemoveNode(scene, button));
        require(nectorSceneFindNode(scene, "button", &button) == nector_not_found, "removed node is still searchable");
        std::cout << "test_scene: PASS; click=1; UTF-8 'A+汉-backspace'=A; animation(50ms)=50; finished=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_scene: FAIL: " << error.what() << '\n';
        return 1;
    }
}
