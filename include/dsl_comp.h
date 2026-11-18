#ifndef nector_cdsl_h
#define nector_cdsl_h

#include "scene.h"

#if defined(_WIN32)
  #if defined(nector_cdsl_export)
    #define nector_cdsl_api __declspec(dllexport)
  #else
    #define nector_cdsl_api __declspec(dllimport)
  #endif
#else
  #define nector_cdsl_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_package nector_package;
typedef nector_package* nector_package_handle;

nector_cdsl_api nector_status nectorCdslCompileText(const char* utf8_source, uint8_t** out_data, size_t* out_size);
nector_cdsl_api nector_status nectorCdslCompileFile(const char* input_path, const char* output_path);
nector_cdsl_api void nectorCdslFreeBuffer(uint8_t* data);
nector_cdsl_api nector_status nectorCdslLoadMemory(const uint8_t* data, size_t size, nector_package_handle* out_package);
nector_cdsl_api nector_status nectorCdslLoadFile(const char* path, nector_package_handle* out_package);
nector_cdsl_api void nectorCdslDestroyPackage(nector_package_handle package);
nector_cdsl_api nector_status nectorCdslGetScene(nector_package_handle package, nector_scene_handle* out_scene);
nector_cdsl_api nector_status nectorCdslCall(nector_package_handle package, const char* action_id);
nector_cdsl_api nector_status nectorCdslHasAction(nector_package_handle package, const char* action_id, int32_t* out_found);
nector_cdsl_api nector_status nectorCdslGetActionCount(nector_package_handle package, int32_t* out_count);
nector_cdsl_api nector_status nectorCdslGetActionId(nector_package_handle package, int32_t index, const char** out_id);
nector_cdsl_api nector_status nectorCdslSetEventCallback(nector_package_handle package, nector_scene_callback callback, void* user_data);

#ifdef __cplusplus
}
#endif
#endif
