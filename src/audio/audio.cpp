#include "audio.h"
#include "../core/internal.h"
#include "../../third_party/miniaudio/miniaudio.h"

#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

struct nector_audio {};
struct nector_sound {};

namespace {
class AudioState {
public:
    AudioState() = default;
    AudioState(const AudioState&) = delete;
    AudioState& operator=(const AudioState&) = delete;

    ~AudioState() {
        if (initialized_) {
            ma_engine_uninit(&engine_);
        }
    }

    ma_result initialize() {
        const ma_result result = ma_engine_init(nullptr, &engine_);
        initialized_ = result == MA_SUCCESS;
        return result;
    }

    ma_engine* engine() {
        return &engine_;
    }

    std::mutex load_mutex;

private:
    ma_engine engine_{};
    bool initialized_ = false;
};

class SoundState {
public:
    explicit SoundState(std::shared_ptr<AudioState> audio) : audio_(std::move(audio)) {}
    SoundState(const SoundState&) = delete;
    SoundState& operator=(const SoundState&) = delete;

    ~SoundState() {
        if (sound_initialized_) {
            ma_sound_uninit(&sound_);
        }
        if (decoder_initialized_) {
            ma_decoder_uninit(&decoder_);
        }
    }

    ma_result initializeFromFile(const char* path) {
        const ma_result result = ma_decoder_init_file(path, nullptr, &decoder_);
        decoder_initialized_ = result == MA_SUCCESS;
        return result == MA_SUCCESS ? initializeSound() : result;
    }

    ma_result initializeFromMemory(const uint8_t* data, size_t size) {
        if (size > encoded_data_.max_size()) {
            return MA_TOO_BIG;
        }
        encoded_data_.assign(data, data + size);
        const ma_result result = ma_decoder_init_memory(encoded_data_.data(), encoded_data_.size(), nullptr, &decoder_);
        decoder_initialized_ = result == MA_SUCCESS;
        return result == MA_SUCCESS ? initializeSound() : result;
    }

    ma_sound* sound() {
        return &sound_;
    }

    ma_uint32 sampleRate() const {
        return sample_rate_;
    }

    bool hasDuration() const {
        return has_duration_;
    }

    double duration() const {
        return duration_;
    }

    ma_uint64 frameCount() const {
        return frame_count_;
    }

    std::mutex mutex;

private:
    ma_result initializeSound() {
        ma_result result = ma_sound_init_from_data_source(
            audio_->engine(), &decoder_, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &sound_);
        if (result != MA_SUCCESS) {
            return result;
        }
        sound_initialized_ = true;

        result = ma_sound_get_data_format(&sound_, nullptr, nullptr, &sample_rate_, nullptr, 0);
        if (result != MA_SUCCESS || sample_rate_ == 0) {
            return result == MA_SUCCESS ? MA_INVALID_DATA : result;
        }

        result = ma_sound_get_length_in_pcm_frames(&sound_, &frame_count_);
        if (result == MA_SUCCESS) {
            has_duration_ = true;
            duration_ = static_cast<double>(frame_count_) / static_cast<double>(sample_rate_);
        } else if (result != MA_NOT_IMPLEMENTED && result != MA_NO_DATA_AVAILABLE) {
            return result;
        }
        return MA_SUCCESS;
    }

    std::shared_ptr<AudioState> audio_;
    ma_decoder decoder_{};
    ma_sound sound_{};
    std::vector<uint8_t> encoded_data_;
    ma_uint32 sample_rate_ = 0;
    ma_uint64 frame_count_ = 0;
    double duration_ = 0.0;
    bool decoder_initialized_ = false;
    bool sound_initialized_ = false;
    bool has_duration_ = false;
};

std::mutex registry_mutex;
std::unordered_map<nector_audio*, std::shared_ptr<AudioState>> audio_states;
std::unordered_map<nector_sound*, std::shared_ptr<SoundState>> sound_states;

std::shared_ptr<AudioState> acquireAudio(nector_audio_handle audio) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    const auto found = audio_states.find(audio);
    return found == audio_states.end() ? nullptr : found->second;
}

std::shared_ptr<SoundState> acquireSound(nector_sound_handle sound) {
    std::lock_guard<std::mutex> lock(registry_mutex);
    const auto found = sound_states.find(sound);
    return found == sound_states.end() ? nullptr : found->second;
}

