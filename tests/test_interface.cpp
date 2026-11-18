#include "interface.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double first, double second) {
    return std::fabs(first - second) < 0.01;
}

nector_node_handle part(nector_interface_row_handle row, nector_interface_row_part which) {
    nector_node_handle node = nullptr;
    check(nectorInterfaceRowGetPart(row, which, &node));
    require(node != nullptr, "row part is null");
    return node;
}

nector_rect bounds(nector_node_handle node) {
    nector_rect result{};
    check(nectorSceneGetNodeBounds(node, &result));
    return result;
}

int32_t boolean(nector_node_handle node, nector_node_property property) {
    int32_t result = 0;
    check(nectorSceneGetNodeBool(node, property, &result));
    return result;
}

void mouse(nector_scene_handle scene, nector_input_event_type type, float x, float y) {
    nector_input_event event{};
    event.type = type;
    event.button = 1;
    event.x = x;
    event.y = y;
    check(nectorSceneHandleInput(scene, &event));
}

void clickAt(nector_scene_handle scene, float x, float y) {
    mouse(scene, nector_event_mouse_down, x, y);
    mouse(scene, nector_event_mouse_up, x, y);
}

void clickNode(nector_scene_handle scene, nector_node_handle node) {
    const nector_rect area = bounds(node);
    clickAt(scene, area.x + area.width * 0.5f, area.y + area.height * 0.5f);
}

void wheel(nector_scene_handle scene, nector_node_handle node, float delta) {
    const nector_rect area = bounds(node);
    nector_input_event event{};
    event.type = nector_event_mouse_wheel;
    event.x = area.x + area.width * 0.5f;
    event.y = area.y + area.height * 0.5f;
    event.dy = delta;
    check(nectorSceneHandleInput(scene, &event));
}

struct ClickCounter {
    int count = 0;
};

void countClick(const nector_ui_event* event, void* user_data) {
    if (event->type == nector_ui_click) ++static_cast<ClickCounter*>(user_data)->count;
}

double scroll(nector_interface_row_handle row) {
    double value = 0;
    check(nectorInterfaceRowGetScroll(row, &value));
    return value;
}

double maxScroll(nector_interface_row_handle row) {
    double value = 0;
    check(nectorInterfaceRowGetMaxScroll(row, &value));
    return value;
}

void setSize(nector_interface_control_handle control, double width, double height) {
    check(nectorInterfaceControlSetNumber(control, nector_property_width, width));
    check(nectorInterfaceControlSetNumber(control, nector_property_height, height));
}

