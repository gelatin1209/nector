#include "nimation.h"
#include "audio.h"
#include "input.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void write16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value));
    data.push_back(static_cast<uint8_t>(value >> 8));
}
void write32(std::vector<uint8_t>& data, uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) data.push_back(static_cast<uint8_t>(value >> shift));
}
std::vector<uint8_t> makeWav() {
    constexpr uint32_t sample_rate = 8000;
    constexpr uint32_t frames = 400;
    std::vector<uint8_t> data;
    data.insert(data.end(), {'R', 'I', 'F', 'F'});
    write32(data, 36 + frames * 2);
    data.insert(data.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    write32(data, 16);
    write16(data, 1);
    write16(data, 1);
    write32(data, sample_rate);
    write32(data, sample_rate * 2);
    write16(data, 2);
    write16(data, 16);
    data.insert(data.end(), {'d', 'a', 't', 'a'});
    write32(data, frames * 2);
    for (uint32_t index = 0; index < frames; ++index) {
        const double angle = 6.28318530717958647692 * 440.0 * index / sample_rate;
        write16(data, static_cast<uint16_t>(static_cast<int16_t>(std::sin(angle) * 5000)));
    }
    return data;
}
}

int main() {
    try {
        nector_animation_handle animation = nullptr;
        check(nectorCreateAnimation(100, &animation));
        std::unique_ptr<nector_animation, decltype(&nectorDestroyAnimation)> animation_guard(animation, nectorDestroyAnimation);
        check(nectorAnimationAddKey(animation, 7, 0, 0, nector_ease_linear));
        check(nectorAnimationAddKey(animation, 7, 100, 100, nector_ease_linear));
        check(nectorAnimationStart(animation));
        check(nectorAnimationTick(animation, 50));
        double value = 0;
        check(nectorAnimationSample(animation, 7, &value));
        require(std::fabs(value - 50) < 0.001, "linear animation sample is wrong");
        check(nectorAnimationTick(animation, 50));
        int32_t finished = 0;
        check(nectorAnimationIsFinished(animation, &finished));
        require(finished == 1, "animation did not finish");

        nector_input_handle input = nullptr;
        check(nectorCreateInput(&input));
        std::unique_ptr<nector_input, decltype(&nectorDestroyInput)> input_guard(input, nectorDestroyInput);
        check(nectorInputInjectMouse(input, nector_event_mouse_move, 12, 34, 0));
        check(nectorInputInjectMouse(input, nector_event_mouse_down, 12, 34, 1));
        check(nectorInputInjectKey(input, nector_key_left, 1, 0));
        check(nectorInputInjectText(input, "A"));
        check(nectorInputInjectTouch(input, nector_event_touch_begin, 9, 20, 30, 0.5f));
        float x = 0, y = 0;
        check(nectorInputGetMousePosition(input, &x, &y));
        int32_t down = 0;
        check(nectorInputIsMouseDown(input, 1, &down));
        int32_t touches = 0;
        check(nectorInputGetTouchCount(input, &touches));
        require(x == 12 && y == 34 && down == 1 && touches == 1, "input state is wrong");
        nector_input_event event{};
        check(nectorInputPoll(input, &event));
        require(event.type == nector_event_mouse_move && event.x == 12, "input queue order is wrong");
        check(nectorInputClear(input));

        const std::vector<uint8_t> wav = makeWav();
        {
            std::ofstream stream("build/test.wav", std::ios::binary | std::ios::trunc);
            stream.write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
        }
        nector_audio_handle audio = nullptr;
        check(nectorCreateAudio(&audio));
        std::unique_ptr<nector_audio, decltype(&nectorDestroyAudio)> audio_guard(audio, nectorDestroyAudio);
        nector_sound_handle sound = nullptr;
        check(nectorAudioLoadMemory(audio, wav.data(), wav.size(), &sound));
        std::unique_ptr<nector_sound, decltype(&nectorAudioUnload)> sound_guard(sound, nectorAudioUnload);
        double duration = 0;
        check(nectorAudioGetDuration(sound, &duration));
        require(std::fabs(duration - 0.05) < 0.0001, "WAV duration is wrong");
        check(nectorAudioSetVolume(sound, 0.25f));
        check(nectorAudioSetLooping(sound, 1));
        check(nectorAudioPlay(sound));
        check(nectorAudioPause(sound));
        check(nectorAudioSeek(sound, 0.02));
        check(nectorAudioStop(sound));
        check(nectorAudioLoad(audio, "build/test.wav", &sound));
        sound_guard.reset(sound);
        std::cout << "test_media: PASS; animation(50ms)=50; input mouse=(12,34), touches=1; WAV duration=0.05s\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_media: FAIL: " << error.what() << '\n';
        return 1;
    }
}
