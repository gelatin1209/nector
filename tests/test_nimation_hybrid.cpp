#include "nimation.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

nector_image_handle makeImage(uint8_t marker) {
    const uint8_t rgba[4] = {marker, 0, 0, 255};
    nector_image_handle image = nullptr;
    check(nectorImageCreate(1, 1, rgba, sizeof(rgba), &image));
    return image;
}

void requireState(nector_animation_handle animation, int32_t expected_frame, double expected_value, uint8_t marker) {
    int32_t frame = -1;
    double value = 0;
    check(nectorAnimationGetCurrentFrame(animation, &frame));
    check(nectorAnimationSample(animation, 4, &value));
    nector_behaviour_sample transform{};
    check(nectorAnimationSampleBehaviour(animation, &transform));
    require(frame == expected_frame && std::fabs(value - expected_value) < 0.0001 &&
            std::fabs(transform.rotation_degrees - expected_value * 0.3) < 0.0001 && transform.visible == 1,
            "hybrid motion, behaviour and frame timeline are not synchronized");
    nector_image_handle image = nullptr;
    check(nectorFlipbookGetCurrentImage(animation, &image));
    uint8_t rgba[4]{};
    check(nectorImageReadPixels(image, rgba, sizeof(rgba)));
    nectorImageRelease(image);
    require(rgba[0] == marker, "hybrid selected the wrong image");
}

using AnimationPtr = std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)>;

}

int main() {
    try {
        nector_animation_handle raw = nullptr;
        check(nectorCreateHybrid(3, 100, &raw));
        AnimationPtr hybrid(raw, nectorDestroyAnimation);
        std::array<nector_image_handle, 3> images{};
        for (int32_t index = 0; index < 3; ++index) {
            images[static_cast<size_t>(index)] = makeImage(static_cast<uint8_t>(10 + index));
            check(nectorFlipbookSetFrame(raw, index, images[static_cast<size_t>(index)]));
            nectorImageDestroy(images[static_cast<size_t>(index)]);
        }
        check(nectorAnimationAddKey(raw, 4, 0, 0, nector_ease_linear));
        check(nectorAnimationAddKey(raw, 4, 300, 300, nector_ease_linear));
        check(nectorAnimationAddBehaviour(raw, nector_behaviour_turn_right, 0, 2, 0, 90));

        check(nectorAnimationStart(raw));
        check(nectorAnimationTick(raw, 50));
        requireState(raw, 0, 50, 10);
        check(nectorAnimationTick(raw, 50));
        requireState(raw, 1, 100, 11);
        check(nectorAnimationSeek(raw, 150));
        requireState(raw, 1, 150, 11);
        check(nectorAnimationSeek(raw, 200));
        requireState(raw, 2, 200, 12);
        check(nectorAnimationSeek(raw, 300));
        requireState(raw, 2, 300, 12);

        check(nectorAnimationStart(raw));
        check(nectorAnimationTick(raw, 250));
        requireState(raw, 2, 250, 12);
        check(nectorAnimationTick(raw, 50));
        requireState(raw, 2, 300, 12);
        int32_t finished = 0;
        check(nectorAnimationIsFinished(raw, &finished));
        require(finished == 1, "hybrid did not finish on its shared timeline");

        std::cout << "test_nimation_hybrid: PASS; keyframe motion and image selection share position and frame boundaries\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nimation_hybrid: FAIL: " << error.what() << '\n';
        return 1;
    }
}
