file(GLOB MSHIDKMDF_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

nt_add_driver(mshidkmdf ${MSHIDKMDF_SOURCES} mshidkmdf.rc)

target_link_libraries(mshidkmdf PRIVATE mdr_shim WDK::hidclass)
