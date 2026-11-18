#ifndef nector_scene_internal_h
#define nector_scene_internal_h

#ifndef nector_scene_export
#define nector_scene_export 1
#endif
#include "scene.h"
#include "../core/internal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr double max_scene_dimension = 1000000.0;
constexpr double max_scene_value = 1.0e12;
constexpr size_t max_scene_text_size = 1024u * 1024u;
constexpr size_t max_scene_id_size = 255u;

struct nector_node {
    nector_scene_handle scene = nullptr;
    nector_node_type type = nector_node_window;
    std::string id;
    nector_node_handle parent = nullptr;
    std::vector<std::unique_ptr<nector_node>> children;
    bool alive = true;

    std::string text;
    std::string title;
    std::string placeholder;
    std::string source;

    double width = 0;
    double height = 0;
    double padding = 0;
    double gap = 0;
    double flex = 0;
    double font_size = 16;
    double value = 0;
    double minimum = 0;
    double maximum = 1;
    double step = 0.01;
    double caret = 0;
    double corner_radius = 0;
    double content_offset_x = 0;
    double content_offset_y = 0;
    double scroll_step = 0;
    int32_t layout = -1;

    int32_t visible = 1;
    int32_t enabled = 1;
    int32_t checked = 0;
    int32_t word_wrap = 0;
    int32_t no_shrink = 0;
    int32_t glass = 0;

    nector_color background{0, 0, 0, 0};
    nector_color foreground{0.10f, 0.12f, 0.16f, 1};
    nector_color border{0.65f, 0.68f, 0.73f, 1};
    nector_color accent{0.16f, 0.43f, 0.86f, 1};

    nector_rect bounds{0, 0, 0, 0};
    nector_rect clip_bounds{0, 0, 0, 0};
    nector_image_handle image = nullptr;
};

struct AnimationBinding {
    nector_animation_handle animation = nullptr;
    int32_t channel = 0;
    nector_node_handle node = nullptr;
    nector_node_property property = nector_property_value;
    int32_t finished_action_id = -1;
    bool was_finished = false;
};

struct nector_scene {
    std::string title;
    int32_t width = 0;
    int32_t height = 0;
    std::unique_ptr<nector_node> root;
    std::unordered_map<std::string, nector_node_handle> nodes;
    std::vector<std::unique_ptr<nector_node>> retired_nodes;
    nector_font_handle font = nullptr;
    nector_scene_callback callback = nullptr;
    void* user_data = nullptr;
    nector_node_handle focused_node = nullptr;
    nector_node_handle pressed_node = nullptr;
    int64_t active_touch_id = 0;
    bool has_active_touch = false;
    bool shift_down = false;
    uint32_t operation_depth = 0;
    bool destroy_pending = false;
    std::vector<AnimationBinding> animations;
};

class SceneGuard {
public:
    explicit SceneGuard(nector_scene_handle scene) : scene_(scene) {
        if (scene_ != nullptr) ++scene_->operation_depth;
    }

    SceneGuard(const SceneGuard&) = delete;
    SceneGuard& operator=(const SceneGuard&) = delete;

    ~SceneGuard() {
        if (scene_ == nullptr) return;
        --scene_->operation_depth;
        if (scene_->operation_depth == 0 && scene_->destroy_pending) {
            nector_scene_handle doomed = scene_;
            scene_ = nullptr;
            delete doomed;
        }
    }

private:
    nector_scene_handle scene_;
};

bool sceneNodeValid(nector_node_handle node);
bool sceneNodeVisible(nector_node_handle node);
bool sceneNodeEnabled(nector_node_handle node);
bool sceneNodeContainer(nector_node_type type);
bool sceneNodeHorizontal(nector_node_handle node);
bool sceneNodeFocusable(nector_node_handle node);
bool sceneRectContains(nector_rect rect, float x, float y);
nector_rect sceneIntersectRect(nector_rect first, nector_rect second);
float sceneClampFloat(double value, double minimum, double maximum);
std::vector<size_t> sceneUtf8Offsets(const std::string& text);
size_t sceneUtf8Length(const std::string& text);
size_t sceneByteOffsetForCaret(const std::string& text, double caret);

void sceneInitializeNode(nector_node_handle node, nector_scene_handle scene,
                         nector_node_type type, const std::string& id);
void sceneClearNodeReferences(nector_scene_handle scene, nector_node_handle node);
nector_status sceneSetNodeNumber(nector_node_handle node, nector_node_property property, double value);
nector_status sceneSetNodeString(nector_node_handle node, nector_node_property property, const char* value);

nector_status sceneEmitNodeEvent(nector_scene_handle scene, nector_ui_event_type type,
                                 nector_node_handle node, const char* text, const char* name,
                                 double value, nector_painter_handle painter);
nector_status sceneSetFocus(nector_scene_handle scene, nector_node_handle node);
nector_status sceneLayoutInternal(nector_scene_handle scene);
nector_status sceneDrawInternal(nector_scene_handle scene, nector_painter_handle painter);
nector_status sceneInputInternal(nector_scene_handle scene, const nector_input_event* input_event);

float sceneTextWidth(nector_scene_handle scene, nector_node_handle node, const std::string& text);
float sceneLineHeight(nector_scene_handle scene, nector_node_handle node);
float sceneAscent(nector_scene_handle scene, nector_node_handle node);

#endif
