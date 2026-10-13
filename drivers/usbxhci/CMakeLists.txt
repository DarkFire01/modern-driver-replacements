file(GLOB XHCI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

nt_add_kmdf_driver(usbxhci ${XHCI_SOURCES} usbxhci.rc)

target_compile_definitions(usbxhci PRIVATE WDF_DEVICE_NO_WDMSEC_H)

# The WDK's stub fills UcxFunctions through .kmdfclassbind; see shim-headers/ucxstub.h
target_link_libraries(usbxhci PRIVATE mdr_shim ucx_headers WDK::ucxstub WDK::ntstrsafe)
