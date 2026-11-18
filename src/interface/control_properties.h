#ifndef nector_interface_control_properties_h
#define nector_interface_control_properties_h

#include "interface_state.h"

namespace interface_internal {

nector_status setControlString(nector_interface_control_handle control,
                               nector_node_property property, const char* value);
nector_status setControlNumber(nector_interface_control_handle control,
                               nector_node_property property, double value);
nector_status setControlBool(nector_interface_control_handle control,
                             nector_node_property property, int32_t value);
nector_status setControlColor(nector_interface_control_handle control,
                              nector_node_property property, nector_color value);
nector_status getControlString(nector_interface_control_handle control,
                               nector_node_property property, const char** out_value);
nector_status getControlNumber(nector_interface_control_handle control,
                               nector_node_property property, double* out_value);
nector_status getControlBool(nector_interface_control_handle control,
                             nector_node_property property, int32_t* out_value);
nector_status getControlColor(nector_interface_control_handle control,
                              nector_node_property property, nector_color* out_value);

} // namespace interface_internal

#endif
