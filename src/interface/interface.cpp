#ifndef nector_interface_export
#define nector_interface_export 1
#endif
#include "interface.h"

#include "context_registry.h"
#include "control_factory.h"
#include "control_properties.h"
#include "event_dispatch.h"
#include "interface_state.h"
#include "operation_guard.h"
#include "row_factory.h"
#include "row_layout.h"
#include "row_scroll.h"

#include <cmath>
#include <cstdint>
#include <exception>
#include <new>

using namespace interface_internal;

nector_status nectorInterfaceCreateContext(nector_scene_handle scene,
                                           nector_interface_context_handle* out_context) {
    if (out_context == nullptr) return interfaceFail(nector_invalid_argument, "out_context is null");
    *out_context = nullptr;
    if (scene == nullptr) return interfaceFail(nector_invalid_argument, "scene is null");
    nector_node_handle root = nullptr;
    const nector_status scene_status = nectorSceneGetRoot(scene, &root);
    if (scene_status != nector_ok) return scene_status;
    interface_api_try {
        return createContext(scene, dispatchSceneEvent, out_context);
    } interface_api_catch
}

void nectorInterfaceDestroyContext(nector_interface_context_handle context) {
    if (context == nullptr || context->destroy_pending) return;
    destroyContext(context);
}

nector_status nectorInterfaceLayout(nector_interface_context_handle context,
                                    int32_t width, int32_t height) {
    if (!contextValid(context)) return interfaceFail(nector_invalid_argument, "interface context is invalid");
    InterfaceGuard guard(context);
    const nector_status status = nectorSceneLayout(context->scene, width, height);
    if (status != nector_ok) return status;
    return refreshControls(context);
}

nector_status nectorInterfaceRefresh(nector_interface_context_handle context) {
    if (!contextValid(context)) return interfaceFail(nector_invalid_argument, "interface context is invalid");
    InterfaceGuard guard(context);
    return refreshControls(context);
}

nector_status nectorInterfaceSetEventCallback(nector_interface_context_handle context,
                                               nector_interface_event_callback callback,
                                               void* user_data) {
    if (!contextValid(context)) return interfaceFail(nector_invalid_argument, "interface context is invalid");
    InterfaceGuard guard(context);
    context->callback = callback;
    context->user_data = user_data;
    return nector_ok;
}

nector_status nectorInterfaceCreateControl(nector_interface_context_handle context,
                                            nector_interface_control_handle parent,
                                            nector_node_type type, const char* id,
                                            nector_interface_control_handle* out_control) {
    if (out_control == nullptr) return interfaceFail(nector_invalid_argument, "out_control is null");
    *out_control = nullptr;
    if (!contextValid(context) || (parent != nullptr && !controlValid(parent)) ||
        type < nector_node_column || type > nector_node_image) {
        return interfaceFail(nector_invalid_argument, "context, parent, or control type is invalid");
    }
    InterfaceGuard guard(context);
    interface_api_try {
        return createControl(context, parent, type, id, out_control);
    } interface_api_catch
}

nector_status nectorInterfaceCreateButton(nector_interface_context_handle context,
                                           nector_interface_control_handle parent,
                                           const char* id, const char* text,
                                           nector_interface_button_handle* out_button) {
    if (out_button == nullptr) return interfaceFail(nector_invalid_argument, "out_button is null");
    *out_button = nullptr;
    if (!contextValid(context) || text == nullptr || (parent != nullptr && !controlValid(parent))) {
        return interfaceFail(nector_invalid_argument, "context, parent, or text is invalid");
    }
    InterfaceGuard guard(context);
    interface_api_try {
        return createButton(context, parent, id, text, out_button);
    } interface_api_catch
}

nector_status nectorInterfaceCreateRow(nector_interface_context_handle context,
                                        nector_interface_control_handle parent,
                                        const char* id, nector_interface_row_handle* out_row) {
    if (out_row == nullptr) return interfaceFail(nector_invalid_argument, "out_row is null");
    *out_row = nullptr;
    if (!contextValid(context) || !compositeIdValid(id) || (parent != nullptr && !controlValid(parent))) {
        return interfaceFail(nector_invalid_argument, "context, parent, or row id is invalid");
    }
    InterfaceGuard guard(context);
    interface_api_try {
        return createRow(context, parent, id, out_row);
    } interface_api_catch
}

nector_status nectorInterfaceDestroyControl(nector_interface_control_handle control) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    interface_api_try {
        return destroyControl(control);
    } interface_api_catch
}

nector_status nectorInterfaceControlGetNode(nector_interface_control_handle control,
                                             nector_node_handle* out_node) {
    if (out_node == nullptr) return interfaceFail(nector_invalid_argument, "out_node is null");
    *out_node = nullptr;
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    *out_node = control->node;
    return nector_ok;
}

