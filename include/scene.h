#ifndef nector_scene_h
#define nector_scene_h

#include "nimation.h"
#include "font.h"
#include "image.h"
#include "input.h"

#if defined(_WIN32)
  #if defined(nector_scene_export)
    #define nector_scene_api __declspec(dllexport)
  #else
    #define nector_scene_api __declspec(dllimport)
  #endif
#else
  #define nector_scene_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_scene nector_scene;
typedef struct nector_node nector_node;
typedef nector_scene* nector_scene_handle;
typedef nector_node* nector_node_handle;

typedef enum nector_node_type {
    nector_node_window = 0,
    nector_node_column = 1,
    nector_node_row = 2,
    nector_node_label = 3,
    nector_node_button = 4,
    nector_node_line_edit = 5,
    nector_node_check_box = 6,
    nector_node_slider = 7,
    nector_node_progress_bar = 8,
    nector_node_spacer = 9,
    nector_node_canvas = 10,
    nector_node_image = 11
} nector_node_type;

typedef enum nector_node_property {
    nector_property_text = 0,
    nector_property_title = 1,
    nector_property_placeholder = 2,
    nector_property_source = 3,
    nector_property_width = 10,
    nector_property_height = 11,
    nector_property_padding = 12,
    nector_property_gap = 13,
    nector_property_flex = 14,
    nector_property_font_size = 15,
    nector_property_value = 16,
    nector_property_minimum = 17,
    nector_property_maximum = 18,
    nector_property_step = 19,
    nector_property_caret = 20,
    nector_property_corner_radius = 21,
    nector_property_content_offset_x = 22,
    nector_property_content_offset_y = 23,
    nector_property_scroll_step = 24,
    nector_property_layout = 25,
    nector_property_visible = 30,
    nector_property_enabled = 31,
    nector_property_checked = 32,
    nector_property_word_wrap = 33,
    nector_property_no_shrink = 34,
    nector_property_glass = 35,
    nector_property_background = 40,
    nector_property_foreground = 41,
    nector_property_border = 42,
    nector_property_accent = 43
} nector_node_property;

typedef enum nector_ui_event_type {
    nector_ui_none = 0,
    nector_ui_click = 1,
    nector_ui_text_changed = 2,
    nector_ui_value_changed = 3,
    nector_ui_toggled = 4,
    nector_ui_paint = 5,
    nector_ui_focus = 6,
    nector_ui_blur = 7,
    nector_ui_custom = 8,
    nector_ui_animation_finished = 9,
    nector_ui_quit = 10,
    nector_ui_scroll = 11
} nector_ui_event_type;

typedef struct nector_ui_event {
    nector_ui_event_type type;
    nector_node_handle node;
    const char* node_id;
    const char* text;
    const char* name;
    double value;
    nector_painter_handle painter;
    nector_rect bounds;
} nector_ui_event;

typedef void (*nector_scene_callback)(const nector_ui_event* event, void* user_data);

nector_scene_api nector_status nectorCreateScene(const char* title, int32_t width, int32_t height, nector_scene_handle* out_scene);
nector_scene_api void nectorDestroyScene(nector_scene_handle scene);
nector_scene_api nector_status nectorSceneGetRoot(nector_scene_handle scene, nector_node_handle* out_root);
nector_scene_api nector_status nectorSceneAddNode(nector_scene_handle scene, nector_node_handle parent, nector_node_type type, const char* id, nector_node_handle* out_node);
nector_scene_api nector_status nectorSceneFindNode(nector_scene_handle scene, const char* id, nector_node_handle* out_node);
nector_scene_api nector_status nectorSceneRemoveNode(nector_scene_handle scene, nector_node_handle node);
nector_scene_api nector_status nectorSceneSetNodeString(nector_node_handle node, nector_node_property property, const char* value);
nector_scene_api nector_status nectorSceneSetNodeNumber(nector_node_handle node, nector_node_property property, double value);
nector_scene_api nector_status nectorSceneSetNodeBool(nector_node_handle node, nector_node_property property, int32_t value);
nector_scene_api nector_status nectorSceneSetNodeColor(nector_node_handle node, nector_node_property property, nector_color value);
nector_scene_api nector_status nectorSceneGetNodeString(nector_node_handle node, nector_node_property property, const char** out_value);
nector_scene_api nector_status nectorSceneGetNodeNumber(nector_node_handle node, nector_node_property property, double* out_value);
nector_scene_api nector_status nectorSceneGetNodeBool(nector_node_handle node, nector_node_property property, int32_t* out_value);
nector_scene_api nector_status nectorSceneGetNodeColor(nector_node_handle node, nector_node_property property, nector_color* out_value);
nector_scene_api nector_status nectorSceneGetNodeType(nector_node_handle node, nector_node_type* out_type);
nector_scene_api nector_status nectorSceneGetNodeId(nector_node_handle node, const char** out_id);
nector_scene_api nector_status nectorSceneGetNodeBounds(nector_node_handle node, nector_rect* out_bounds);
nector_scene_api nector_status nectorSceneSetNodeImage(nector_node_handle node, nector_image_handle image);
nector_scene_api nector_status nectorSceneSetFont(nector_scene_handle scene, nector_font_handle font);
nector_scene_api nector_status nectorSceneSetEventCallback(nector_scene_handle scene, nector_scene_callback callback, void* user_data);
nector_scene_api nector_status nectorSceneEmit(nector_scene_handle scene, const char* name, const char* text, double value);
nector_scene_api nector_status nectorSceneBindAnimation(nector_scene_handle scene, nector_animation_handle animation, int32_t channel, nector_node_handle node, nector_node_property property, int32_t finished_action_id);
nector_scene_api nector_status nectorSceneTick(nector_scene_handle scene, double delta_ms);
nector_scene_api nector_status nectorSceneLayout(nector_scene_handle scene, int32_t width, int32_t height);
nector_scene_api nector_status nectorSceneDraw(nector_scene_handle scene, nector_painter_handle painter);
nector_scene_api nector_status nectorSceneHandleInput(nector_scene_handle scene, const nector_input_event* input_event);
nector_scene_api nector_status nectorSceneGetSize(nector_scene_handle scene, int32_t* out_width, int32_t* out_height);
nector_scene_api nector_status nectorSceneGetTitle(nector_scene_handle scene, const char** out_title);

#ifdef __cplusplus
}
#endif
#endif
