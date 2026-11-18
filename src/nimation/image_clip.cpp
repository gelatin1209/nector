#include "internal.h"

namespace nimation_internal {

nector_status storeImageClip(nector_animation& animation, int32_t frame_index, nector_image_handle image) {
    const nector_status status = nectorImageRetain(image);
    if (status != nector_ok) return status;
    nector_image_handle& slot = animation.images[static_cast<size_t>(frame_index)];
    nector_image_handle previous = slot;
    slot = image;
    nectorImageRelease(previous);
    return nector_ok;
}

}
