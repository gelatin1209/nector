#ifndef nector_interface_h
#define nector_interface_h

#include "scene.h"

#if defined(_WIN32)
  #if defined(nector_interface_export)
    #define nector_interface_api __declspec(dllexport)
  #else
    #define nector_interface_api __declspec(dllimport)
  #endif
#else
  #define nector_interface_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_interface_context nector_interface_context;
typedef struct nector_interface_control nector_interface_control;
typedef nector_interface_context* nector_interface_context_handle;
typedef nector_interface_control* nector_interface_control_handle;
typedef nector_interface_control_handle nector_interface_row_handle;
typedef nector_interface_control_handle nector_interface_button_handle;

typedef enum nector_interface_row_part {
    nector_interface_row_node = 0,
    nector_interface_row_viewport = 1,
    nector_interface_row_content = 2,
    nector_interface_row_backward = 3,
    nector_interface_row_forward = 4
} nector_interface_row_part;

typedef void (*nector_interface_event_callback)(const nector_ui_event* event, void* user_data);

/*
 * A context does not own its scene. The scene must outlive the context. A context
 * owns every control it creates, its scene nodes, and the scene event callback
 * slot until nectorInterfaceDestroyContext returns. Use the interface callbacks
 * below instead of replacing the scene callback while the context is alive.
 * Node handles returned by this API are borrowed from their owning control.
 */
nector_interface_api nector_status nectorInterfaceCreateContext(nector_scene_handle scene, nector_interface_context_handle* out_context);
nector_interface_api void nectorInterfaceDestroyContext(nector_interface_context_handle context);
nector_interface_api nector_status nectorInterfaceLayout(nector_interface_context_handle context, int32_t width, int32_t height);
nector_interface_api nector_status nectorInterfaceRefresh(nector_interface_context_handle context);
nector_interface_api nector_status nectorInterfaceSetEventCallback(nector_interface_context_handle context, nector_interface_event_callback callback, void* user_data);

/* A null parent adds the control to the scene root; a Row parent adds to its content. */
nector_interface_api nector_status nectorInterfaceCreateControl(nector_interface_context_handle context, nector_interface_control_handle parent, nector_node_type type, const char* id, nector_interface_control_handle* out_control);
nector_interface_api nector_status nectorInterfaceCreateRow(nector_interface_context_handle context, nector_interface_control_handle parent, const char* id, nector_interface_row_handle* out_row);
nector_interface_api nector_status nectorInterfaceCreateButton(nector_interface_context_handle context, nector_interface_control_handle parent, const char* id, const char* text, nector_interface_button_handle* out_button);
nector_interface_api nector_status nectorInterfaceDestroyControl(nector_interface_control_handle control);
nector_interface_api nector_status nectorInterfaceControlGetNode(nector_interface_control_handle control, nector_node_handle* out_node);
nector_interface_api nector_status nectorInterfaceControlSetEventCallback(nector_interface_control_handle control, nector_interface_event_callback callback, void* user_data);

nector_interface_api nector_status nectorInterfaceControlSetString(nector_interface_control_handle control, nector_node_property property, const char* value);
nector_interface_api nector_status nectorInterfaceControlSetNumber(nector_interface_control_handle control, nector_node_property property, double value);
nector_interface_api nector_status nectorInterfaceControlSetBool(nector_interface_control_handle control, nector_node_property property, int32_t value);
nector_interface_api nector_status nectorInterfaceControlSetColor(nector_interface_control_handle control, nector_node_property property, nector_color value);
nector_interface_api nector_status nectorInterfaceControlGetString(nector_interface_control_handle control, nector_node_property property, const char** out_value);
nector_interface_api nector_status nectorInterfaceControlGetNumber(nector_interface_control_handle control, nector_node_property property, double* out_value);
nector_interface_api nector_status nectorInterfaceControlGetBool(nector_interface_control_handle control, nector_node_property property, int32_t* out_value);
nector_interface_api nector_status nectorInterfaceControlGetColor(nector_interface_control_handle control, nector_node_property property, nector_color* out_value);

/* layout: 0 = horizontal, 1 = vertical. scroll is nonnegative content pixels. */
nector_interface_api nector_status nectorInterfaceRowSetLayout(nector_interface_row_handle row, int32_t layout);
nector_interface_api nector_status nectorInterfaceRowGetLayout(nector_interface_row_handle row, int32_t* out_layout);
nector_interface_api nector_status nectorInterfaceRowSetScroll(nector_interface_row_handle row, double scroll);
nector_interface_api nector_status nectorInterfaceRowGetScroll(nector_interface_row_handle row, double* out_scroll);
nector_interface_api nector_status nectorInterfaceRowScrollBy(nector_interface_row_handle row, double delta);
nector_interface_api nector_status nectorInterfaceRowGetMaxScroll(nector_interface_row_handle row, double* out_max_scroll);
nector_interface_api nector_status nectorInterfaceRowGetPart(nector_interface_row_handle row, nector_interface_row_part part, nector_node_handle* out_node);

#ifdef __cplusplus
}
#endif
#endif
