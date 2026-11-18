#ifndef nector_nimation_animation_state_h
#define nector_nimation_animation_state_h

#include "nimation.h"
#include "../core/internal.h"

#include <cstdint>
#include <map>
#include <vector>

namespace nimation_internal {

enum class AnimationKind {
    keyframe,
    frame,
    behaviour,
    flipbook,
    hybrid
};

struct Key {
    double time;
    double value;
    nector_easing easing;
    nector_easing_parameters parameters;
};

struct BehaviourSpan {
    nector_behaviour behaviour;
    int32_t start_frame;
    int32_t end_frame;
    double start_value;
    double end_value;
};

}

struct nector_animation {
    std::map<int32_t, std::vector<nimation_internal::Key>> channels;
    double duration = 0;
    double position = 0;
    int32_t loop_count = 0;
    uint64_t completed_loops = 0;
    double playback_rate = 1.0;
    nector_playback_direction direction = nector_playback_normal;
    int travel_direction = 1;
    nector_animation_event_callback event_callback = nullptr;
    void* event_user_data = nullptr;
    bool dispatching_event = false;
    bool playing = false;
    bool finished = false;
    nimation_internal::AnimationKind kind = nimation_internal::AnimationKind::keyframe;
    int32_t frame_count = 0;
    double frame_duration = 0;
    nector_behaviour behaviour = nector_behaviour_forward;
    double behaviour_amount = 0;
    std::vector<nimation_internal::BehaviourSpan> behaviours;
    std::vector<nector_image_handle> images;

    ~nector_animation();
};

#endif
