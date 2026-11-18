#include "animation_state.h"

nector_animation::~nector_animation() {
    for (nector_image_handle image : images) nectorImageRelease(image);
}
