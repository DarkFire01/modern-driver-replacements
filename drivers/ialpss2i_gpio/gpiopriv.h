/*
 * PROJECT:     ReactOS Intel LPSS GPIO controller driver
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Private declarations
 * COPYRIGHT:   Copyright 2026 Justin Miller <justin.miller@reactos.org>
 */

/**
 * @file
 * @brief
 * Intel PCH GPIO, as a GpioClx client.
 *
 * Pads are grouped into communities, each with its own register window, and
 * every pad has a set of configuration registers. A GpioClx bank is one group.
 */

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <gpioclx.h>

/* Registration packet layout this driver fills in */
#define GPIO_REGISTRATION_VERSION 3

/* HOSTSW_OWN position inside a community window, named after the first part to use it */
#define GPIO_COMMUNITY_HOSTSW_OWN_SKL   0x0D0
#define GPIO_COMMUNITY_HOSTSW_OWN_CNL   0x0B0
#define GPIO_COMMUNITY_HOSTSW_OWN_CNL_H 0x0C0
#define GPIO_COMMUNITY_HOSTSW_OWN_MTL_M 0x110
#define GPIO_COMMUNITY_HOSTSW_OWN_MTL_P 0x140
#define GPIO_COMMUNITY_HOSTSW_OWN_ADL_S 0x150
#define GPIO_COMMUNITY_GPI_STATUS       0x100
#define GPIO_COMMUNITY_GPI_ENABLE       0x120
#define GPIO_COMMUNITY_PAD_BASE         0x400

/* Skylake pads have two configuration registers, Cannon Lake onwards four */
#define GPIO_PAD_STRIDE_2REG 8
#define GPIO_PAD_STRIDE_4REG 16

/* PADCFG0 fields */
#define PADCFG0_GPIOTXSTATE         (1UL << 0)
#define PADCFG0_GPIORXSTATE         (1UL << 1)
#define PADCFG0_GPIOTXDIS           (1UL << 8)
#define PADCFG0_GPIORXDIS           (1UL << 9)
#define PADCFG0_PMODE_MASK          (0x7UL << 10)
#define PADCFG0_GPIROUTIOXAPIC      (1UL << 20)
#define PADCFG0_RXINV               (1UL << 23)
#define PADCFG0_RXEVCFG_MASK        (0x3UL << 25)
#define PADCFG0_RXEVCFG_LEVEL       (0x0UL << 25)
#define PADCFG0_RXEVCFG_EDGE        (0x1UL << 25)
#define PADCFG0_RXEVCFG_DISABLED    (0x2UL << 25)
#define PADCFG0_RXEVCFG_EDGE_BOTH   (0x3UL << 25)

/* One Intel pad group */
typedef struct _GPIO_BANK_DESCRIPTOR
{
    UCHAR Community;
    UCHAR HostSwOwnIndex;
    USHORT PadOffset;
    USHORT PinCount;
} GPIO_BANK_DESCRIPTOR, *PGPIO_BANK_DESCRIPTOR;

/* One SoC's pad geometry, picked by hardware id */
typedef struct _GPIO_LAYOUT
{
    PCWSTR HardwareId;
    PCSTR Name;
    const GPIO_BANK_DESCRIPTOR *Banks;
    ULONG BankCount;
    ULONG CommunityCount;
    USHORT HostSwOwnOffset;
    USHORT PadStride;

    /* Bank width reported to GpioClx; at least as wide as the widest group */
    UCHAR PinsPerBank;
} GPIO_LAYOUT, *PGPIO_LAYOUT;

#define GPIO_MAX_COMMUNITIES 8
#define GPIO_MAX_BANKS       24

#define GPIO_POOL_TAG 'oIPG'

/* What one pad needs to survive a power transition */
typedef struct _GPIO_PIN_STATE
{
    ULONG SavedConfig0;
    ULONG SavedConfig1;

    /* Connected or armed at some point since the last cold start */
    BOOLEAN InUse;
    BOOLEAN ContextSaved;
} GPIO_PIN_STATE, *PGPIO_PIN_STATE;

typedef struct _GPIO_BANK_STATE
{
    PGPIO_PIN_STATE Pins;
    ULONG SavedInterruptEnable;
    BOOLEAN ContextSaved;
} GPIO_BANK_STATE, *PGPIO_BANK_STATE;

