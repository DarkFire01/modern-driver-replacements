/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     The WDK's <usb.h>, plus what it hides from older NTDDI targets
 *
 * The drivers handle every URB a newer usbd client can send, whichever OS
 * they are built for, so the RS3 isoch path-delay URB is always declared.
 * It is not in the URB union below RS3; reach it by casting the URB.
 */

#pragma once

#include <../shared/usb.h>

#if (NTDDI_VERSION < NTDDI_WIN10_RS3)

#define URB_FUNCTION_GET_ISOCH_PIPE_TRANSFER_PATH_DELAYS 0x003D

struct _URB_GET_ISOCH_PIPE_TRANSFER_PATH_DELAYS {
    struct _URB_HEADER Hdr;
    USBD_PIPE_HANDLE PipeHandle;
    ULONG MaximumSendPathDelayInMilliSeconds;
    ULONG MaximumCompletionPathDelayInMilliSeconds;
};

#endif

/* The WDK spells it INAVLID */
#ifndef USBD_STATUS_INVALID_PIPE_FLAGS
#define USBD_STATUS_INVALID_PIPE_FLAGS USBD_STATUS_INAVLID_PIPE_FLAGS
#endif
