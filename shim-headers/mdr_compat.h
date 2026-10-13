/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Constants the ReactOS SDK defines and the WDK does not
 *
 * Force-included ahead of every source, so macros only: no types exist yet.
 */

#pragma once

/* ntddk.h declares PNP_LOCATION_INTERFACE but not its version */
#define PNP_LOCATION_INTERFACE_VERSION 1

/* ReactOS's usbioctl.h exposes the hub port status query outside the internal set */
#define IOCTL_USB_GET_PORT_STATUS \
    CTL_CODE(FILE_DEVICE_USB, USB_GET_PORT_STATUS, METHOD_BUFFERED, FILE_ANY_ACCESS)

/* srbhelper.h asserts on it at every NTDDI, storport.h only defines it from Win11 GE */
#define STOR_ADDRESS_TYPE_NVME 0x2
