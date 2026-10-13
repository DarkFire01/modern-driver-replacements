add_subdirectory(stornvme)

# The WDK's UCX headers only exist from NTDDI_WINTHRESHOLD on, and its oldest
# KMDF (1.15) first shipped with Windows 10.
if(NT_TARGET_WINNT_VALUE LESS 0x0A00)
    message(STATUS "USB 3 stack skipped: UCX and KMDF ${KMDF_VERSION} need Windows 10 or later")
else()
    add_subdirectory(ucx01000)
    add_subdirectory(usbhub3)
    add_subdirectory(usbxhci)
endif()
