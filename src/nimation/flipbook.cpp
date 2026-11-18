#include "internal.h"

nector_status nectorCreateFrameAnimation(int32_t frame_count, double frame_duration_ms,
                                         nector_animation_handle* out_animation) {
    return nimation_internal::createFrameTimeline(nimation_internal::AnimationKind::frame,
                                                  frame_count, frame_duration_ms, out_animation);
}

nector_status nectorCreateFlipbook(int32_t frame_count, double frame_duration_ms,
                                   nector_animation_handle* out_animation) {
    return nimation_internal::createFrameTimeline(nimation_internal::AnimationKind::flipbook,
                                                  frame_count, frame_duration_ms, out_animation);
}

nector_status nectorFlipbookSetFrame(nector_animation_handle animation, int32_t frame_index, nector_image_handle image) {
    if (!nimation_internal::validAnimation(animation) || image == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or image is null");
    }
    if (!nimation_internal::hasImageClips(animation->kind)) {
        return nectorFail(nector_invalid_state, "animation has no image clips");
    }
    if (frame_index < 0 || frame_index >= animation->frame_count) {
        return nectorFail(nector_invalid_argument, "frame index is outside the animation");
    }
    return nimation_internal::storeImageClip(*animation, frame_index, image);
}

nector_status nectorFlipbookGetCurrentImage(nector_animation_handle animation, nector_image_handle* out_image) {
    if (!nimation_internal::validAnimation(animation) || out_image == nullptr) {
        return nectorFail(nector_invalid_argument, "animation or output is null");
    }
    *out_image = nullptr;
    if (!nimation_internal::hasImageClips(animation->kind)) {
        return nectorFail(nector_invalid_state, "animation has no image clips");
    }
    nector_image_handle image = animation->images[static_cast<size_t>(nimation_internal::currentFrame(*animation))];
    if (image == nullptr) return nectorFail(nector_not_found, "current frame has no image");
    const nector_status status = nectorImageRetain(image);
    if (status != nector_ok) return status;
    *out_image = image;
    return nector_ok;
}
