#include "nector.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

static_assert(!std::is_copy_constructible<::NImation>::value, "NImation must be move-only");
static_assert(!std::is_copy_assignable<::NImation>::value, "NImation must be move-only");
static_assert(std::is_move_constructible<::NImation>::value, "NImation must be movable");
static_assert(!std::is_copy_constructible<::NImation::Flipbook>::value, "Flipbook must be move-only");
static_assert(!std::is_copy_constructible<::NImation::Hybrid>::value, "Hybrid must be move-only");
static_assert(std::is_copy_constructible<::NImation::Timeline>::value, "Timeline is a borrowed view");

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double first, double second) {
    return std::fabs(first - second) < 0.0001;
}

nector::Image makeImage(std::uint8_t marker) {
    const std::vector<std::uint8_t> rgba{marker, 0, 0, 255};
    return nector::Image::create(1, 1, rgba);
}

std::uint8_t marker(const nector::Image& image) {
    return image.readPixels()[0];
}

}

int main() {
    try {
        bool rejected = false;
        try {
            ::NImation invalid(0);
        } catch (const nector::Error& error) {
            rejected = error.status() == nector::Status::InvalidArgument;
        }
        require(rejected, "C++ NImation accepted zero frames");

        ::NImation animation(4, 100);
        animation.addBehaviour(::NImation::Behaviour::TurnRight, 0, 1, 0, 90);
        animation.addBehaviour(::NImation::Behaviour::Forward, 1, 2, 0, 40);
        animation.addBehaviour(::NImation::Behaviour::Hide, 3, 3, 0, 0);
        ::NImation::Timeline timeline = animation.timeline();
        require(timeline.frameCount() == 4 && near(timeline.frameDuration(), 100),
                "C++ timeline metadata is wrong");
        timeline.seek(150);
        ::NImation::Transform transform = animation.sampleTransform();
        require(timeline.currentFrame() == 1 && near(transform.rotationDegrees, 67.5) &&
                near(transform.y, -10) && transform.visible,
                "C++ behaviour sampling is wrong");
        animation.start();
        animation.tick(250);
        require(animation.isFinished() && animation.currentFrame() == 3 && !animation.sampleTransform().visible,
                "C++ playback did not reach the Hide endpoint");

        ::NImation moved(std::move(animation));
        moved.seekFrame(0);
        require(moved.sampleTransform().visible && moved.timeline().position() == 0,
                "moving NImation lost its timeline");

        ::NImation::Flipbook flipbook(2, 100);
        flipbook.setFrame(0, makeImage(10));
        flipbook.setFrame(1, makeImage(20));
        flipbook.seekFrame(1);
        require(marker(flipbook.currentImage()) == 20, "C++ Flipbook selected the wrong image");

        ::NImation::Hybrid hybrid(2, 100);
        hybrid.setFrame(0, makeImage(30));
        hybrid.setFrame(1, makeImage(40));
        hybrid.addBehaviour(::NImation::Behaviour::Backward, 0, 1, 0, 20);
        hybrid.seek(150);
        require(marker(hybrid.currentImage()) == 40 && near(hybrid.sampleTransform().y, 15),
                "C++ Hybrid did not synchronize movement and images");

        std::cout << "test_nimation_cpp: PASS; RAII, move-only, Timeline, Behaviour, Flipbook and Hybrid\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nimation_cpp: FAIL: " << error.what() << '\n';
        return 1;
    }
}
