#ifndef nector_core_h
#define nector_core_h

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
  #if defined(nector_core_export)
    #define nector_core_api __declspec(dllexport)
  #else
    #define nector_core_api __declspec(dllimport)
  #endif
#else
  #define nector_core_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum nector_status {
    nector_ok = 0,
    nector_invalid_argument = 1,
    nector_out_of_memory = 2,
    nector_io_error = 3,
    nector_invalid_format = 4,
    nector_unsupported = 5,
    nector_invalid_state = 6,
    nector_not_found = 7,
    nector_limit_exceeded = 8,
    nector_backend_error = 9,
    nector_callback_error = 10
} nector_status;

typedef struct nector_point {
    float x;
    float y;
} nector_point;

typedef struct nector_rect {
    float x;
    float y;
    float width;
    float height;
} nector_rect;

typedef struct nector_color {
    float r;
    float g;
    float b;
    float a;
} nector_color;

typedef struct nector_transform {
    float m00;
    float m01;
    float m02;
    float m10;
    float m11;
    float m12;
} nector_transform;

nector_core_api const char* nectorGetVersion(void);
nector_core_api const char* nectorGetLastError(void);
nector_core_api void nectorClearError(void);
nector_core_api const char* nectorStatusText(nector_status status);
nector_core_api void nectorCoreSetError(nector_status status, const char* message);
nector_core_api nector_status nectorCoreValidateUtf8(const char* text, size_t length);
nector_core_api uint32_t nectorCoreCrc32(const uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif
#endif
