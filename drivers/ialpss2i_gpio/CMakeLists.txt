if(NOT TARGET WDK::msgpioclxstub)
    message(STATUS "ialpss2i_gpio skipped: the WDK has no msgpioclxstub.lib for ${WDK_ARCH}")
    return()
endif()

file(GLOB IALPSS2I_GPIO_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

nt_add_kmdf_driver(ialpss2i_gpio ${IALPSS2I_GPIO_SOURCES} ialpss2i_gpio.rc)

target_link_libraries(ialpss2i_gpio PRIVATE mdr_shim WDK::msgpioclxstub)
