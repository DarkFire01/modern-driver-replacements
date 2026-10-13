file(GLOB USBHUB3_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

nt_add_kmdf_driver(usbhub3 ${USBHUB3_SOURCES} usbhub3.rc)

# CMake only feeds .def sources to DLL targets; a .sys is an executable to it.
set(_def "${CMAKE_CURRENT_SOURCE_DIR}/usbhub3.def")
target_link_options(usbhub3 PRIVATE "/DEF:${_def}")
set_property(TARGET usbhub3 APPEND PROPERTY LINK_DEPENDS "${_def}")

target_compile_definitions(usbhub3 PRIVATE WDF_DEVICE_NO_WDMSEC_H)

target_link_libraries(usbhub3 PRIVATE
    mdr_shim ucx_headers
    WDK::usbdex WDK::usbd WDK::ntstrsafe WDK::wdmguid)