nector_status nectorInterfaceControlSetEventCallback(nector_interface_control_handle control,
                                                      nector_interface_event_callback callback,
                                                      void* user_data) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    control->callback = callback;
    control->user_data = user_data;
    return nector_ok;
}

nector_status nectorInterfaceControlSetString(nector_interface_control_handle control,
                                               nector_node_property property, const char* value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return setControlString(control, property, value);
}

nector_status nectorInterfaceControlSetNumber(nector_interface_control_handle control,
                                               nector_node_property property, double value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return setControlNumber(control, property, value);
}

nector_status nectorInterfaceControlSetBool(nector_interface_control_handle control,
                                             nector_node_property property, int32_t value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return setControlBool(control, property, value);
}

nector_status nectorInterfaceControlSetColor(nector_interface_control_handle control,
                                              nector_node_property property, nector_color value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return setControlColor(control, property, value);
}

nector_status nectorInterfaceControlGetString(nector_interface_control_handle control,
                                               nector_node_property property, const char** out_value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return getControlString(control, property, out_value);
}

nector_status nectorInterfaceControlGetNumber(nector_interface_control_handle control,
                                               nector_node_property property, double* out_value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return getControlNumber(control, property, out_value);
}

nector_status nectorInterfaceControlGetBool(nector_interface_control_handle control,
                                             nector_node_property property, int32_t* out_value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return getControlBool(control, property, out_value);
}

nector_status nectorInterfaceControlGetColor(nector_interface_control_handle control,
                                              nector_node_property property, nector_color* out_value) {
    if (!controlValid(control)) return interfaceFail(nector_invalid_argument, "control is invalid");
    InterfaceGuard guard(control->context);
    return getControlColor(control, property, out_value);
}

nector_status nectorInterfaceRowSetLayout(nector_interface_row_handle row, int32_t layout) {
    if (!rowValid(row) || (layout != 0 && layout != 1)) {
        return interfaceFail(nector_invalid_argument, "row or layout is invalid");
    }
    InterfaceGuard guard(row->context);
    return setRowLayout(row, layout);
}

nector_status nectorInterfaceRowGetLayout(nector_interface_row_handle row, int32_t* out_layout) {
    if (out_layout == nullptr) return interfaceFail(nector_invalid_argument, "out_layout is null");
    *out_layout = 0;
    if (!rowValid(row)) return interfaceFail(nector_invalid_argument, "row is invalid");
    InterfaceGuard guard(row->context);
    *out_layout = row->layout;
    return nector_ok;
}

nector_status nectorInterfaceRowSetScroll(nector_interface_row_handle row, double scroll) {
    if (!rowValid(row)) return interfaceFail(nector_invalid_argument, "row is invalid");
    InterfaceGuard guard(row->context);
    return setRowScroll(row, scroll);
}

nector_status nectorInterfaceRowGetScroll(nector_interface_row_handle row, double* out_scroll) {
    if (out_scroll == nullptr) return interfaceFail(nector_invalid_argument, "out_scroll is null");
    *out_scroll = 0;
    if (!rowValid(row)) return interfaceFail(nector_invalid_argument, "row is invalid");
    InterfaceGuard guard(row->context);
    *out_scroll = row->scroll;
    return nector_ok;
}

nector_status nectorInterfaceRowScrollBy(nector_interface_row_handle row, double delta) {
    if (!rowValid(row) || !std::isfinite(delta)) {
        return interfaceFail(nector_invalid_argument, "row or scroll delta is invalid");
    }
    InterfaceGuard guard(row->context);
    return scrollRowBy(row, delta);
}

nector_status nectorInterfaceRowGetMaxScroll(nector_interface_row_handle row, double* out_max_scroll) {
    if (out_max_scroll == nullptr) return interfaceFail(nector_invalid_argument, "out_max_scroll is null");
    *out_max_scroll = 0;
    if (!rowValid(row)) return interfaceFail(nector_invalid_argument, "row is invalid");
    InterfaceGuard guard(row->context);
    *out_max_scroll = row->max_scroll;
    return nector_ok;
}

nector_status nectorInterfaceRowGetPart(nector_interface_row_handle row, nector_interface_row_part part,
                                         nector_node_handle* out_node) {
    if (out_node == nullptr) return interfaceFail(nector_invalid_argument, "out_node is null");
    *out_node = nullptr;
    if (!rowValid(row) || part < nector_interface_row_node || part > nector_interface_row_forward) {
        return interfaceFail(nector_invalid_argument, "row or row part is invalid");
    }
    InterfaceGuard guard(row->context);
    switch (part) {
        case nector_interface_row_node: *out_node = row->node; break;
        case nector_interface_row_viewport: *out_node = row->viewport; break;
        case nector_interface_row_content: *out_node = row->content; break;
        case nector_interface_row_backward: *out_node = row->backward; break;
        case nector_interface_row_forward: *out_node = row->forward; break;
        default: return interfaceFail(nector_invalid_argument, "row part is invalid");
    }
    return nector_ok;
}
