#ifndef nector_platform_h
#define nector_platform_h

#include "scene.h"

#if defined(_WIN32)
  #if defined(nector_platform_export)
    #define nector_platform_api __declspec(dllexport)
  #else
    #define nector_platform_api __declspec(dllimport)
  #endif
#else
  #define nector_platform_api __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nector_application nector_application;
typedef struct nector_window nector_window;
typedef nector_application* nector_application_handle;
typedef nector_window* nector_window_handle;

nector_platform_api nector_status nectorCreateApplication(nector_application_handle* out_application);
nector_platform_api void nectorDestroyApplication(nector_application_handle application);
nector_platform_api nector_status nectorCreateWindow(nector_application_handle application, nector_scene_handle scene, nector_window_handle* out_window);
nector_platform_api void nectorDestroyWindow(nector_window_handle window);
nector_platform_api nector_status nectorWindowShow(nector_window_handle window);
nector_platform_api nector_status nectorWindowHide(nector_window_handle window);
nector_platform_api nector_status nectorWindowRequestClose(nector_window_handle window);
nector_platform_api nector_status nectorWindowGetInput(nector_window_handle window, nector_input_handle* out_input);
nector_platform_api nector_status nectorWindowGetScene(nector_window_handle window, nector_scene_handle* out_scene);
nector_platform_api nector_status nectorWindowGetSize(nector_window_handle window, int32_t* out_width, int32_t* out_height);
nector_platform_api nector_status nectorWindowSetTitle(nector_window_handle window, const char* title);
nector_platform_api nector_status nectorWindowRender(nector_window_handle window);
nector_platform_api nector_status nectorProcessEvents(nector_application_handle application);
nector_platform_api nector_status nectorRun(nector_application_handle application);
nector_platform_api nector_status nectorQuit(nector_application_handle application);
nector_platform_api nector_status nectorApplicationIsRunning(nector_application_handle application, int32_t* out_running);
nector_platform_api const char* nectorPlatformName(void);

#ifdef __cplusplus
}
#endif
#endif
