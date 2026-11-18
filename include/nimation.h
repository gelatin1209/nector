#ifndef nector_anim_h
#define nector_anim_h

#include "core.h"
#include "image.h"

#if defined(_WIN32)
  #if defined(nector_anim_export)
    #define nector_anim_api __declspec(dllexport)
  #else
    #define nector_anim_api __declspec(dllimport)
  #endif
#else
  #define nector_anim_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_animation nector_animation;
typedef nector_animation* nector_animation_handle;

typedef enum nector_easing {
    nector_ease_linear = 0,
    nector_ease_in = 1,
    nector_ease_out = 2,
    nector_ease_in_out = 3,
    nector_ease_smooth = 4,
    nector_ease_bounce = 5,
    nector_ease_cubic_bezier = 6,
    nector_ease_steps = 7
} nector_easing;

typedef enum nector_step_position {
    nector_step_end = 0,
    nector_step_start = 1
} nector_step_position;

typedef struct nector_easing_parameters {
    double x1;
    double y1;
    double x2;
    double y2;
    uint32_t steps;
    nector_step_position step_position;
} nector_easing_parameters;

typedef enum nector_playback_direction {
    nector_playback_normal = 0,
    nector_playback_reverse = 1,
    nector_playback_ping_pong = 2
} nector_playback_direction;

typedef enum nector_animation_event_type {
    nector_animation_frame_event = 0,
    nector_animation_loop_event = 1,
    nector_animation_finish_event = 2
} nector_animation_event_type;

typedef struct nector_animation_event {
    nector_animation_event_type type;
    int32_t frame_index;
    uint64_t completed_loops;
    double position_ms;
} nector_animation_event;

typedef void (*nector_animation_event_callback)(const nector_animation_event* event, void* user_data);

typedef enum nector_behaviour {
    nector_behaviour_forward = 0,
    nector_behaviour_backward = 1,
    nector_behaviour_turn_left = 2,
    nector_behaviour_turn_right = 3,
    nector_behaviour_hide = 4,
    nector_behaviour_scale = 5,
    nector_behaviour_fade = 6
} nector_behaviour;

typedef struct nector_behaviour_sample {
    double x;
    double y;
    double rotation_degrees;
    int32_t visible;
} nector_behaviour_sample;

typedef struct nector_behaviour_sample_ex {
    double x;
    double y;
    double rotation_degrees;
    double scale;
    double opacity;
    int32_t visible;
} nector_behaviour_sample_ex;

nector_anim_api nector_status nectorCreateAnimation(double duration_ms, nector_animation_handle* out_animation);
nector_anim_api nector_status nectorCreateFrameAnimation(int32_t frame_count, double frame_duration_ms, nector_animation_handle* out_animation);
nector_anim_api nector_status nectorCreateBehaviourAnimation(nector_behaviour behaviour, double duration_ms, double amount, nector_animation_handle* out_animation);
nector_anim_api nector_status nectorCreateFlipbook(int32_t frame_count, double frame_duration_ms, nector_animation_handle* out_animation);
nector_anim_api nector_status nectorCreateHybrid(int32_t frame_count, double frame_duration_ms, nector_animation_handle* out_animation);
nector_anim_api void nectorDestroyAnimation(nector_animation_handle animation);
nector_anim_api nector_status nectorAnimationAddKey(nector_animation_handle animation, int32_t channel, double time_ms, double value, nector_easing easing);
nector_anim_api nector_status nectorAnimationAddKeyEx(nector_animation_handle animation, int32_t channel, double time_ms, double value, nector_easing easing, const nector_easing_parameters* parameters);
/* Frame and Hybrid animations only. Frame ranges are inclusive; Hide ignores the values. */
nector_anim_api nector_status nectorAnimationAddBehaviour(nector_animation_handle animation, nector_behaviour behaviour, int32_t start_frame, int32_t end_frame, double start_value, double end_value);
nector_anim_api nector_status nectorAnimationSetLoop(nector_animation_handle animation, int32_t loop_count);
nector_anim_api nector_status nectorAnimationSetDirection(nector_animation_handle animation, nector_playback_direction direction);
nector_anim_api nector_status nectorAnimationGetDirection(nector_animation_handle animation, nector_playback_direction* out_direction);
nector_anim_api nector_status nectorAnimationSetRate(nector_animation_handle animation, double rate);
nector_anim_api nector_status nectorAnimationGetRate(nector_animation_handle animation, double* out_rate);
nector_anim_api nector_status nectorAnimationSetEventCallback(nector_animation_handle animation, nector_animation_event_callback callback, void* user_data);
nector_anim_api nector_status nectorAnimationGetCompletedLoops(nector_animation_handle animation, uint64_t* out_completed_loops);
nector_anim_api nector_status nectorAnimationStart(nector_animation_handle animation);
nector_anim_api nector_status nectorAnimationPause(nector_animation_handle animation);
nector_anim_api nector_status nectorAnimationStop(nector_animation_handle animation);
nector_anim_api nector_status nectorAnimationSeek(nector_animation_handle animation, double time_ms);
nector_anim_api nector_status nectorAnimationSeekFrame(nector_animation_handle animation, int32_t frame_index);
nector_anim_api nector_status nectorAnimationTick(nector_animation_handle animation, double delta_ms);
nector_anim_api nector_status nectorAnimationSample(nector_animation_handle animation, int32_t channel, double* out_value);
nector_anim_api nector_status nectorAnimationSampleBehaviour(nector_animation_handle animation, nector_behaviour_sample* out_sample);
nector_anim_api nector_status nectorAnimationSampleBehaviourEx(nector_animation_handle animation, nector_behaviour_sample_ex* out_sample);
nector_anim_api nector_status nectorAnimationGetPosition(nector_animation_handle animation, double* out_time_ms);
nector_anim_api nector_status nectorAnimationGetFrameCount(nector_animation_handle animation, int32_t* out_frame_count);
nector_anim_api nector_status nectorAnimationGetFrameDuration(nector_animation_handle animation, double* out_frame_duration_ms);
nector_anim_api nector_status nectorAnimationGetCurrentFrame(nector_animation_handle animation, int32_t* out_frame_index);
nector_anim_api nector_status nectorAnimationIsPlaying(nector_animation_handle animation, int32_t* out_playing);
nector_anim_api nector_status nectorAnimationIsFinished(nector_animation_handle animation, int32_t* out_finished);
nector_anim_api nector_status nectorFlipbookSetFrame(nector_animation_handle animation, int32_t frame_index, nector_image_handle image);
/* The returned image is retained and must be released with nectorImageRelease. */
nector_anim_api nector_status nectorFlipbookGetCurrentImage(nector_animation_handle animation, nector_image_handle* out_image);

#ifdef __cplusplus
}
#endif
#endif
