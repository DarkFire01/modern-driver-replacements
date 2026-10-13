include_guard(GLOBAL)

#
# KMDF on top of NtKernel.cmake.
#
# Headers come from Include/wdf/kmdf/<ver> and the loader/entry stubs from
# Lib/wdf/kmdf/<arch>/<ver>, exactly as the WDK's MSBuild targets pick them.
#

set(KMDF_VERSION "1.15" CACHE STRING
    "KMDF version to build against (Include/wdf/kmdf/<ver> in the WDK)")

function(nt_kmdf_init)
    if(TARGET nt_kmdf_settings)
        return()
    endif()

    if(NOT KMDF_VERSION MATCHES "^([0-9]+)\\.([0-9]+)$")
        message(FATAL_ERROR "KMDF_VERSION must look like 1.15, got '${KMDF_VERSION}'")
    endif()
    set(_major "${CMAKE_MATCH_1}")
    set(_minor "${CMAKE_MATCH_2}")

    set(_inc "${WDK_ROOT}/Include/wdf/kmdf/${KMDF_VERSION}")
    set(_lib "${WDK_ROOT}/Lib/wdf/kmdf/${WDK_ARCH}/${KMDF_VERSION}")

    if(NOT EXISTS "${_inc}/wdf.h")
        file(GLOB _have RELATIVE "${WDK_ROOT}/Include/wdf/kmdf" "${WDK_ROOT}/Include/wdf/kmdf/*")
        string(REPLACE ";" ", " _have "${_have}")
        message(FATAL_ERROR "KMDF ${KMDF_VERSION} headers not found in the WDK. Installed: ${_have}")
    endif()
    if(NOT EXISTS "${_lib}/wdfldr.lib")
        message(FATAL_ERROR "KMDF ${KMDF_VERSION} libraries for ${WDK_ARCH} not found at ${_lib}")
    endif()

    set(KMDF_INCLUDE_DIR "${_inc}" CACHE INTERNAL "")
    set(KMDF_LIBRARY_DIR "${_lib}" CACHE INTERNAL "")

    add_library(nt_kmdf_settings INTERFACE)
    target_link_libraries(nt_kmdf_settings INTERFACE nt_kernel_settings)
    target_include_directories(nt_kmdf_settings SYSTEM INTERFACE "${_inc}")
    target_compile_definitions(nt_kmdf_settings INTERFACE
        KMDF_VERSION_MAJOR=${_major}
        KMDF_VERSION_MINOR=${_minor})

    foreach(_name wdfldr wdfdriverentry)
        add_library(WDK::${_name} INTERFACE IMPORTED)
        set_target_properties(WDK::${_name} PROPERTIES
            INTERFACE_LINK_LIBRARIES "${_lib}/${_name}.lib")
    endforeach()

    set(KMDF_VERSION_MAJOR "${_major}" PARENT_SCOPE)
    set(KMDF_VERSION_MINOR "${_minor}" PARENT_SCOPE)
endfunction()

#
# nt_add_kmdf_driver(<name> <sources>...)
#
# A KMDF driver written in C++.  Same link line as nt_add_driver, except the
# image enters through FxDriverEntry, which binds to the framework (and to any
# class extension whose stub is linked in) before calling DriverEntry.
#
function(nt_add_kmdf_driver name)
    nt_add_driver(${name} ENTRY FxDriverEntry ${ARGN})
    target_link_libraries(${name} PRIVATE nt_kmdf_settings WDK::wdfldr WDK::wdfdriverentry)

    set_target_properties(${name} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)

    target_compile_options(${name} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/GR->
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
endfunction()