nector_status statusFromMiniaudio(ma_result result) {
    switch (result) {
        case MA_INVALID_ARGS:
        case MA_OUT_OF_RANGE:
        case MA_BAD_SEEK:
            return nector_invalid_argument;
        case MA_OUT_OF_MEMORY:
            return nector_out_of_memory;
        case MA_DOES_NOT_EXIST:
            return nector_not_found;
        case MA_ACCESS_DENIED:
        case MA_TOO_MANY_OPEN_FILES:
        case MA_IO_ERROR:
        case MA_NO_SPACE:
            return nector_io_error;
        case MA_INVALID_FILE:
        case MA_INVALID_DATA:
        case MA_CRC_MISMATCH:
            return nector_invalid_format;
        case MA_NOT_IMPLEMENTED:
        case MA_FORMAT_NOT_SUPPORTED:
        case MA_DEVICE_TYPE_NOT_SUPPORTED:
        case MA_SHARE_MODE_NOT_SUPPORTED:
        case MA_NO_BACKEND:
        case MA_NO_DEVICE:
        case MA_API_NOT_FOUND:
        case MA_BACKEND_NOT_ENABLED:
            return nector_unsupported;
        case MA_INVALID_OPERATION:
        case MA_DEVICE_NOT_INITIALIZED:
        case MA_DEVICE_ALREADY_INITIALIZED:
        case MA_DEVICE_NOT_STARTED:
        case MA_DEVICE_NOT_STOPPED:
            return nector_invalid_state;
        case MA_TOO_BIG:
            return nector_limit_exceeded;
        default:
            return nector_backend_error;
    }
}

nector_status miniaudioFailure(const char* operation, ma_result result) {
    const std::string message = std::string(operation) + ": " + ma_result_description(result);
    return nectorFail(statusFromMiniaudio(result), message.c_str());
}

nector_status registerSound(const std::shared_ptr<SoundState>& state, nector_sound_handle* out_sound) {
    std::unique_ptr<nector_sound> handle(new nector_sound);
    {
        std::lock_guard<std::mutex> lock(registry_mutex);
        sound_states.emplace(handle.get(), state);
    }
    *out_sound = handle.release();
    return nector_ok;
}

template <typename Load>
nector_status loadSound(nector_audio_handle audio, nector_sound_handle* out_sound, Load load) {
    const std::shared_ptr<AudioState> audio_state = acquireAudio(audio);
    if (audio_state == nullptr) {
        return nectorFail(nector_invalid_argument, "audio handle is invalid");
    }

    std::lock_guard<std::mutex> lock(audio_state->load_mutex);
    auto sound_state = std::make_shared<SoundState>(audio_state);
    const ma_result result = load(sound_state);
    if (result != MA_SUCCESS) {
        return miniaudioFailure("failed to load audio", result);
    }
    return registerSound(sound_state, out_sound);
}
}

nector_status nectorCreateAudio(nector_audio_handle* out_audio) {
    if (out_audio == nullptr) return nectorFail(nector_invalid_argument, "out_audio is null");
    *out_audio = nullptr;
    nector_api_try {
        auto state = std::make_shared<AudioState>();
        const ma_result result = state->initialize();
        if (result != MA_SUCCESS) {
            return miniaudioFailure("failed to initialize audio device", result);
        }

        std::unique_ptr<nector_audio> handle(new nector_audio);
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            audio_states.emplace(handle.get(), state);
        }
        *out_audio = handle.release();
        return nector_ok;
    } nector_api_catch
}

void nectorDestroyAudio(nector_audio_handle audio) {
    if (audio == nullptr) return;
    try {
        std::shared_ptr<AudioState> state;
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            const auto found = audio_states.find(audio);
            if (found == audio_states.end()) {
                nectorCoreSetError(nector_invalid_argument, "audio handle is invalid");
                return;
            }
            state = std::move(found->second);
            audio_states.erase(found);
            delete audio;
        }
        state.reset();
    } catch (const std::exception& exception) {
        nectorCoreSetError(nector_backend_error, exception.what());
    } catch (...) {
        nectorCoreSetError(nector_backend_error, "unknown native error");
    }
}

nector_status nectorAudioLoad(nector_audio_handle audio, const char* path, nector_sound_handle* out_sound) {
    if (out_sound == nullptr) return nectorFail(nector_invalid_argument, "out_sound is null");
    *out_sound = nullptr;
    if (audio == nullptr || path == nullptr || path[0] == '\0') {
        return nectorFail(nector_invalid_argument, "audio or path is invalid");
    }
    nector_api_try {
        return loadSound(audio, out_sound, [path](const std::shared_ptr<SoundState>& state) {
            return state->initializeFromFile(path);
        });
    } nector_api_catch
}

nector_status nectorAudioLoadMemory(nector_audio_handle audio, const uint8_t* data, size_t size,
                                    nector_sound_handle* out_sound) {
    if (out_sound == nullptr) return nectorFail(nector_invalid_argument, "out_sound is null");
    *out_sound = nullptr;
    if (audio == nullptr || data == nullptr || size == 0) {
        return nectorFail(nector_invalid_argument, "audio or memory buffer is invalid");
    }
    nector_api_try {
        return loadSound(audio, out_sound, [data, size](const std::shared_ptr<SoundState>& state) {
            return state->initializeFromMemory(data, size);
        });
    } nector_api_catch
}

