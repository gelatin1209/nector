#include "nimation.h"

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

void requireRejected(nector_status status, const char* message) {
    require(status != nector_ok, message);
}

int32_t frame(nector_animation_handle animation) {
    int32_t value = -1;
    check(nectorAnimationGetCurrentFrame(animation, &value));
    return value;
}

double position(nector_animation_handle animation) {
    double value = -1;
    check(nectorAnimationGetPosition(animation, &value));
    return value;
}

using AnimationPtr = std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)>;

}

int main() {
    try {
        nector_animation_handle invalid = reinterpret_cast<nector_animation_handle>(1);
        requireRejected(nectorCreateFrameAnimation(0, 10, &invalid), "zero frame capacity was accepted");
        require(invalid == nullptr, "failed frame creation left a non-null output");
        requireRejected(nectorCreateFrameAnimation(61, 10, &invalid), "frame capacity above 60 was accepted");
        requireRejected(nectorCreateFrameAnimation(3, 0, &invalid), "zero frame duration was accepted");
        requireRejected(nectorCreateFrameAnimation(3, NAN, &invalid), "NaN frame duration was accepted");
        check(nectorCreateFrameAnimation(60, 1, &invalid));
        nectorDestroyAnimation(invalid);

        nector_animation_handle raw = nullptr;
        check(nectorCreateFrameAnimation(3, 100, &raw));
        AnimationPtr animation(raw, nectorDestroyAnimation);
        int32_t count = 0;
        double frame_duration = 0;
        check(nectorAnimationGetFrameCount(raw, &count));
        check(nectorAnimationGetFrameDuration(raw, &frame_duration));
        require(count == 3 && frame_duration == 100 && frame(raw) == 0, "frame timeline metadata is wrong");
        check(nectorAnimationStart(raw));
        check(nectorAnimationTick(raw, 99.999));
        require(frame(raw) == 0, "frame changed before its boundary");
        check(nectorAnimationTick(raw, 0.001));
        require(frame(raw) == 1 && std::fabs(position(raw) - 100) < 0.0001, "first frame boundary is wrong");
        check(nectorAnimationTick(raw, 100));
        require(frame(raw) == 2, "second frame boundary is wrong");
        check(nectorAnimationTick(raw, 99.999));
        require(frame(raw) == 2, "last frame changed before completion");
        check(nectorAnimationTick(raw, 0.001));
        int32_t finished = 0;
        check(nectorAnimationIsFinished(raw, &finished));
        require(finished == 1 && frame(raw) == 2 && position(raw) == 300, "completion did not hold the last frame");

        check(nectorAnimationSeekFrame(raw, 1));
        check(nectorAnimationIsFinished(raw, &finished));
        require(frame(raw) == 1 && position(raw) == 100 && finished == 0, "frame seek is wrong");
        requireRejected(nectorAnimationSeekFrame(raw, -1), "negative frame seek was accepted");
        requireRejected(nectorAnimationSeekFrame(raw, 3), "out-of-range frame seek was accepted");

        check(nectorAnimationStop(raw));
        check(nectorAnimationSetLoop(raw, 1));
        check(nectorAnimationStart(raw));
        check(nectorAnimationTick(raw, 350));
        require(frame(raw) == 0 && std::fabs(position(raw) - 50) < 0.0001, "first loop wrap is wrong");
        check(nectorAnimationTick(raw, 250));
        check(nectorAnimationIsFinished(raw, &finished));
        require(finished == 1 && frame(raw) == 2 && position(raw) == 300, "finite loop completion is wrong");

        nector_animation_handle legacy_raw = nullptr;
        check(nectorCreateAnimation(100, &legacy_raw));
        AnimationPtr legacy(legacy_raw, nectorDestroyAnimation);
        requireRejected(nectorAnimationStart(legacy_raw), "empty legacy animation was allowed to start");
        requireRejected(nectorAnimationSeekFrame(legacy_raw, 0), "legacy animation accepted a frame seek");
        check(nectorAnimationAddKey(legacy_raw, 7, 0, 0, nector_ease_linear));
        check(nectorAnimationAddKey(legacy_raw, 7, 100, 100, nector_ease_linear));
        check(nectorAnimationStart(legacy_raw));
        check(nectorAnimationTick(legacy_raw, 50));
        double value = 0;
        check(nectorAnimationSample(legacy_raw, 7, &value));
        require(std::fabs(value - 50) < 0.0001, "legacy keyframe sampling regressed");

        std::cout << "test_nimation_timeline: PASS; capacity=1..60; boundaries, seek, tick, loops and legacy keys\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nimation_timeline: FAIL: " << error.what() << '\n';
        return 1;
    }
}