void testRows() {
    nector_scene_handle scene = nullptr;
    check(nectorCreateScene("rows", 360, 240, &scene));
    std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
    nector_interface_context_handle context = nullptr;
    check(nectorInterfaceCreateContext(scene, &context));
    std::unique_ptr<nector_interface_context, decltype(&nectorInterfaceDestroyContext)> context_guard(
        context, nectorInterfaceDestroyContext);

    nector_interface_row_handle row = nullptr;
    check(nectorInterfaceCreateRow(context, nullptr, "row", &row));
    setSize(row, 240, 64);
    std::vector<nector_interface_button_handle> buttons(4, nullptr);
    const char* ids[] = {"one", "two", "three", "four"};
    for (size_t index = 0; index < buttons.size(); ++index) {
        check(nectorInterfaceCreateButton(context, row, ids[index], ids[index], &buttons[index]));
        setSize(buttons[index], 64, 32);
    }
    check(nectorInterfaceLayout(context, 360, 240));

    nector_node_handle backward = part(row, nector_interface_row_backward);
    nector_node_handle forward = part(row, nector_interface_row_forward);
    nector_node_handle viewport = part(row, nector_interface_row_viewport);
    require(boolean(backward, nector_property_visible) == 1, "horizontal backward button is hidden on overflow");
    require(boolean(forward, nector_property_visible) == 1, "horizontal forward button is hidden on overflow");
    require(boolean(backward, nector_property_enabled) == 0, "backward button starts enabled");
    require(boolean(forward, nector_property_enabled) == 1, "forward button starts disabled");
    require(maxScroll(row) > 0, "horizontal overflow has no scroll range");
    require(near(scroll(row), 0), "horizontal row does not start at zero");

    ClickCounter fourth_clicks;
    check(nectorInterfaceControlSetEventCallback(buttons[3], countClick, &fourth_clicks));
    nector_node_handle fourth_node = nullptr;
    check(nectorInterfaceControlGetNode(buttons[3], &fourth_node));
    const nector_rect clipped = bounds(fourth_node);
    clickAt(scene, clipped.x + clipped.width * 0.5f, clipped.y + clipped.height * 0.5f);
    require(fourth_clicks.count == 0 && near(scroll(row), 0), "fully clipped content was clickable");

    clickNode(scene, backward);
    require(near(scroll(row), 0), "disabled backward button changed scroll");
    clickNode(scene, forward);
    require(scroll(row) > 0, "forward button did not scroll");
    wheel(scene, viewport, -1);
    require(near(scroll(row), maxScroll(row)), "wheel did not clamp to maximum");
    require(boolean(forward, nector_property_enabled) == 0, "forward button remains enabled at maximum");
    require(boolean(backward, nector_property_enabled) == 1, "backward button is disabled away from minimum");
    wheel(scene, viewport, -1);
    require(near(scroll(row), maxScroll(row)), "wheel exceeded maximum");
    wheel(scene, viewport, 1);
    require(scroll(row) < maxScroll(row), "reverse wheel did not scroll backward");

    check(nectorInterfaceRowSetScroll(row, 1.0e100));
    require(near(scroll(row), maxScroll(row)), "large scroll was not clamped to maximum");
    check(nectorInterfaceRowSetScroll(row, -100));
    require(near(scroll(row), 0), "negative scroll was not clamped to zero");
    check(nectorInterfaceRowScrollBy(row, 20));
    require(near(scroll(row), 20), "relative scrolling is incorrect");
    check(nectorInterfaceRowScrollBy(row, -1000));
    require(near(scroll(row), 0), "relative scrolling crossed minimum");

    check(nectorInterfaceControlSetNumber(row, nector_property_layout, 1));
    setSize(row, 160, 100);
    check(nectorInterfaceLayout(context, 360, 240));
    int32_t layout = -1;
    check(nectorInterfaceRowGetLayout(row, &layout));
    require(layout == 1, "vertical layout did not persist");
    nector_node_handle first_node = nullptr;
    nector_node_handle second_node = nullptr;
    check(nectorInterfaceControlGetNode(buttons[0], &first_node));
    check(nectorInterfaceControlGetNode(buttons[1], &second_node));
    const nector_rect first = bounds(first_node);
    const nector_rect second = bounds(second_node);
    require(near(first.x, second.x) && second.y > first.y, "vertical content is not arranged vertically");
    require(maxScroll(row) > 0, "vertical overflow has no scroll range");
    check(nectorInterfaceRowScrollBy(row, 1.0e100));
    double offset_x = 0;
    double offset_y = 0;
    check(nectorSceneGetNodeNumber(viewport, nector_property_content_offset_x, &offset_x));
    check(nectorSceneGetNodeNumber(viewport, nector_property_content_offset_y, &offset_y));
    require(near(offset_x, 0) && near(offset_y, -maxScroll(row)), "vertical content offset is inconsistent");
    require(boolean(forward, nector_property_enabled) == 0, "vertical boundary is not disabled");

    require(nectorInterfaceRowSetLayout(row, 2) == nector_invalid_argument, "invalid layout was accepted");
    require(nectorInterfaceRowSetScroll(row, std::numeric_limits<double>::quiet_NaN()) == nector_invalid_argument,
            "NaN scroll was accepted");
}

