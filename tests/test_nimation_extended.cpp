#include "nector.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using AnimationPtr = std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)>;

struct Event {
    nector_animation_event_type type;
    int32_t frame;
    uint64_t loops;
    double position;
};

void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double first, double second) {
    return std::fabs(first - second) < 0.0001;
}

AnimationPtr frameAnimation(int32_t frames, double duration) {
    nector_animation_handle raw = nullptr;
    check(nectorCreateFrameAnimation(frames, duration, &raw));
    return AnimationPtr(raw, nectorDestroyAnimation);
}

double position(nector_animation_handle animation) {
    double value = -1;
    check(nectorAnimationGetPosition(animation, &value));
    return value;
}

uint64_t loops(nector_animation_handle animation) {
    uint64_t value = 0;
    check(nectorAnimationGetCompletedLoops(animation, &value));
    return value;
}

bool finished(nector_animation_handle animation) {
    int32_t value = 0;
    check(nectorAnimationIsFinished(animation, &value));
    return value != 0;
}

bool playing(nector_animation_handle animation) {
    int32_t value = 0;
    check(nectorAnimationIsPlaying(animation, &value));
    return value != 0;
}

void recordEvent(const nector_animation_event* event, void* user_data) {
    auto& events = *static_cast<std::vector<Event>*>(user_data);
    events.push_back({event->type, event->frame_index, event->completed_loops, event->position_ms});
}

void expectEvent(const Event& actual, nector_animation_event_type type, int32_t frame,
                 uint64_t loops, double position, const char* message) {
    require(actual.type == type && actual.frame == frame && actual.loops == loops &&
            near(actual.position, position), message);
}

double sampleAt(nector_animation_handle animation, int32_t channel, double time_ms) {
    check(nectorAnimationSeek(animation, time_ms));
    double value = 0;
    check(nectorAnimationSample(animation, channel, &value));
    return value;
}

void testLargeDeltaAndNormalEvents() {
    AnimationPtr plain = frameAnimation(3, 100);
    check(nectorAnimationSetLoop(plain.get(), 1));
    check(nectorAnimationStart(plain.get()));
    check(nectorAnimationTick(plain.get(), 650));
    require(finished(plain.get()) && !playing(plain.get()) && loops(plain.get()) == 1 &&
            near(position(plain.get()), 300), "large delta ignored the finite loop limit");

    AnimationPtr animation = frameAnimation(3, 100);
    std::vector<Event> events;
    check(nectorAnimationSetLoop(animation.get(), 1));
    check(nectorAnimationSetEventCallback(animation.get(), recordEvent, &events));
    check(nectorAnimationStart(animation.get()));
    check(nectorAnimationTick(animation.get(), 650));
    require(events.size() == 7, "large delta did not emit every normal-direction boundary");
    expectEvent(events[0], nector_animation_frame_event, 1, 0, 100, "normal frame 1 event is wrong");
    expectEvent(events[1], nector_animation_frame_event, 2, 0, 200, "normal frame 2 event is wrong");
    expectEvent(events[2], nector_animation_loop_event, 0, 1, 0, "normal loop event is wrong");
    expectEvent(events[3], nector_animation_frame_event, 0, 1, 0, "normal wrapped frame event is wrong");
    expectEvent(events[4], nector_animation_frame_event, 1, 1, 100, "second-pass frame 1 event is wrong");
    expectEvent(events[5], nector_animation_frame_event, 2, 1, 200, "second-pass frame 2 event is wrong");
    expectEvent(events[6], nector_animation_finish_event, 2, 1, 300, "normal finish event is wrong");
    require(near(position(animation.get()), position(plain.get())) &&
            loops(animation.get()) == loops(plain.get()) && finished(animation.get()),
            "event delivery changed normal playback state");
}