void nectorAudioUnload(nector_sound_handle sound) {
    if (sound == nullptr) return;
    try {
        std::shared_ptr<SoundState> state;
        {
            std::lock_guard<std::mutex> lock(registry_mutex);
            const auto found = sound_states.find(sound);
            if (found == sound_states.end()) {
                nectorCoreSetError(nector_invalid_argument, "sound handle is invalid");
                return;
            }
            state = std::move(found->second);
            sound_states.erase(found);
            delete sound;
        }
        state.reset();
    } catch (const std::exception& exception) {
        nectorCoreSetError(nector_backend_error, exception.what());
    } catch (...) {
        nectorCoreSetError(nector_backend_error, "unknown native error");
    }
}

nector_status nectorAudioPlay(nector_sound_handle sound) {
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        const ma_result result = ma_sound_start(state->sound());
        return result == MA_SUCCESS ? nector_ok : miniaudioFailure("failed to play audio", result);
    } nector_api_catch
}

nector_status nectorAudioPause(nector_sound_handle sound) {
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        const ma_result result = ma_sound_stop(state->sound());
        return result == MA_SUCCESS ? nector_ok : miniaudioFailure("failed to pause audio", result);
    } nector_api_catch
}

nector_status nectorAudioStop(nector_sound_handle sound) {
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        ma_result result = ma_sound_stop(state->sound());
        if (result != MA_SUCCESS) return miniaudioFailure("failed to stop audio", result);
        result = ma_sound_seek_to_pcm_frame(state->sound(), 0);
        return result == MA_SUCCESS ? nector_ok : miniaudioFailure("failed to rewind audio", result);
    } nector_api_catch
}

nector_status nectorAudioSetVolume(nector_sound_handle sound, float volume) {
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    if (!nectorFinite(volume) || volume < 0.0f || volume > 1.0f) {
        return nectorFail(nector_invalid_argument, "volume must be between zero and one");
    }
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        ma_sound_set_volume(state->sound(), volume);
        return nector_ok;
    } nector_api_catch
}

nector_status nectorAudioGetVolume(nector_sound_handle sound, float* out_volume) {
    if (out_volume == nullptr) return nectorFail(nector_invalid_argument, "out_volume is null");
    *out_volume = 0.0f;
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        *out_volume = ma_sound_get_volume(state->sound());
        return nector_ok;
    } nector_api_catch
}

nector_status nectorAudioSetLooping(nector_sound_handle sound, int32_t looping) {
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    if (looping != 0 && looping != 1) {
        return nectorFail(nector_invalid_argument, "looping must be zero or one");
    }
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        ma_sound_set_looping(state->sound(), looping == 1 ? MA_TRUE : MA_FALSE);
        return nector_ok;
    } nector_api_catch
}

nector_status nectorAudioIsPlaying(nector_sound_handle sound, int32_t* out_playing) {
    if (out_playing == nullptr) return nectorFail(nector_invalid_argument, "out_playing is null");
    *out_playing = 0;
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        *out_playing = ma_sound_is_playing(state->sound()) && !ma_sound_at_end(state->sound()) ? 1 : 0;
        return nector_ok;
    } nector_api_catch
}

nector_status nectorAudioSeek(nector_sound_handle sound, double seconds) {
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    if (!std::isfinite(seconds) || seconds < 0.0) {
        return nectorFail(nector_invalid_argument, "seek position must be finite and nonnegative");
    }
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->hasDuration() && seconds > state->duration()) {
            return nectorFail(nector_invalid_argument, "seek position exceeds audio duration");
        }

        const long double frame_position = static_cast<long double>(seconds) * state->sampleRate();
        if (frame_position > static_cast<long double>(std::numeric_limits<ma_uint64>::max())) {
            return nectorFail(nector_limit_exceeded, "seek position is too large");
        }
        ma_uint64 frame = static_cast<ma_uint64>(frame_position);
        if (state->hasDuration() && frame > state->frameCount()) {
            frame = state->frameCount();
        }
        const ma_result result = ma_sound_seek_to_pcm_frame(state->sound(), frame);
        return result == MA_SUCCESS ? nector_ok : miniaudioFailure("failed to seek audio", result);
    } nector_api_catch
}

nector_status nectorAudioGetDuration(nector_sound_handle sound, double* out_seconds) {
    if (out_seconds == nullptr) return nectorFail(nector_invalid_argument, "out_seconds is null");
    *out_seconds = 0.0;
    if (sound == nullptr) return nectorFail(nector_invalid_argument, "sound is null");
    nector_api_try {
        const auto state = acquireSound(sound);
        if (state == nullptr) return nectorFail(nector_invalid_argument, "sound handle is invalid");
        std::lock_guard<std::mutex> lock(state->mutex);
        if (!state->hasDuration()) {
            return nectorFail(nector_unsupported, "audio duration is unavailable");
        }
        *out_seconds = state->duration();
        return nector_ok;
    } nector_api_catch
}
