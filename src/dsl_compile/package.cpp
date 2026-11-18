#include "dsl_comp.h"
#include "../core/internal.h"
#include "compiler.h"
#include "package_schema.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using namespace nector_cdsl_detail;

struct PackageData : PackageRecords {
    std::unordered_map<std::string, size_t> action_indices;
};

bool parseData(const uint8_t* data, size_t size, size_t absolute_offset, PackageData& package, std::string& error) {
    return readDataSection(data, size, absolute_offset, package, error);
}

bool parseText(const uint8_t* data, size_t size, size_t absolute_offset, PackageData& package, std::string& error) {
    if (!readTextSection(data, size, absolute_offset, package, error)) return false;
    for (size_t index = 0; index < package.actions.size(); ++index) {
        package.action_indices.emplace(package.actions[index].id, index);
    }
    return true;
}

bool parsePackage(const uint8_t* data, size_t size, PackageData& package, std::string& error) {
    PackageLayout layout;
    if (!readPackageLayout(data, size, layout, error)) return false;
    return parseData(data + layout.data_offset, layout.data_size, layout.data_offset, package, error) &&
           parseText(data + layout.text_offset, layout.text_size, layout.text_offset, package, error);
}

double rootNumber(const PackageData& package, nector_node_property property, double fallback) {
    for (const auto& item : package.nodes.front().properties) {
        if (item.property == property && item.value.type == ValueType::number) return item.value.number;
    }
    return fallback;
}

std::string rootString(const PackageData& package, nector_node_property property, const std::string& fallback) {
    for (const auto& item : package.nodes.front().properties) {
        if (item.property == property && item.value.type == ValueType::string) return item.value.string;
    }
    return fallback;
}

bool readFile(const char* path, std::vector<uint8_t>& bytes, std::string& error) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        error = "cannot open input file";
        return false;
    }
    const std::streamoff end = stream.tellg();
    if (end < 0 || static_cast<uint64_t>(end) > max_package_size) {
        error = "input file exceeds 64 MiB";
        return false;
    }
    bytes.resize(static_cast<size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), end)) {
        error = "cannot read input file";
        return false;
    }
    return true;
}

nector_status failFormat(const std::string& error) {
    if (error.find("unsupported") != std::string::npos) return nectorFail(nector_unsupported, error.c_str());
    return nectorFail(nector_invalid_format, error.c_str());
}

} // namespace

struct nector_package {
    PackageData data;
    nector_scene_handle scene = nullptr;
    nector_scene_callback callback = nullptr;
    void* user_data = nullptr;
    std::unordered_map<nector_node_handle, uint32_t> handles;
    int call_depth = 0;
    bool executing = false;

    ~nector_package() {
        if (scene != nullptr) nectorDestroyScene(scene);
    }

    nector_status execute(size_t action_index) {
        if (action_index >= data.actions.size()) return nectorFail(nector_not_found, "action ID was not found");
        if (call_depth >= 64) return nectorFail(nector_callback_error, "action callback recursion limit exceeded");
        struct DepthGuard {
            int& depth;
            explicit DepthGuard(int& value) : depth(value) { ++depth; }
            ~DepthGuard() { --depth; }
        } guard(call_depth);
        const auto& action = data.actions[action_index];
        executing = true;
        for (const auto& operation : action.operations) {
            nector_status status = nector_ok;
            if (operation.opcode == Operation::emit) {
                status = nectorSceneEmit(scene, operation.name.c_str(), operation.text.c_str(), operation.value);
            } else {
                if (operation.node >= data.nodes.size()) return nectorFail(nector_invalid_format, "action node index is invalid");
                const nector_node_handle node = data.nodes[operation.node].handle;
                switch (operation.opcode) {
                    case Operation::set_string: status = nectorSceneSetNodeString(node, operation.property.property, operation.property.value.string.c_str()); break;
                    case Operation::set_number: status = nectorSceneSetNodeNumber(node, operation.property.property, operation.property.value.number); break;
                    case Operation::set_boolean: status = nectorSceneSetNodeBool(node, operation.property.property, operation.property.value.boolean ? 1 : 0); break;
                    case Operation::set_color: status = nectorSceneSetNodeColor(node, operation.property.property, operation.property.value.color); break;
                    case Operation::emit: break;
                }
            }
            if (status != nector_ok) {
                executing = false;
                return status;
            }
        }
        executing = false;
        return nector_ok;
    }

