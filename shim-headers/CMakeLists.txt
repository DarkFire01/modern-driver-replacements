#
# mdr_shim, what ReactOS-derived sources expect from the ReactOS SDK.
#
# Header-only: <debug.h>, <pseh/pseh2.h>, <reactos/version.rc>, plus the
# private interfaces the public WDK does not declare (include/): the
# <drivers/usb3/...> hub/UCX contracts and the wdfldr class-library ABI.
#
add_library(mdr_shim INTERFACE)
target_include_directories(mdr_shim INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/wdf")
target_compile_definitions(mdr_shim INTERFACE
    MDR_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    MDR_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    MDR_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    MDR_VERSION_STR="${PROJECT_VERSION}")
target_compile_options(mdr_shim INTERFACE
    "$<$<COMPILE_LANGUAGE:C,CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/mdr_compat.h>")
target_link_libraries(mdr_shim INTERFACE nt_kernel_settings)

# UCX 1.6, the version usbxhci and usbhub3 are written against.
set(UCX_VERSION "1.6" CACHE STRING "UCX version (km/ucx/<ver> in the WDK)")

add_library(ucx_headers INTERFACE)
target_include_directories(ucx_headers SYSTEM INTERFACE
    "${WDK_ROOT}/Include/${WDK_INCLUDE_VERSION}/km/ucx/${UCX_VERSION}")

add_library(WDK::ucxstub INTERFACE IMPORTED GLOBAL)
set_target_properties(WDK::ucxstub PROPERTIES
    INTERFACE_LINK_LIBRARIES "${WDK_LIBRARY_DIR}/ucx/${UCX_VERSION}/ucxstub.lib")

# SPB 1.1, the version spbcx serves and its clients are written against.
set(SPB_VERSION "1.1" CACHE STRING "SpbCx version (km/spb/<ver> in the WDK)")

add_library(spb_headers INTERFACE)
target_include_directories(spb_headers SYSTEM INTERFACE
    "${WDK_ROOT}/Include/${WDK_INCLUDE_VERSION}/km/spb/${SPB_VERSION}")