/* Per controller state, allocated by GpioClx as ControllerContextSize */
typedef struct _GPIO_CONTROLLER
{
    WDFDEVICE Device;

    /* One memory resource per community */
    PUCHAR CommunityBase[GPIO_MAX_COMMUNITIES];
    ULONG CommunityLength[GPIO_MAX_COMMUNITIES];
    ULONG CommunityCount;

    const GPIO_LAYOUT *Layout;
    const GPIO_BANK_DESCRIPTOR *Banks;
    ULONG BankCount;

    GPIO_BANK_STATE BankState[GPIO_MAX_BANKS];

    USHORT TotalPins;
    UCHAR PinsPerBank;
} GPIO_CONTROLLER, *PGPIO_CONTROLLER;

/**
 * @brief
 * The community window a group lives in, or NULL if it is not mapped.
 */
FORCEINLINE
PUCHAR
GpioBankWindow(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId)
{
    const GPIO_BANK_DESCRIPTOR *Bank;

    if (BankId >= Controller->BankCount)
        return NULL;

    Bank = &Controller->Banks[BankId];
    if (Bank->Community >= Controller->CommunityCount)
        return NULL;

    return Controller->CommunityBase[Bank->Community];
}

/**
 * @brief
 * The PADCFG0 register of one pad, or NULL if the pad does not exist.
 */
FORCEINLINE
PULONG
GpioPadAddress(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId,
    _In_ PIN_NUMBER PinNumber)
{
    PUCHAR Window;

    Window = GpioBankWindow(Controller, BankId);
    if (Window == NULL || PinNumber >= Controller->Banks[BankId].PinCount)
        return NULL;

    return (PULONG)(Window +
                    GPIO_COMMUNITY_PAD_BASE +
                    Controller->Banks[BankId].PadOffset +
                    (ULONG)PinNumber * Controller->Layout->PadStride);
}

/**
 * @brief
 * The dword covering a group in one of a community's per group arrays.
 */
FORCEINLINE
PULONG
GpioGroupRegister(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId,
    _In_ ULONG BlockOffset)
{
    PUCHAR Window;

    Window = GpioBankWindow(Controller, BankId);
    if (Window == NULL)
        return NULL;

    return (PULONG)(Window + BlockOffset +
                    (ULONG)Controller->Banks[BankId].HostSwOwnIndex * sizeof(ULONG));
}

/**
 * @brief
 * The saved state of one pad, or NULL.
 */
FORCEINLINE
PGPIO_PIN_STATE
GpioPinState(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId,
    _In_ PIN_NUMBER PinNumber)
{
    if (BankId >= Controller->BankCount ||
        PinNumber >= Controller->Banks[BankId].PinCount ||
        Controller->BankState[BankId].Pins == NULL)
    {
        return NULL;
    }

    return &Controller->BankState[BankId].Pins[PinNumber];
}

EVT_WDF_DRIVER_DEVICE_ADD GpioEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP GpioEvtDriverCleanup;

GPIO_CLIENT_PREPARE_CONTROLLER GpioPrepareController;
GPIO_CLIENT_RELEASE_CONTROLLER GpioReleaseController;
GPIO_CLIENT_START_CONTROLLER GpioStartController;
GPIO_CLIENT_STOP_CONTROLLER GpioStopController;
GPIO_CLIENT_QUERY_CONTROLLER_BASIC_INFORMATION GpioQueryControllerBasicInformation;

GPIO_CLIENT_CONNECT_IO_PINS GpioConnectIoPins;
GPIO_CLIENT_DISCONNECT_IO_PINS GpioDisconnectIoPins;
GPIO_CLIENT_READ_PINS GpioReadGpioPins;
GPIO_CLIENT_WRITE_PINS GpioWriteGpioPins;

GPIO_CLIENT_ENABLE_INTERRUPT GpioEnableInterrupt;
GPIO_CLIENT_DISABLE_INTERRUPT GpioDisableInterrupt;
GPIO_CLIENT_UNMASK_INTERRUPT GpioUnmaskInterrupt;
GPIO_CLIENT_MASK_INTERRUPTS GpioMaskInterrupts;
GPIO_CLIENT_QUERY_ACTIVE_INTERRUPTS GpioQueryActiveInterrupts;
GPIO_CLIENT_CLEAR_ACTIVE_INTERRUPTS GpioClearActiveInterrupts;

GPIO_CLIENT_RECONFIGURE_INTERRUPT GpioReconfigureInterrupt;
GPIO_CLIENT_SAVE_BANK_HARDWARE_CONTEXT GpioSaveBankHardwareContext;
GPIO_CLIENT_RESTORE_BANK_HARDWARE_CONTEXT GpioRestoreBankHardwareContext;