    void dispatch(const nector_ui_event* event) {
        if (event == nullptr) return;
        uint32_t node_index = 0;
        if (event->node != nullptr) {
            const auto found = handles.find(event->node);
            if (found == handles.end()) return;
            node_index = found->second;
        }
        for (const auto& binding : data.bindings) {
            if (binding.node == node_index && binding.event == event->type) {
                const auto action = data.action_indices.find(binding.action);
                if (action != data.action_indices.end()) {
                    const nector_status status = execute(action->second);
                    if (status != nector_ok) return;
                }
            }
        }
        if (callback != nullptr) {
            try {
                callback(event, user_data);
            } catch (...) {
                nectorCoreSetError(nector_callback_error, "scene callback threw an exception");
            }
        }
    }
};

namespace {
void packageCallback(const nector_ui_event* event, void* user_data) {
    static_cast<nector_package*>(user_data)->dispatch(event);
}
}

nector_status nectorCdslCompileText(const char* utf8_source, uint8_t** out_data, size_t* out_size) {
    if (utf8_source == nullptr || out_data == nullptr || out_size == nullptr) return nectorFail(nector_invalid_argument, "compile argument is null");
    *out_data = nullptr;
    *out_size = 0;
    nector_api_try {
        std::vector<uint8_t> output;
        std::string error;
        if (!nector_cdsl_detail::compileSource(utf8_source, output, error)) return failFormat(error);
        std::unique_ptr<uint8_t, decltype(&std::free)> buffer(static_cast<uint8_t*>(std::malloc(output.size())), &std::free);
        if (!buffer) return nectorFail(nector_out_of_memory, "cannot allocate compiled package");
        if (!output.empty()) std::memcpy(buffer.get(), output.data(), output.size());
        *out_size = output.size();
        *out_data = buffer.release();
        return nector_ok;
    } nector_api_catch
}

nector_status nectorCdslCompileFile(const char* input_path, const char* output_path) {
    if (input_path == nullptr || input_path[0] == '\0' || output_path == nullptr || output_path[0] == '\0') {
        return nectorFail(nector_invalid_argument, "compile path is null or empty");
    }
    nector_api_try {
        std::vector<uint8_t> source;
        std::string error;
        if (!readFile(input_path, source, error)) return nectorFail(nector_io_error, error.c_str());
        std::string text(source.begin(), source.end());
        if (text.find('\0') != std::string::npos) return nectorFail(nector_invalid_format, "DSL source contains a NUL byte");
        std::vector<uint8_t> output;
        if (!nector_cdsl_detail::compileSource(text.c_str(), output, error)) return failFormat(error);
        std::ofstream stream(output_path, std::ios::binary | std::ios::trunc);
        if (!stream) return nectorFail(nector_io_error, "cannot open output package");
        if (!output.empty()) stream.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()));
        stream.close();
        if (!stream) return nectorFail(nector_io_error, "cannot write complete output package");
        return nector_ok;
    } nector_api_catch
}

void nectorCdslFreeBuffer(uint8_t* data) {
    std::free(data);
}

