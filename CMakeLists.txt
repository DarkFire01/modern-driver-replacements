#   cmake -B build/win10 -G Ninja -DNT_TARGET=win10
#   cmake --build build/win10
cmake_minimum_required(VERSION 3.24)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(modern-driver-replacements
    VERSION 0.1.0
    DESCRIPTION "Modern NT drivers, built for many Windows releases"
    LANGUAGES C CXX)

if(NOT MSVC)
    message(FATAL_ERROR
        "modern-driver-replacements builds kernel-mode images and needs the MSVC "
        "toolchain from a Visual Studio developer environment. Run one of the "
        "scripts in scripts/.")
endif()

include(NtTargets)
include(NtKernel)
include(NtKmdf)

nt_target_list(_nt_all_targets)
string(REPLACE ";" ", " _nt_all_targets_pretty "${_nt_all_targets}")

set(NT_TARGET "win10" CACHE STRING
    "Windows release to build for. One of: ${_nt_all_targets_pretty}")
set_property(CACHE NT_TARGET PROPERTY STRINGS ${_nt_all_targets})

option(NT_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(WDK_REQUESTED_LIBRARIES usbd usbdex storport)
find_package(WDK REQUIRED)
nt_target_info(${NT_TARGET})
nt_kernel_init()
nt_kmdf_init()

message(STATUS "modern-driver-replacements target : ${NT_TARGET_DISPLAY_NAME} (${NT_TARGET_ID})")
message(STATUS "  NTDDI_VERSION : ${NT_TARGET_NTDDI}")
message(STATUS "  _WIN32_WINNT  : ${NT_TARGET_WIN32_WINNT}")
message(STATUS "  subsystem     : NATIVE,${NT_TARGET_SUBSYSTEM_VERSION}")
message(STATUS "  architecture  : ${WDK_ARCH}")
message(STATUS "  KMDF          : ${KMDF_VERSION}")
if(WDK_INCLUDE_VERSION STREQUAL WDK_LIB_VERSION)
    message(STATUS "  WDK           : ${WDK_VERSION} at ${WDK_ROOT}")
else()
    message(STATUS "  WDK headers   : ${WDK_INCLUDE_VERSION} at ${WDK_ROOT}")
    message(STATUS "  WDK libraries : ${WDK_LIB_VERSION} (km/${WDK_ARCH})")
endif()

if(WDK_ARCH STREQUAL "x86" AND NT_TARGET_ID STREQUAL "win11")
    message(WARNING
        "Windows 11 ships no 32-bit kernel; an x86 image built for it will "
        "link but has no OS to load it.")
endif()

# stornvme is written against the Windows 8 storport interface.
math(EXPR NT_TARGET_WINNT_VALUE "${NT_TARGET_WIN32_WINNT}")
if(NT_TARGET_WINNT_VALUE LESS 0x0602)
    message(FATAL_ERROR
        "${NT_TARGET_DISPLAY_NAME} is not a supported target, Windows 8 is the oldest.")
endif()

# Keep each target's artifacts apart, so several NT_TARGETs can share a tree.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(shim-headers)
add_subdirectory(drivers)
