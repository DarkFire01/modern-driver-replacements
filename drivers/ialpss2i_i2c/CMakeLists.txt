file(GLOB IALPSS2I_I2C_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

nt_add_kmdf_driver(ialpss2i_i2c ${IALPSS2I_I2C_SOURCES} ialpss2i_i2c.rc)

# WDF_DECLARE_CONTEXT_TYPE wants this off when wdmsec.h is not around
target_compile_definitions(ialpss2i_i2c PRIVATE WDF_DEVICE_NO_WDMSEC_H)

target_link_libraries(ialpss2i_i2c PRIVATE mdr_shim spb_headers WDK::spbcxstubs WDK::ntstrsafe)
