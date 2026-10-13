file(GLOB GPIOCLX_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

nt_add_kmdf_driver(gpioclx ${GPIOCLX_SOURCES} gpioclx.rc)

target_link_libraries(gpioclx PRIVATE mdr_shim WDK::ntstrsafe WDK::wdmsec)