void testReverseAndPingPong() {
    AnimationPtr reverse = frameAnimation(3, 100);
    std::vector<Event> reverse_events;
    check(nectorAnimationSetDirection(reverse.get(), nector_playback_reverse));
    check(nectorAnimationSetEventCallback(reverse.get(), recordEvent, &reverse_events));
    check(nectorAnimationStart(reverse.get()));
    require(near(position(reverse.get()), 300), "reverse playback did not start at the end");
    check(nectorAnimationTick(reverse.get(), 300));
    require(reverse_events.size() == 3, "reverse playback emitted incorrect event count");
    expectEvent(reverse_events[0], nector_animation_frame_event, 1, 0, 200, "reverse first frame is wrong");
    expectEvent(reverse_events[1], nector_animation_frame_event, 0, 0, 100, "reverse second frame is wrong");
    expectEvent(reverse_events[2], nector_animation_finish_event, 0, 0, 0, "reverse finish is wrong");
    require(finished(reverse.get()) && near(position(reverse.get()), 0), "reverse did not finish at zero");

    AnimationPtr ping_pong = frameAnimation(2, 100);
    std::vector<Event> ping_pong_events;
    check(nectorAnimationSetDirection(ping_pong.get(), nector_playback_ping_pong));
    check(nectorAnimationSetLoop(ping_pong.get(), 1));
    check(nectorAnimationSetEventCallback(ping_pong.get(), recordEvent, &ping_pong_events));
    check(nectorAnimationStart(ping_pong.get()));
    check(nectorAnimationTick(ping_pong.get(), 800));
    require(ping_pong_events.size() == 6, "ping-pong emitted incorrect event count");
    expectEvent(ping_pong_events[0], nector_animation_frame_event, 1, 0, 100, "ping-pong forward frame is wrong");
    expectEvent(ping_pong_events[1], nector_animation_frame_event, 0, 0, 100, "ping-pong backward frame is wrong");
    expectEvent(ping_pong_events[2], nector_animation_loop_event, 0, 1, 0, "ping-pong loop is wrong");
    expectEvent(ping_pong_events[3], nector_animation_frame_event, 1, 1, 100, "ping-pong second forward frame is wrong");
    expectEvent(ping_pong_events[4], nector_animation_frame_event, 0, 1, 100, "ping-pong second backward frame is wrong");
    expectEvent(ping_pong_events[5], nector_animation_finish_event, 0, 1, 0, "ping-pong finish is wrong");
    require(finished(ping_pong.get()) && loops(ping_pong.get()) == 1 && near(position(ping_pong.get()), 0),
            "ping-pong did not complete one full round trip per loop");

    AnimationPtr midpoint = frameAnimation(2, 100);
    check(nectorAnimationSetDirection(midpoint.get(), nector_playback_ping_pong));
    check(nectorAnimationSetLoop(midpoint.get(), 1));
    check(nectorAnimationStart(midpoint.get()));
    check(nectorAnimationTick(midpoint.get(), 600));
    require(!finished(midpoint.get()) && loops(midpoint.get()) == 1 && near(position(midpoint.get()), 200),
            "large ping-pong delta did not preserve the turnaround");
    check(nectorAnimationTick(midpoint.get(), 200));
    require(finished(midpoint.get()) && near(position(midpoint.get()), 0),
            "large ping-pong delta did not finish at the origin");
}

void testRateSeekAndValidation() {
    AnimationPtr animation = frameAnimation(4, 50);
    check(nectorAnimationSetRate(animation.get(), 2.5));
    double rate = 0;
    check(nectorAnimationGetRate(animation.get(), &rate));
    require(near(rate, 2.5), "playback rate getter is wrong");
    check(nectorAnimationStart(animation.get()));
    check(nectorAnimationTick(animation.get(), 40));
    require(near(position(animation.get()), 100), "playback rate did not scale delta time");
    require(nectorAnimationSetRate(animation.get(), 0) == nector_invalid_argument,
            "zero playback rate was accepted");
    require(nectorAnimationSetRate(animation.get(), NAN) == nector_invalid_argument,
            "NaN playback rate was accepted");

    std::vector<Event> events;
    check(nectorAnimationSetEventCallback(animation.get(), recordEvent, &events));
    check(nectorAnimationSetLoop(animation.get(), 1));
    check(nectorAnimationStop(animation.get()));
    check(nectorAnimationStart(animation.get()));
    check(nectorAnimationTick(animation.get(), 100));
    require(loops(animation.get()) == 1 && !events.empty(), "test setup did not cross a loop");
    const size_t event_count = events.size();
    check(nectorAnimationSeek(animation.get(), 75));
    require(loops(animation.get()) == 0 && events.size() == event_count && !finished(animation.get()),
            "Seek did not reset loops silently");

    check(nectorAnimationSetDirection(animation.get(), nector_playback_ping_pong));
    check(nectorAnimationSeek(animation.get(), 0));
    require(!finished(animation.get()), "Seek(0) marked an unstarted ping-pong animation finished");
    nector_playback_direction direction = nector_playback_normal;
    check(nectorAnimationGetDirection(animation.get(), &direction));
    require(direction == nector_playback_ping_pong, "playback direction getter is wrong");
}

