file(GLOB SPBCX_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

nt_add_kmdf_driver(spbcx ${SPBCX_SOURCES} spbcx.rc)

# WDF_DECLARE_CONTEXT_TYPE wants this off when wdmsec.h is not around
target_compile_definitions(spbcx PRIVATE WDF_DEVICE_NO_WDMSEC_H)

target_link_libraries(spbcx PRIVATE mdr_shim spb_headers WDK::ntstrsafe)
