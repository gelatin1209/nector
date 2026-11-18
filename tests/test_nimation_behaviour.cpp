#include "nimation.h"

#include <cmath>
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

bool near(double first, double second) {
    return std::fabs(first - second) < 0.0001;
}

using AnimationPtr = std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)>;

void checkMovement(nector_behaviour behaviour, double expected_y, double expected_rotation) {
    nector_animation_handle raw = nullptr;
    check(nectorCreateBehaviourAnimation(behaviour, 100, 90, &raw));
    AnimationPtr animation(raw, nectorDestroyAnimation);
    check(nectorAnimationSeek(raw, 50));
    nector_behaviour_sample sample{};
    check(nectorAnimationSampleBehaviour(raw, &sample));
    require(near(sample.x, 0) && near(sample.y, expected_y) &&
            near(sample.rotation_degrees, expected_rotation) && sample.visible == 1,
            "movement behaviour sampled incorrect values");
    check(nectorAnimationSeek(raw, 100));
    check(nectorAnimationSampleBehaviour(raw, &sample));
    require(near(sample.y, expected_y * 2) && near(sample.rotation_degrees, expected_rotation * 2),
            "movement behaviour endpoint is wrong");
}

}

int main() {
    try {
        checkMovement(nector_behaviour_forward, -45, 0);
        checkMovement(nector_behaviour_backward, 45, 0);
        checkMovement(nector_behaviour_turn_left, 0, -45);
        checkMovement(nector_behaviour_turn_right, 0, 45);

        nector_animation_handle raw = nullptr;
        check(nectorCreateBehaviourAnimation(nector_behaviour_hide, 100, 0, &raw));
        AnimationPtr hide(raw, nectorDestroyAnimation);
        nector_behaviour_sample sample{};
        check(nectorAnimationSeek(raw, 99.999));
        check(nectorAnimationSampleBehaviour(raw, &sample));
        require(sample.visible == 1 && sample.x == 0 && sample.y == 0 && sample.rotation_degrees == 0,
                "Hide became invisible before its endpoint");
        check(nectorAnimationStart(raw));
        check(nectorAnimationTick(raw, 100));
        check(nectorAnimationSampleBehaviour(raw, &sample));
        require(sample.visible == 0, "Hide did not become invisible at its endpoint");
        check(nectorAnimationStart(raw));
        check(nectorAnimationSampleBehaviour(raw, &sample));
        require(sample.visible == 1, "restarting Hide did not restore visibility");

        check(nectorAnimationSetLoop(raw, -1));
        check(nectorAnimationSeek(raw, 100));
        check(nectorAnimationStart(raw));
        check(nectorAnimationTick(raw, 100));
        check(nectorAnimationSampleBehaviour(raw, &sample));
        require(sample.visible == 1, "looping Hide did not restore visibility at the wrap");

        nector_animation_handle framed_raw = nullptr;
        check(nectorCreateFrameAnimation(4, 100, &framed_raw));
        AnimationPtr framed(framed_raw, nectorDestroyAnimation);
        check(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_turn_right, 0, 1, 0, 90));
        check(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_forward, 1, 2, 0, 40));
        check(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_hide, 3, 3, 0, 0));
        check(nectorAnimationSeek(framed_raw, 50));
        check(nectorAnimationSampleBehaviour(framed_raw, &sample));
        require(near(sample.rotation_degrees, 22.5) && near(sample.y, 0) && sample.visible == 1,
                "the first behaviour span did not interpolate");
        check(nectorAnimationSeek(framed_raw, 150));
        check(nectorAnimationSampleBehaviour(framed_raw, &sample));
        require(near(sample.rotation_degrees, 67.5) && near(sample.y, -10) && sample.visible == 1,
                "overlapping behaviour spans did not retain their independent channels");
        check(nectorAnimationSeek(framed_raw, 250));
        check(nectorAnimationSampleBehaviour(framed_raw, &sample));
        require(near(sample.rotation_degrees, 90) && near(sample.y, -30),
                "a completed behaviour span did not hold its end value");
        check(nectorAnimationSeek(framed_raw, 350));
        check(nectorAnimationSampleBehaviour(framed_raw, &sample));
        require(sample.visible == 1, "a Hide span hid the frame before its endpoint");
        check(nectorAnimationSeek(framed_raw, 400));
        check(nectorAnimationSampleBehaviour(framed_raw, &sample));
        require(sample.visible == 0, "a Hide span did not hide the frame at its endpoint");
        check(nectorAnimationSeek(framed_raw, 0));
        check(nectorAnimationSampleBehaviour(framed_raw, &sample));
        require(sample.visible == 1 && near(sample.y, 0) && near(sample.rotation_degrees, 0),
                "seeking to zero did not restore the initial behaviour state");
        requireRejected(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_forward, -1, 1, 0, 1),
                        "a negative behaviour start frame was accepted");
        requireRejected(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_forward, 2, 1, 0, 1),
                        "a reversed behaviour frame range was accepted");
        requireRejected(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_forward, 0, 4, 0, 1),
                        "an out-of-range behaviour end frame was accepted");
        requireRejected(nectorAnimationAddBehaviour(framed_raw, static_cast<nector_behaviour>(99), 0, 1, 0, 1),
                        "an invalid behaviour span type was accepted");
        requireRejected(nectorAnimationAddBehaviour(framed_raw, nector_behaviour_forward, 0, 1, NAN, 1),
                        "a NaN behaviour span value was accepted");

        nector_animation_handle flipbook_raw = nullptr;
        check(nectorCreateFlipbook(2, 100, &flipbook_raw));
        AnimationPtr flipbook(flipbook_raw, nectorDestroyAnimation);
        requireRejected(nectorAnimationAddBehaviour(flipbook_raw, nector_behaviour_forward, 0, 1, 0, 1),
                        "a Flipbook accepted a movement behaviour");
        requireRejected(nectorAnimationSampleBehaviour(flipbook_raw, &sample),
                        "a Flipbook sampled a movement behaviour");

        nector_animation_handle invalid = reinterpret_cast<nector_animation_handle>(1);
        requireRejected(nectorCreateBehaviourAnimation(static_cast<nector_behaviour>(99), 100, 1, &invalid),
                        "invalid behaviour was accepted");
        require(invalid == nullptr, "failed behaviour creation left a non-null output");
        requireRejected(nectorCreateBehaviourAnimation(nector_behaviour_forward, 100, INFINITY, &invalid),
                        "infinite behaviour amount was accepted");
        requireRejected(nectorCreateBehaviourAnimation(nector_behaviour_forward, 100, -1, &invalid),
                        "negative behaviour amount was accepted");

        nector_animation_handle legacy_raw = nullptr;
        check(nectorCreateAnimation(100, &legacy_raw));
        AnimationPtr legacy(legacy_raw, nectorDestroyAnimation);
        requireRejected(nectorAnimationSampleBehaviour(legacy_raw, &sample), "keyframe animation sampled as a behaviour");

        std::cout << "test_nimation_behaviour: PASS; Forward=-y, Backward=+y, turns=degrees, Hide at endpoint\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nimation_behaviour: FAIL: " << error.what() << '\n';
        return 1;
    }
}