void testExtendedEasing() {
    nector_animation_handle raw = nullptr;
    check(nectorCreateAnimation(100, &raw));
    AnimationPtr animation(raw, nectorDestroyAnimation);
    const nector_easing_parameters bezier{0.42, 0.0, 0.58, 1.0, 0, nector_step_end};
    check(nectorAnimationAddKey(raw, 1, 0, 0, nector_ease_linear));
    check(nectorAnimationAddKeyEx(raw, 1, 100, 100, nector_ease_cubic_bezier, &bezier));
    const double first = sampleAt(raw, 1, 25);
    const double middle = sampleAt(raw, 1, 50);
    const double last = sampleAt(raw, 1, 75);
    require(near(middle, 50) && first > 0 && first < 50 && near(first + last, 100),
            "cubic-bezier easing is not symmetric or monotonic at its control midpoint");
    require(nectorAnimationAddKey(raw, 2, 0, 0, nector_ease_cubic_bezier) == nector_invalid_argument,
            "legacy AddKey accepted an extended easing");
    const nector_easing_parameters invalid_bezier{-0.1, 0, 0.5, 1, 0, nector_step_end};
    require(nectorAnimationAddKeyEx(raw, 2, 0, 0, nector_ease_cubic_bezier, &invalid_bezier) ==
            nector_invalid_argument, "invalid cubic-bezier x control was accepted");

    nector_animation_handle steps_raw = nullptr;
    check(nectorCreateAnimation(100, &steps_raw));
    AnimationPtr steps(steps_raw, nectorDestroyAnimation);
    check(nectorAnimationAddKey(steps_raw, 1, 0, 0, nector_ease_linear));
    const nector_easing_parameters end{0, 0, 0, 0, 4, nector_step_end};
    check(nectorAnimationAddKeyEx(steps_raw, 1, 100, 100, nector_ease_steps, &end));
    require(near(sampleAt(steps_raw, 1, 24), 0) && near(sampleAt(steps_raw, 1, 25), 25) &&
            near(sampleAt(steps_raw, 1, 99), 75), "steps(end) boundaries are wrong");

    nector_animation_handle start_raw = nullptr;
    check(nectorCreateAnimation(100, &start_raw));
    AnimationPtr start(start_raw, nectorDestroyAnimation);
    check(nectorAnimationAddKey(start_raw, 1, 0, 0, nector_ease_linear));
    const nector_easing_parameters beginning{0, 0, 0, 0, 4, nector_step_start};
    check(nectorAnimationAddKeyEx(start_raw, 1, 100, 100, nector_ease_steps, &beginning));
    require(near(sampleAt(start_raw, 1, 1), 25) && near(sampleAt(start_raw, 1, 99), 100),
            "steps(start) boundaries are wrong");
    const nector_easing_parameters invalid_steps{0, 0, 0, 0, 0, nector_step_end};
    require(nectorAnimationAddKeyEx(start_raw, 2, 0, 0, nector_ease_steps, &invalid_steps) ==
            nector_invalid_argument, "zero-step easing was accepted");
}

