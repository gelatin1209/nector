#include "dsl_comp.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void check(nector_status status) {
    if (status != nector_ok) throw std::runtime_error(nectorGetLastError());
}
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void onEvent(const nector_ui_event* event, void* user_data) {
    if (event->type == nector_ui_custom) ++*static_cast<int*>(user_data);
}
}

int main() {
    try {
        const char source[] =
            "window \"main\" { title=\"DSL test\"; width=320; height=240; padding=8;"
            " label \"status\" { text=\"before\"; } button \"go\" { text=\"Go\"; on_click=\"run\"; } }"
            " action \"run\" { set \"status\".text=\"after\"; set \"status\".font_size=22;"
            " emit \"done\", \"payload\", 42; }";
        uint8_t* compiled = nullptr;
        size_t compiled_size = 0;
        check(nectorCdslCompileText(source, &compiled, &compiled_size));
        std::unique_ptr<uint8_t, decltype(&nectorCdslFreeBuffer)> buffer(compiled, nectorCdslFreeBuffer);
        require(compiled_size > 100 && std::memcmp(compiled, "NECTOR", 6) == 0, "package magic is missing");
        require(std::memcmp(compiled + 16, "Data Section", 12) == 0, "Data Section marker is missing");
        nector_package_handle package = nullptr;
        check(nectorCdslLoadMemory(compiled, compiled_size, &package));
        std::unique_ptr<nector_package, decltype(&nectorCdslDestroyPackage)> package_guard(package, nectorCdslDestroyPackage);
        int32_t action_count = 0;
        check(nectorCdslGetActionCount(package, &action_count));
        require(action_count == 1, "wrong action count");
        int32_t found = 0;
        check(nectorCdslHasAction(package, "run", &found));
        require(found == 1, "compiled action was not found");
        nector_scene_handle scene = nullptr;
        check(nectorCdslGetScene(package, &scene));
        nector_node_handle status = nullptr;
        check(nectorSceneFindNode(scene, "status", &status));
        int custom_events = 0;
        check(nectorCdslSetEventCallback(package, onEvent, &custom_events));
        check(nectorCdslCall(package, "run"));
        const char* text = nullptr;
        check(nectorSceneGetNodeString(status, nector_property_text, &text));
        require(std::strcmp(text, "after") == 0, "action did not update label text");
        double font_size = 0;
        check(nectorSceneGetNodeNumber(status, nector_property_font_size, &font_size));
        require(font_size == 22 && custom_events == 1, "action property or emit result is wrong");
        require(nectorCdslCall(package, "missing") == nector_not_found, "missing action was not reported");

        std::vector<uint8_t> damaged(compiled, compiled + compiled_size);
        damaged[45] ^= 0x80;
        nector_package_handle rejected = nullptr;
        require(nectorCdslLoadMemory(damaged.data(), damaged.size(), &rejected) == nector_invalid_format && rejected == nullptr,
                "CRC-damaged package was accepted");
        damaged.assign(compiled, compiled + compiled_size - 1);
        require(nectorCdslLoadMemory(damaged.data(), damaged.size(), &rejected) == nector_invalid_format,
                "truncated package was accepted");
        damaged.assign(compiled, compiled + compiled_size);
        damaged.push_back(0);
        require(nectorCdslLoadMemory(damaged.data(), damaged.size(), &rejected) == nector_invalid_format,
                "trailing package data was accepted");
        uint8_t* invalid_output = reinterpret_cast<uint8_t*>(1);
        size_t invalid_size = 99;
        require(nectorCdslCompileText("window \"a\" { label \"x\" { text=1; } }", &invalid_output, &invalid_size) == nector_invalid_format,
                "wrong property type was accepted");
        require(invalid_output == nullptr && invalid_size == 0, "failed compile returned a buffer");

        {
            std::ofstream stream("build/test.nector", std::ios::binary | std::ios::trunc);
            stream << source;
        }
        check(nectorCdslCompileFile("build/test.nector", "build/test.dll-nector"));
        package_guard.reset();
        check(nectorCdslLoadFile("build/test.dll-nector", &package));
        package_guard.reset(package);
        std::cout << "test_dsl: PASS; bytes=" << compiled_size << "; action run changed text 'before' -> 'after'; custom_events=1\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_dsl: FAIL: " << error.what() << '\n';
        return 1;
    }
}
