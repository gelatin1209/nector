#include "nector.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {

static_assert(!std::is_copy_constructible<::NInterface>::value, "NInterface must be move-only");
static_assert(std::is_move_constructible<::NInterface>::value, "NInterface must be movable");
static_assert(!std::is_copy_constructible<::NInterface::Row>::value, "Row handles must be unique");
static_assert(!std::is_copy_constructible<::NInterface::Button>::value, "Button handles must be unique");

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void click(nector::Scene& scene, const nector::Node& node) {
    const nector::Rect area = node.bounds();
    nector::InputEvent event;
    event.type = nector::InputEventType::MouseDown;
    event.button = 1;
    event.x = area.x + area.width * 0.5F;
    event.y = area.y + area.height * 0.5F;
    scene.handleInput(event);
    event.type = nector::InputEventType::MouseUp;
    scene.handleInput(event);
}

}

int main() {
    try {
        nector::Scene scene("cpp-interface", 360, 240);
        ::NInterface interface(scene);
        ::NInterface::Row row(interface, "row");
        row.setNumber(nector::NodeProperty::Width, 240);
        row.setNumber(nector::NodeProperty::Height, 100);

        std::array<std::unique_ptr<::NInterface::Button>, 4> buttons;
        for (std::size_t index = 0; index < buttons.size(); ++index) {
            const std::string id = "button-" + std::to_string(index);
            buttons[index] = std::make_unique<::NInterface::Button>(interface, row, id, id);
            buttons[index]->setNumber(nector::NodeProperty::Width, 64);
            buttons[index]->setNumber(nector::NodeProperty::Height, 32);
        }
        interface.layout(360, 240);
        require(static_cast<std::int32_t>(row.layout) == 0 && row.maxScroll() > 0,
                "C++ horizontal Row did not overflow");
        row.layout = 1;
        require(static_cast<std::int32_t>(row.layout) == 1, "C++ Row.layout assignment did not persist");
        row.setScroll(1.0e100);
        require(std::fabs(row.scroll() - row.maxScroll()) < 0.001, "C++ Row did not clamp to maximum");
        row.scrollBy(-1.0e100);
        require(row.scroll() == 0, "C++ Row did not clamp to zero");
        require(row.part(::NInterface::Row::Part::Forward).boolean(nector::NodeProperty::Visible),
                "C++ Row overflow button is hidden");

        buttons[0]->glass = false;
        require(!static_cast<bool>(buttons[0]->glass), "C++ Button glass disable failed");
        buttons[0]->glass = true;
        require(static_cast<bool>(buttons[0]->glass), "C++ Button glass enable failed");
        buttons[0]->setString(nector::NodeProperty::Text, "Changed");
        require(buttons[0]->string(nector::NodeProperty::Text) == "Changed",
                "C++ control string property failed");

        int clicks = 0;
        buttons[0]->setEventCallback([&clicks](const nector::UiEvent& event) {
            if (event.type == nector::UiEventType::Click) ++clicks;
        });
        interface.refresh();
        click(scene, buttons[0]->node());
        require(clicks == 1, "C++ Button callback did not receive a real click");

        ::NInterface::Control label(interface, row, nector::NodeType::Label, "label");
        label.setString(nector::NodeProperty::Text, "generic");
        require(label.string(nector::NodeProperty::Text) == "generic", "C++ generic control failed");
        label.destroy();
        require(!label, "destroyed C++ control retained a handle");
        bool missing = false;
        try {
            (void)scene.findNode("label");
        } catch (const nector::Error& error) {
            missing = error.status() == nector::Status::NotFound;
        }
        require(missing, "destroying a C++ control left its scene node alive");

        bool rejected = false;
        try {
            row.layout = 2;
        } catch (const nector::Error& error) {
            rejected = error.status() == nector::Status::InvalidArgument;
        }
        require(rejected, "C++ Row accepted an invalid layout");

        std::cout << "test_interface_cpp: PASS; Row.layout, scrolling, parts, Button.glass, callbacks and RAII\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_interface_cpp: FAIL: " << error.what() << '\n';
        return 1;
    }
}
