#ifndef nector_interface_control_factory_h
#define nector_interface_control_factory_h

#include "interface_state.h"

namespace interface_internal {

class SceneNodeCleanup {
public:
    SceneNodeCleanup(nector_interface_context_handle context, nector_node_handle node);
    SceneNodeCleanup(const SceneNodeCleanup&) = delete;
    SceneNodeCleanup& operator=(const SceneNodeCleanup&) = delete;
    ~SceneNodeCleanup();
    void release();

private:
    nector_interface_context_handle context_;
    nector_node_handle node_;
};

nector_status parentNode(nector_interface_context_handle context,
                         nector_interface_control_handle parent,
                         nector_node_handle* out_parent);
nector_status attachControl(nector_interface_context_handle context,
                            nector_interface_control_handle parent,
                            nector_node_handle node, ControlKind kind,
                            nector_interface_control_handle* out_control);
nector_status createControl(nector_interface_context_handle context,
                            nector_interface_control_handle parent,
                            nector_node_type type, const char* id,
                            nector_interface_control_handle* out_control);
nector_status createButton(nector_interface_context_handle context,
                           nector_interface_control_handle parent,
                           const char* id, const char* text,
                           nector_interface_button_handle* out_button);

} // namespace interface_internal

#endif
