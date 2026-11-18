include_guard()

get_filename_component(_NECTOR_PACKAGE_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(_NECTOR_LIBRARY "${_NECTOR_PACKAGE_PREFIX}/library")
set(_NECTOR_INCLUDE "${_NECTOR_LIBRARY}/include")

if(WIN32)
    set(_NECTOR_LIBRARY_PREFIX "")
    set(_NECTOR_SHARED_SUFFIX ".dll")
elseif(APPLE)
    set(_NECTOR_LIBRARY_PREFIX "lib")
    set(_NECTOR_SHARED_SUFFIX ".dylib")
else()
    set(_NECTOR_LIBRARY_PREFIX "lib")
    set(_NECTOR_SHARED_SUFFIX ".so")
endif()

function(_nector_add_imported_target target stem)
    if(TARGET Nector::${target})
        return()
    endif()

    set(location "${_NECTOR_LIBRARY}/${_NECTOR_LIBRARY_PREFIX}${stem}${_NECTOR_SHARED_SUFFIX}")
    if(NOT EXISTS "${location}")
        message(FATAL_ERROR "Nector library is missing: ${location}")
    endif()

    add_library(Nector::${target} SHARED IMPORTED)
    set_target_properties(Nector::${target} PROPERTIES
        IMPORTED_LOCATION "${location}"
        INTERFACE_INCLUDE_DIRECTORIES "${_NECTOR_INCLUDE}")
    if(WIN32)
        set_target_properties(Nector::${target} PROPERTIES
            IMPORTED_IMPLIB "${_NECTOR_LIBRARY}/static/${stem}.dll.a")
    endif()
endfunction()

_nector_add_imported_target(nector Nector)
_nector_add_imported_target(nector_dsl_compile Nector.DslCompile)
_nector_add_imported_target(nector_core Nector.Core)
_nector_add_imported_target(nector_rasterization Nector.Rasterization)
_nector_add_imported_target(nector_font Nector.Font)
_nector_add_imported_target(nector_image Nector.Image)
_nector_add_imported_target(nector_nimation NImation)
_nector_add_imported_target(nector_input Nector.Input)
_nector_add_imported_target(nector_scene Nector.Scene)
_nector_add_imported_target(nector_audio Nector.Audio)
_nector_add_imported_target(nector_platform Nector.Platform)
_nector_add_imported_target(nector_interface Nector.Interface)

set_property(TARGET Nector::nector_dsl_compile APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector)
set_property(TARGET Nector::nector_rasterization APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector_core)
set_property(TARGET Nector::nector_font APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector_core Nector::nector_rasterization)
set_property(TARGET Nector::nector_image APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector_core Nector::nector_rasterization)
set_property(TARGET Nector::nector_nimation APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector_core Nector::nector_image)
set_property(TARGET Nector::nector_input APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector_core)
set_property(TARGET Nector::nector_scene APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    Nector::nector_core Nector::nector_rasterization Nector::nector_font Nector::nector_image
    Nector::nector_nimation Nector::nector_input)
set_property(TARGET Nector::nector_audio APPEND PROPERTY INTERFACE_LINK_LIBRARIES Nector::nector_core)
set_property(TARGET Nector::nector_platform APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    Nector::nector_core Nector::nector_rasterization Nector::nector_input Nector::nector_scene)
set_property(TARGET Nector::nector_interface APPEND PROPERTY INTERFACE_LINK_LIBRARIES
    Nector::nector_core Nector::nector_scene)

unset(_NECTOR_INCLUDE)
unset(_NECTOR_LIBRARY)
unset(_NECTOR_LIBRARY_PREFIX)
unset(_NECTOR_PACKAGE_PREFIX)
unset(_NECTOR_SHARED_SUFFIX)
