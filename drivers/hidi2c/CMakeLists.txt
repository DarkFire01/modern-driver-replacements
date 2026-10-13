file(GLOB HIDI2C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

nt_add_kmdf_driver(hidi2c ${HIDI2C_SOURCES} hidi2c.rc)

# WDF_DECLARE_CONTEXT_TYPE wants this off when wdmsec.h is not around
target_compile_definitions(hidi2c PRIVATE WDF_DEVICE_NO_WDMSEC_H)

target_link_libraries(hidi2c PRIVATE mdr_shim WDK::ntstrsafe)
