file(GLOB UCX_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

nt_add_kmdf_driver(ucx01000 ${UCX_SOURCES} ucx01000.rc)

# WDF_DECLARE_CONTEXT_TYPE wants this off when wdmsec.h is not around
target_compile_definitions(ucx01000 PRIVATE WDF_DEVICE_NO_WDMSEC_H)

target_link_libraries(ucx01000 PRIVATE mdr_shim ucx_headers WDK::ntstrsafe)
