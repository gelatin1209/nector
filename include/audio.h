#ifndef nector_audio_h
#define nector_audio_h

#include "core.h"

#if defined(_WIN32)
  #if defined(nector_audio_export)
    #define nector_audio_api __declspec(dllexport)
  #else
    #define nector_audio_api __declspec(dllimport)
  #endif
#else
  #define nector_audio_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_audio nector_audio;
typedef struct nector_sound nector_sound;
typedef nector_audio* nector_audio_handle;
typedef nector_sound* nector_sound_handle;

nector_audio_api nector_status nectorCreateAudio(nector_audio_handle* out_audio);
nector_audio_api void nectorDestroyAudio(nector_audio_handle audio);
nector_audio_api nector_status nectorAudioLoad(nector_audio_handle audio, const char* path, nector_sound_handle* out_sound);
nector_audio_api nector_status nectorAudioLoadMemory(nector_audio_handle audio, const uint8_t* data, size_t size, nector_sound_handle* out_sound);
nector_audio_api void nectorAudioUnload(nector_sound_handle sound);
nector_audio_api nector_status nectorAudioPlay(nector_sound_handle sound);
nector_audio_api nector_status nectorAudioPause(nector_sound_handle sound);
nector_audio_api nector_status nectorAudioStop(nector_sound_handle sound);
nector_audio_api nector_status nectorAudioSetVolume(nector_sound_handle sound, float volume);
nector_audio_api nector_status nectorAudioGetVolume(nector_sound_handle sound, float* out_volume);
nector_audio_api nector_status nectorAudioSetLooping(nector_sound_handle sound, int32_t looping);
nector_audio_api nector_status nectorAudioIsPlaying(nector_sound_handle sound, int32_t* out_playing);
nector_audio_api nector_status nectorAudioSeek(nector_sound_handle sound, double seconds);
nector_audio_api nector_status nectorAudioGetDuration(nector_sound_handle sound, double* out_seconds);

#ifdef __cplusplus
}
#endif
#endif
