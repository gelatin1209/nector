include_guard()

get_filename_component(Nector_PACKAGE_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(Nector_INCLUDE_DIRS "${Nector_PACKAGE_PREFIX}/library/include")
set(Nector_LIBRARIES Nector::nector)
set(Nector_VERSION "0.1.0")

include("${CMAKE_CURRENT_LIST_DIR}/NectorTargets.cmake")
