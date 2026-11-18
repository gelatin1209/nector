#include "internal.h"

namespace {

bool sameBinding(const AnimationBinding& first, const AnimationBinding& second) {
    return first.animation == second.animation && first.channel == second.channel &&
           first.node == second.node && first.property == second.property &&
           first.finished_action_id == second.finished_action_id;
}

} // namespace

nector_status nectorSceneBindAnimation(nector_scene_handle scene, nector_animation_handle animation,
                                       int32_t channel, nector_node_handle node,
                                       nector_node_property property, int32_t finished_action_id) {
    if (scene == nullptr || animation == nullptr || !sceneNodeValid(node) || node->scene != scene) {
        return nectorFail(nector_invalid_argument, "scene, animation, or node is invalid");
    }
    SceneGuard guard(scene);
    if (channel < 0 || channel >= 10000) return nectorFail(nector_invalid_argument, "animation channel is invalid");
    double current_value = 0;
    const nector_status property_status = nectorSceneGetNodeNumber(node, property, &current_value);
    if (property_status != nector_ok) return property_status;
    int32_t finished = 0;
    const nector_status animation_status = nectorAnimationIsFinished(animation, &finished);
    if (animation_status != nector_ok) return animation_status;
    nector_api_try {
        AnimationBinding binding;
        binding.animation = animation;
        binding.channel = channel;
        binding.node = node;
        binding.property = property;
        binding.finished_action_id = finished_action_id;
        binding.was_finished = finished != 0;
        scene->animations.push_back(binding);
        return nector_ok;
    } nector_api_catch
}

nector_status nectorSceneTick(nector_scene_handle scene, double delta_ms) {
    if (scene == nullptr || !std::isfinite(delta_ms) || delta_ms < 0 || delta_ms > 1.0e9) {
        return nectorFail(nector_invalid_argument, "scene or animation delta is invalid");
    }
    SceneGuard guard(scene);
    nector_api_try {
        const std::vector<AnimationBinding> bindings = scene->animations;
        std::vector<nector_animation_handle> animations;
        for (const AnimationBinding& binding : bindings) {
            if (std::find(animations.begin(), animations.end(), binding.animation) == animations.end()) {
                animations.push_back(binding.animation);
            }
        }
        for (nector_animation_handle animation : animations) {
            const nector_status status = nectorAnimationTick(animation, delta_ms);
            if (status != nector_ok) return status;
        }

        std::vector<AnimationBinding> finished_bindings;
        std::vector<double> sampled_values;
        for (const AnimationBinding& binding : bindings) {
            if (!sceneNodeValid(binding.node) || binding.node->scene != scene) continue;
            double sampled = 0;
            nector_status status = nectorAnimationSample(binding.animation, binding.channel, &sampled);
            if (status == nector_not_found) continue;
            if (status != nector_ok) return status;
            if (!std::isfinite(sampled)) return nectorFail(nector_backend_error, "animation sampled a non-finite value");
            status = sceneSetNodeNumber(binding.node, binding.property, sampled);
            if (status != nector_ok) return status;

            int32_t finished = 0;
            status = nectorAnimationIsFinished(binding.animation, &finished);
            if (status != nector_ok) return status;
            for (AnimationBinding& active : scene->animations) {
                if (sameBinding(active, binding)) active.was_finished = finished != 0;
            }
            if (finished != 0 && !binding.was_finished) {
                finished_bindings.push_back(binding);
                sampled_values.push_back(sampled);
            }
        }

        for (size_t index = 0; index < finished_bindings.size(); ++index) {
            const AnimationBinding& binding = finished_bindings[index];
            const bool has_action = binding.finished_action_id >= 0;
            const std::string action_name = has_action ? std::to_string(binding.finished_action_id) : std::string();
            nector_status status = sceneEmitNodeEvent(scene, nector_ui_animation_finished, binding.node,
                                                      nullptr, has_action ? action_name.c_str() : nullptr,
                                                      sampled_values[index], nullptr);
            if (status != nector_ok) return status;
            if (has_action) {
                status = sceneEmitNodeEvent(scene, nector_ui_custom, nullptr, nullptr, action_name.c_str(),
                                            binding.finished_action_id, nullptr);
                if (status != nector_ok) return status;
            }
        }
        return nector_ok;
    } nector_api_catch
}
