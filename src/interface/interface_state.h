#ifndef nector_interface_state_h
#define nector_interface_state_h

#include "interface.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace interface_internal {

enum class ControlKind {
    generic,
    row,
    button
};

} // namespace interface_internal

struct nector_interface_control {
    nector_interface_context_handle context = nullptr;
    nector_interface_control_handle parent = nullptr;
    interface_internal::ControlKind kind = interface_internal::ControlKind::generic;
    nector_node_handle node = nullptr;
    nector_node_handle viewport = nullptr;
    nector_node_handle content = nullptr;
    nector_node_handle backward = nullptr;
    nector_node_handle forward = nullptr;
    nector_interface_event_callback callback = nullptr;
    void* user_data = nullptr;
    int32_t layout = 0;
    double scroll = 0;
    double max_scroll = 0;
    bool alive = true;
};

struct nector_interface_context {
    nector_scene_handle scene = nullptr;
    std::vector<std::unique_ptr<nector_interface_control>> controls;
    std::vector<std::unique_ptr<nector_interface_control>> retired_controls;
    nector_interface_event_callback callback = nullptr;
    void* user_data = nullptr;
    uint32_t operation_depth = 0;
    bool destroy_pending = false;
    bool refreshing = false;
};

namespace interface_internal {

nector_status interfaceFail(nector_status status, const char* message);
bool contextValid(nector_interface_context_handle context);
bool controlValid(nector_interface_control_handle control);
bool rowValid(nector_interface_row_handle row);

} // namespace interface_internal

#define interface_api_try try
#define interface_api_catch \
    catch (const std::bad_alloc&) { return interface_internal::interfaceFail(nector_out_of_memory, "out of memory"); } \
    catch (const std::exception& exception) { return interface_internal::interfaceFail(nector_backend_error, exception.what()); } \
    catch (...) { return interface_internal::interfaceFail(nector_backend_error, "unknown native error"); }

#endif