void testNonOverflowAndGenericControl() {
    nector_scene_handle scene = nullptr;
    check(nectorCreateScene("non-overflow", 220, 160, &scene));
    std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
    nector_interface_context_handle context = nullptr;
    check(nectorInterfaceCreateContext(scene, &context));
    std::unique_ptr<nector_interface_context, decltype(&nectorInterfaceDestroyContext)> context_guard(
        context, nectorInterfaceDestroyContext);
    nector_interface_row_handle row = nullptr;
    check(nectorInterfaceCreateRow(context, nullptr, "small", &row));
    setSize(row, 180, 60);
    nector_interface_button_handle button = nullptr;
    check(nectorInterfaceCreateButton(context, row, "only", "Only", &button));
    setSize(button, 64, 32);
    check(nectorInterfaceLayout(context, 220, 160));
    require(near(maxScroll(row), 0) && near(scroll(row), 0), "non-overflow row has a scroll range");
    require(boolean(part(row, nector_interface_row_backward), nector_property_visible) == 0,
            "non-overflow backward button is visible");
    require(boolean(part(row, nector_interface_row_forward), nector_property_visible) == 0,
            "non-overflow forward button is visible");
    check(nectorInterfaceRowSetScroll(row, 100));
    require(near(scroll(row), 0), "non-overflow row accepted scroll");
    wheel(scene, part(row, nector_interface_row_viewport), -1);
    require(near(scroll(row), 0), "non-overflow wheel changed scroll");

    nector_interface_control_handle label = nullptr;
    check(nectorInterfaceCreateControl(context, row, nector_node_label, "label", &label));
    check(nectorInterfaceControlSetString(label, nector_property_text, "generic"));
    const char* text = nullptr;
    check(nectorInterfaceControlGetString(label, nector_property_text, &text));
    require(std::strcmp(text, "generic") == 0, "generic string property failed");
    check(nectorInterfaceControlSetNumber(label, nector_property_width, 40));
    double width = 0;
    check(nectorInterfaceControlGetNumber(label, nector_property_width, &width));
    require(near(width, 40), "generic numeric property failed");
    check(nectorInterfaceDestroyControl(label));
    nector_node_handle removed = nullptr;
    require(nectorSceneFindNode(scene, "label", &removed) == nector_not_found,
            "destroying a generic control did not remove its node");
    require(nectorInterfaceCreateContext(scene, &context) == nector_invalid_state,
            "duplicate scene context was accepted");
}

struct DestroyControlState {
    nector_interface_control_handle control = nullptr;
    nector_status status = nector_ok;
    int calls = 0;
};

void destroyControlOnClick(const nector_ui_event* event, void* user_data) {
    if (event->type != nector_ui_click) return;
    auto* state = static_cast<DestroyControlState*>(user_data);
    ++state->calls;
    state->status = nectorInterfaceDestroyControl(state->control);
}

struct DestroyContextState {
    nector_interface_context_handle context = nullptr;
    int calls = 0;
};

void destroyContextOnEvent(const nector_ui_event* event, void* user_data) {
    if (event->type != nector_ui_custom) return;
    auto* state = static_cast<DestroyContextState*>(user_data);
    ++state->calls;
    nectorInterfaceDestroyContext(state->context);
}

void testCallbackDestruction() {
    nector_scene_handle scene = nullptr;
    check(nectorCreateScene("callbacks", 200, 120, &scene));
    std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
    nector_interface_context_handle context = nullptr;
    check(nectorInterfaceCreateContext(scene, &context));
    nector_interface_button_handle button = nullptr;
    check(nectorInterfaceCreateButton(context, nullptr, "self-destroy", "Destroy", &button));
    setSize(button, 100, 40);
    check(nectorInterfaceLayout(context, 200, 120));
    DestroyControlState control_state;
    control_state.control = button;
    check(nectorInterfaceControlSetEventCallback(button, destroyControlOnClick, &control_state));
    nector_node_handle node = nullptr;
    check(nectorInterfaceControlGetNode(button, &node));
    clickNode(scene, node);
    require(control_state.calls == 1 && control_state.status == nector_ok,
            "control could not destroy itself during callback");
    nector_node_handle removed = nullptr;
    require(nectorSceneFindNode(scene, "self-destroy", &removed) == nector_not_found,
            "self-destroyed control node remains in scene");

    DestroyContextState context_state;
    context_state.context = context;
    check(nectorInterfaceSetEventCallback(context, destroyContextOnEvent, &context_state));
    check(nectorSceneEmit(scene, "destroy-context", nullptr, 0));
    require(context_state.calls == 1, "context destruction callback was not delivered");
    check(nectorSceneEmit(scene, "after-destroy", nullptr, 0));
    check(nectorInterfaceCreateContext(scene, &context));
    nectorInterfaceDestroyContext(context);
}

