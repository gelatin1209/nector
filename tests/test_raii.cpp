#include "nector.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
    try {
        require(!nector::getVersion().empty(), "version is empty");
        nector::Surface surface(32, 24);
        surface.clear({0.1f, 0.2f, 0.3f, 1.0f});
        nector::Surface moved_surface = std::move(surface);
        require(!surface && moved_surface.size().width == 32, "surface move failed");
        nector::Painter painter(moved_surface);
        painter.setFillColor({1, 0, 0, 1});
        nector::Path path;
        path.addRectangle({2, 2, 12, 10});
        painter.fillPath(path, nector::FillRule::Nonzero);
        require(moved_surface.readPixels().size() == 32U * 24U * 4U, "surface readback failed");

        const std::vector<std::uint8_t> rgba(2U * 3U * 4U, 255);
        nector::NImage image = nector::NImage::create(2, 3, rgba);
        require(image.size().width == 2 && image.size().height == 3, "NImage creation failed");
        image.draw(painter, {0, 0, 2, 3}, {0, 0, 2, 3});

        nector::Animation animation(100);
        animation.addKey(1, 0, 0);
        animation.addKey(1, 100, 10);
        animation.start();
        animation.tick(50);
        require(std::fabs(animation.sample(1) - 5) < 0.001, "animation wrapper failed");

        nector::Input input;
        nector::InputEvent key_event;
        key_event.type = nector::InputEventType::KeyDown;
        key_event.key = static_cast<std::int32_t>(nector::Key::Enter);
        input.push(key_event);
        require(input.isKeyDown(nector::Key::Enter), "input state wrapper failed");
        require(input.poll().type == nector::InputEventType::KeyDown, "input queue wrapper failed");

        int event_count = 0;
        nector::Scene first_scene("first", 200, 100);
        first_scene.setEventCallback([&event_count](const nector::UiEvent& event) {
            if (event.type == nector::UiEventType::Custom) ++event_count;
        });
        nector::Node label = first_scene.addNode(first_scene.root(), nector::NodeType::Label, "label");
        label.setString(nector::NodeProperty::Text, "before");
        nector::Scene second_scene("second", 300, 120);
        second_scene = std::move(first_scene);
        second_scene.emitEvent("moved", "ok", 1);
        require(event_count == 1, "scene callback did not survive move assignment");
        require(second_scene.findNode("label").string(nector::NodeProperty::Text) == "before",
                "scene contents did not survive move assignment");

        int reentrant_events = 0;
        std::unique_ptr<nector::Scene> reentrant_scene = std::make_unique<nector::Scene>("old", 100, 80);
        reentrant_scene->setEventCallback([&reentrant_scene, &reentrant_events](const nector::UiEvent&) {
            ++reentrant_events;
            reentrant_scene = std::make_unique<nector::Scene>("replacement", 120, 90);
        });
        reentrant_scene->emitEvent("replace");
        require(reentrant_events == 1 && reentrant_scene->title() == "replacement",
                "scene replacement inside its callback failed");

        const std::string source =
            "window \"main\" { label \"status\" { text = \"before\"; } }\n"
            "action \"finish\" { set \"status\".text = \"after\"; emit \"done\", \"yes\", 7; }\n";
        std::vector<std::uint8_t> package_data = nector::CdslPackage::compileText(source);
        nector::CdslPackage package = nector::CdslPackage::loadMemory(package_data);
        int package_events = 0;
        package.setEventCallback([&package_events](const nector::UiEvent& event) {
            if (event.name == "done" && event.text == "yes" && event.value == 7) ++package_events;
        });
        nector::CdslPackage moved_package = std::move(package);
        moved_package.call("finish");
        require(package_events == 1, "package callback did not survive move");
        require(moved_package.scene().findNode("status").string(nector::NodeProperty::Text) == "after",
                "package action failed through RAII wrapper");

        bool rejected = false;
        try {
            nector::Surface invalid_surface(0, 10);
        } catch (const nector::Error& error) {
            rejected = error.status() == nector::Status::InvalidArgument;
        }
        require(rejected, "C status was not translated to nector::Error");
        std::cout << "test_raii: PASS; resources, moves, callbacks, NImage, input, animation and CDSL\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_raii: FAIL: " << error.what() << '\n';
        return 1;
    }
}
