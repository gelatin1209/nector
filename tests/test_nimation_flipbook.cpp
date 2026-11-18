#include "nimation.h"

#include <array>
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

void requireRejected(nector_status status, const char* message) {
    require(status != nector_ok, message);
}

nector_image_handle makeImage(uint8_t marker) {
    const uint8_t rgba[4] = {marker, 0, 0, 255};
    nector_image_handle image = nullptr;
    check(nectorImageCreate(1, 1, rgba, sizeof(rgba), &image));
    return image;
}

void requireMarker(nector_image_handle image, uint8_t marker) {
    uint8_t rgba[4]{};
    check(nectorImageReadPixels(image, rgba, sizeof(rgba)));
    require(rgba[0] == marker && rgba[3] == 255, "image pixel or lifetime is wrong");
}

void requireCurrentMarker(nector_animation_handle animation, uint8_t marker) {
    nector_image_handle current = nullptr;
    check(nectorFlipbookGetCurrentImage(animation, &current));
    require(current != nullptr, "current image is null");
    requireMarker(current, marker);
    nectorImageRelease(current);
}

using AnimationPtr = std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)>;

}

int main() {
    try {
        nector_image_handle standalone = makeImage(77);
        check(nectorImageRetain(standalone));
        nectorImageDestroy(standalone);
        requireMarker(standalone, 77);
        nectorImageRelease(standalone);
        requireRejected(nectorImageRetain(nullptr), "null image retain was accepted");
        nectorImageRelease(nullptr);

        nector_animation_handle empty_raw = nullptr;
        check(nectorCreateFlipbook(1, 10, &empty_raw));
        AnimationPtr empty(empty_raw, nectorDestroyAnimation);
        nector_image_handle missing = reinterpret_cast<nector_image_handle>(1);
        require(nectorFlipbookGetCurrentImage(empty_raw, &missing) == nector_not_found && missing == nullptr,
                "empty image slot did not report not_found");

        constexpr int32_t image_count = 21;
        nector_animation_handle raw = nullptr;
        check(nectorCreateFlipbook(image_count, 10, &raw));
        AnimationPtr flipbook(raw, nectorDestroyAnimation);
        std::array<nector_image_handle, image_count> images{};
        for (int32_t index = 0; index < image_count; ++index) {
            images[static_cast<size_t>(index)] = makeImage(static_cast<uint8_t>(index));
            check(nectorFlipbookSetFrame(raw, index, images[static_cast<size_t>(index)]));
        }
        for (nector_image_handle image : images) nectorImageDestroy(image);

        for (int32_t index = 0; index < image_count; ++index) {
            check(nectorAnimationSeekFrame(raw, index));
            requireCurrentMarker(raw, static_cast<uint8_t>(index));
        }

        check(nectorAnimationStop(raw));
        check(nectorAnimationStart(raw));
        for (int32_t index = 0; index < image_count; ++index) {
            int32_t current_frame = -1;
            check(nectorAnimationGetCurrentFrame(raw, &current_frame));
            require(current_frame == index, "flipbook playback frame order is wrong");
            requireCurrentMarker(raw, static_cast<uint8_t>(index));
            check(nectorAnimationTick(raw, 10));
        }
        int32_t finished = 0;
        check(nectorAnimationIsFinished(raw, &finished));
        require(finished == 1, "flipbook did not finish after its last frame");
        requireCurrentMarker(raw, 20);

        nector_image_handle replacement = makeImage(222);
        check(nectorFlipbookSetFrame(raw, 0, replacement));
        nectorImageDestroy(replacement);
        check(nectorAnimationSeekFrame(raw, 0));
        requireCurrentMarker(raw, 222);

        nector_image_handle retained = nullptr;
        check(nectorFlipbookGetCurrentImage(raw, &retained));
        flipbook.reset();
        requireMarker(retained, 222);
        nectorImageRelease(retained);

        nector_image_handle invalid_slot_image = makeImage(1);
        requireRejected(nectorFlipbookSetFrame(empty_raw, -1, invalid_slot_image), "negative image slot was accepted");
        requireRejected(nectorFlipbookSetFrame(empty_raw, 0, nullptr), "null image slot was accepted");
        nectorImageDestroy(invalid_slot_image);

        std::cout << "test_nimation_flipbook: PASS; 21 ordered clips; slot replacement, retain/release and getter lifetime\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nimation_flipbook: FAIL: " << error.what() << '\n';
        return 1;
    }
}