void testScaleAndFade() {
    nector_animation_handle scale_raw = nullptr;
    check(nectorCreateBehaviourAnimation(nector_behaviour_scale, 100, 2, &scale_raw));
    AnimationPtr scale(scale_raw, nectorDestroyAnimation);
    check(nectorAnimationSeek(scale_raw, 50));
    nector_behaviour_sample_ex sample{};
    check(nectorAnimationSampleBehaviourEx(scale_raw, &sample));
    require(near(sample.scale, 1.5) && near(sample.opacity, 1) && sample.visible == 1,
            "legacy Scale behaviour is wrong");

    nector_animation_handle fade_raw = nullptr;
    check(nectorCreateBehaviourAnimation(nector_behaviour_fade, 100, 0, &fade_raw));
    AnimationPtr fade(fade_raw, nectorDestroyAnimation);
    check(nectorAnimationSeek(fade_raw, 25));
    check(nectorAnimationSampleBehaviourEx(fade_raw, &sample));
    require(near(sample.opacity, 0.75) && near(sample.scale, 1), "legacy Fade behaviour is wrong");

    AnimationPtr framed = frameAnimation(2, 100);
    check(nectorAnimationAddBehaviour(framed.get(), nector_behaviour_scale, 0, 1, 1, 3));
    check(nectorAnimationAddBehaviour(framed.get(), nector_behaviour_fade, 0, 1, 1, 0));
    check(nectorAnimationSeek(framed.get(), 100));
    check(nectorAnimationSampleBehaviourEx(framed.get(), &sample));
    require(near(sample.scale, 2) && near(sample.opacity, 0.5), "Scale/Fade span interpolation is wrong");
    check(nectorAnimationSeek(framed.get(), 200));
    check(nectorAnimationSampleBehaviourEx(framed.get(), &sample));
    require(near(sample.scale, 3) && near(sample.opacity, 0), "Scale/Fade span endpoints are wrong");
    require(nectorAnimationAddBehaviour(framed.get(), nector_behaviour_scale, 0, 1, -1, 1) ==
            nector_invalid_argument, "negative Scale span was accepted");
    require(nectorAnimationAddBehaviour(framed.get(), nector_behaviour_fade, 0, 1, 0, 2) ==
            nector_invalid_argument, "out-of-range Fade span was accepted");
}

void testCppWrappers() {
    nector::Animation animation(200);
    animation.addKey(1, 0, 0);
    nector::EasingParameters parameters{};
    parameters.steps = 2;
    parameters.step_position = nector_step_start;
    animation.addKey(1, 200, 100, nector::Easing::Steps, parameters);
    std::vector<nector::AnimationEvent> events;
    animation.setEventCallback([&events](const nector::AnimationEvent& event) { events.push_back(event); });
    animation.setLoop(1);
    nector::Animation moved(std::move(animation));
    moved.start();
    moved.tick(400);
    require(events.size() == 2 && events[0].type == nector_animation_loop_event &&
            events[1].type == nector_animation_finish_event && moved.completedLoops() == 1 &&
            moved.isFinished(), "C++ callback did not survive a move");

    ::NImation::Hybrid hybrid(2, 100);
    hybrid.addBehaviour(::NImation::Behaviour::Scale, 0, 1, 1, 3);
    hybrid.addBehaviour(::NImation::Behaviour::Fade, 0, 1, 1, 0);
    hybrid.seek(100);
    const ::NImation::Transform transform = hybrid.sampleTransform();
    require(near(transform.scale, 2) && near(transform.opacity, 0.5),
            "C++ Hybrid discarded Scale/Fade samples");

    nector::Animation failure(100);
    failure.addKey(1, 0, 0);
    failure.addKey(1, 100, 1);
    failure.setEventCallback([](const nector::AnimationEvent&) { throw std::runtime_error("callback failure"); });
    failure.start();
    bool rejected = false;
    try {
        failure.tick(100);
    } catch (const nector::Error& error) {
        rejected = error.status() == nector::Status::CallbackError;
    }
    require(rejected && !failure.isPlaying(), "C++ callback exception was not translated safely");
}

}

int main() {
    try {
        testLargeDeltaAndNormalEvents();
        testReverseAndPingPong();
        testRateSeekAndValidation();
        testExtendedEasing();
        testScaleAndFade();
        testCppWrappers();
        std::cout << "test_nimation_extended: PASS; large delta, ordered events, directions, rate, easing, Scale/Fade and C++\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_nimation_extended: FAIL: " << error.what() << '\n';
        return 1;
    }
}