nector_status nectorCdslLoadMemory(const uint8_t* data, size_t size, nector_package_handle* out_package) {
    if (out_package == nullptr) return nectorFail(nector_invalid_argument, "out_package is null");
    *out_package = nullptr;
    nector_api_try {
        std::unique_ptr<nector_package> package = std::make_unique<nector_package>();
        std::string error;
        if (!parsePackage(data, size, package->data, error)) return failFormat(error);
        const double width_value = rootNumber(package->data, nector_property_width, 640);
        const double height_value = rootNumber(package->data, nector_property_height, 480);
        const std::string title = rootString(package->data, nector_property_title, "Nector");
        nector_status status = nectorCreateScene(title.c_str(), static_cast<int32_t>(width_value), static_cast<int32_t>(height_value), &package->scene);
        if (status != nector_ok) return status;
        status = nectorSceneGetRoot(package->scene, &package->data.nodes[0].handle);
        if (status != nector_ok) return status;
        package->handles.emplace(package->data.nodes[0].handle, 0);
        for (size_t index = 1; index < package->data.nodes.size(); ++index) {
            auto& node = package->data.nodes[index];
            if (node.parent < 0 || static_cast<size_t>(node.parent) >= index) return nectorFail(nector_invalid_format, "node parent order is invalid");
            status = nectorSceneAddNode(package->scene, package->data.nodes[static_cast<size_t>(node.parent)].handle,
                                        node.type, node.id.c_str(), &node.handle);
            if (status != nector_ok) return status;
            package->handles.emplace(node.handle, static_cast<uint32_t>(index));
        }
        for (auto& node : package->data.nodes) {
            for (const auto& property : node.properties) {
                switch (property.value.type) {
                    case ValueType::string: status = nectorSceneSetNodeString(node.handle, property.property, property.value.string.c_str()); break;
                    case ValueType::number: status = nectorSceneSetNodeNumber(node.handle, property.property, property.value.number); break;
                    case ValueType::boolean: status = nectorSceneSetNodeBool(node.handle, property.property, property.value.boolean ? 1 : 0); break;
                    case ValueType::color: status = nectorSceneSetNodeColor(node.handle, property.property, property.value.color); break;
                }
                if (status != nector_ok) return status;
            }
        }
        status = nectorSceneSetEventCallback(package->scene, packageCallback, package.get());
        if (status != nector_ok) return status;
        *out_package = package.release();
        return nector_ok;
    } nector_api_catch
}

nector_status nectorCdslLoadFile(const char* path, nector_package_handle* out_package) {
    if (path == nullptr || path[0] == '\0' || out_package == nullptr) return nectorFail(nector_invalid_argument, "package path or output is null");
    *out_package = nullptr;
    nector_api_try {
        std::vector<uint8_t> bytes;
        std::string error;
        if (!readFile(path, bytes, error)) return nectorFail(nector_io_error, error.c_str());
        return nectorCdslLoadMemory(bytes.data(), bytes.size(), out_package);
    } nector_api_catch
}

void nectorCdslDestroyPackage(nector_package_handle package) {
    delete package;
}

nector_status nectorCdslGetScene(nector_package_handle package, nector_scene_handle* out_scene) {
    if (package == nullptr || out_scene == nullptr) return nectorFail(nector_invalid_argument, "package or output is null");
    *out_scene = package->scene;
    return nector_ok;
}

nector_status nectorCdslCall(nector_package_handle package, const char* action_id) {
    if (package == nullptr || action_id == nullptr || action_id[0] == '\0') return nectorFail(nector_invalid_argument, "package or action ID is invalid");
    const auto found = package->data.action_indices.find(action_id);
    if (found == package->data.action_indices.end()) return nectorFail(nector_not_found, "action ID was not found");
    nector_api_try {
        return package->execute(found->second);
    } nector_api_catch
}

nector_status nectorCdslHasAction(nector_package_handle package, const char* action_id, int32_t* out_found) {
    if (package == nullptr || action_id == nullptr || out_found == nullptr) return nectorFail(nector_invalid_argument, "action query argument is null");
    *out_found = package->data.action_indices.find(action_id) != package->data.action_indices.end() ? 1 : 0;
    return nector_ok;
}

nector_status nectorCdslGetActionCount(nector_package_handle package, int32_t* out_count) {
    if (package == nullptr || out_count == nullptr) return nectorFail(nector_invalid_argument, "package or output is null");
    if (package->data.actions.size() > static_cast<size_t>(std::numeric_limits<int32_t>::max())) return nectorFail(nector_limit_exceeded, "action count exceeds API range");
    *out_count = static_cast<int32_t>(package->data.actions.size());
    return nector_ok;
}

nector_status nectorCdslGetActionId(nector_package_handle package, int32_t index, const char** out_id) {
    if (package == nullptr || out_id == nullptr || index < 0 || static_cast<size_t>(index) >= package->data.actions.size()) {
        return nectorFail(nector_invalid_argument, "action index or output is invalid");
    }
    *out_id = package->data.actions[static_cast<size_t>(index)].id.c_str();
    return nector_ok;
}

nector_status nectorCdslSetEventCallback(nector_package_handle package, nector_scene_callback callback, void* user_data) {
    if (package == nullptr) return nectorFail(nector_invalid_argument, "package is null");
    package->callback = callback;
    package->user_data = user_data;
    return nector_ok;
}
