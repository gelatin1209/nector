#include "control_properties.h"

#include "row_layout.h"

namespace interface_internal {

#define interface_setter(function_name, scene_function, value_type) \
    nector_status function_name(nector_interface_control_handle control, nector_node_property property, value_type value) { \
        const nector_status status = scene_function(control->node, property, value); \
        if (status != nector_ok) return status; \
        return refreshControls(control->context); \
    }

interface_setter(setControlString, nectorSceneSetNodeString, const char*)
interface_setter(setControlBool, nectorSceneSetNodeBool, int32_t)
interface_setter(setControlColor, nectorSceneSetNodeColor, nector_color)

nector_status setControlNumber(nector_interface_control_handle control,
                               nector_node_property property, double value) {
    if (control->kind == ControlKind::row && property == nector_property_layout) {
        if (value != 0 && value != 1) return interfaceFail(nector_invalid_argument, "layout must be zero or one");
        return setRowLayout(control, static_cast<int32_t>(value));
    }
    const nector_status status = nectorSceneSetNodeNumber(control->node, property, value);
    if (status != nector_ok) return status;
    return refreshControls(control->context);
}

#define interface_getter(function_name, scene_function, value_type) \
    nector_status function_name(nector_interface_control_handle control, nector_node_property property, value_type out_value) { \
        return scene_function(control->node, property, out_value); \
    }

interface_getter(getControlString, nectorSceneGetNodeString, const char**)
interface_getter(getControlNumber, nectorSceneGetNodeNumber, double*)
interface_getter(getControlBool, nectorSceneGetNodeBool, int32_t*)
interface_getter(getControlColor, nectorSceneGetNodeColor, nector_color*)

} // namespace interface_internal