struct Pixel {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

Pixel pixelAt(const std::vector<uint8_t>& pixels, int width, int x, int y) {
    const size_t offset = (static_cast<size_t>(y * width + x)) * 4;
    return {pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]};
}

void testGlassPixels() {
    nector_scene_handle scene = nullptr;
    check(nectorCreateScene("glass", 220, 100, &scene));
    std::unique_ptr<nector_scene, decltype(&nectorDestroyScene)> scene_guard(scene, nectorDestroyScene);
    nector_node_handle root = nullptr;
    check(nectorSceneGetRoot(scene, &root));
    check(nectorSceneSetNodeColor(root, nector_property_background, {0, 0, 0, 0}));
    nector_interface_context_handle context = nullptr;
    check(nectorInterfaceCreateContext(scene, &context));
    std::unique_ptr<nector_interface_context, decltype(&nectorInterfaceDestroyContext)> context_guard(
        context, nectorInterfaceDestroyContext);
    nector_interface_button_handle button = nullptr;
    check(nectorInterfaceCreateButton(context, nullptr, "glass", "", &button));
    setSize(button, 180, 60);
    nector_node_handle node = nullptr;
    check(nectorInterfaceControlGetNode(button, &node));
    require(boolean(node, nector_property_glass) == 1, "interface button did not enable glass");
    check(nectorInterfaceLayout(context, 220, 100));

    nector_surface_handle surface = nullptr;
    check(nectorCreateSurface(220, 100, &surface));
    std::unique_ptr<nector_surface, decltype(&nectorDestroySurface)> surface_guard(surface, nectorDestroySurface);
    nector_painter_handle painter = nullptr;
    check(nectorCreatePainter(surface, &painter));
    std::unique_ptr<nector_painter, decltype(&nectorDestroyPainter)> painter_guard(painter, nectorDestroyPainter);
    std::vector<uint8_t> pixels(220 * 100 * 4);

    check(nectorSurfaceClear(surface, {0.08f, 0.12f, 0.20f, 1}));
    check(nectorSceneDraw(scene, painter));
    check(nectorSurfaceReadPixels(surface, pixels.data(), pixels.size()));
    const Pixel dark_top = pixelAt(pixels, 220, 90, 6);
    const Pixel dark_bottom = pixelAt(pixels, 220, 90, 52);
    require(dark_top.r > dark_bottom.r + 8 && dark_top.g > dark_bottom.g + 8,
            "glass gradient or top highlight is missing");

    check(nectorSurfaceClear(surface, {0.80f, 0.10f, 0.10f, 1}));
    check(nectorSceneDraw(scene, painter));
    check(nectorSurfaceReadPixels(surface, pixels.data(), pixels.size()));
    const Pixel light_top = pixelAt(pixels, 220, 90, 6);
    require(std::abs(static_cast<int>(light_top.r) - static_cast<int>(dark_top.r)) > 30,
            "glass is opaque instead of semi-transparent");

    ClickCounter clicks;
    check(nectorInterfaceControlSetEventCallback(button, countClick, &clicks));
    check(nectorInterfaceControlSetBool(button, nector_property_enabled, 0));
    clickNode(scene, node);
    require(clicks.count == 0, "disabled glass button emitted a click");
    check(nectorInterfaceControlSetBool(button, nector_property_enabled, 1));
    clickNode(scene, node);
    require(clicks.count == 1, "enabled glass button did not emit a click");
    mouse(scene, nector_event_mouse_down, 90, 30);
    check(nectorSurfaceClear(surface, {0.08f, 0.12f, 0.20f, 1}));
    check(nectorSceneDraw(scene, painter));
    mouse(scene, nector_event_mouse_up, 90, 30);
}

} // namespace

int main() {
    try {
        testRows();
        testNonOverflowAndGenericControl();
        testCallbackDestruction();
        testGlassPixels();
        std::cout << "test_interface: PASS; horizontal+vertical, overflow controls, wheel, clamp, clip, generic, glass pixels\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_interface: FAIL: " << error.what() << '\n';
        return 1;
    }
}
