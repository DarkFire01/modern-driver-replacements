/*
 * PROJECT:     ReactOS HID Stack
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     HID over I2C: private definitions
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

#pragma once

#include <ntddk.h>
#include <wdf.h>
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <spb.h>
#include <hidport.h>
#include <debug.h>

#define HIDI2C_POOL_TAG 'I2iH'

/* HID descriptor as read from the device, little endian */
#include <pshpack1.h>
typedef struct _HIDI2C_DESCRIPTOR
{
    USHORT DescLength;
    USHORT BcdVersion;
    USHORT ReportDescLength;
    USHORT ReportDescRegister;
    USHORT InputRegister;
    USHORT MaxInputLength;
    USHORT OutputRegister;
    USHORT MaxOutputLength;
    USHORT CommandRegister;
    USHORT DataRegister;
    USHORT VendorId;
    USHORT ProductId;
    USHORT VersionId;
    ULONG Reserved;
} HIDI2C_DESCRIPTOR, *PHIDI2C_DESCRIPTOR;
#include <poppack.h>

#define HIDI2C_DESCRIPTOR_LENGTH        30
#define HIDI2C_BCD_VERSION              0x0100

/* Command register opcodes */
#define HIDI2C_OPCODE_RESET             0x01
#define HIDI2C_OPCODE_SET_POWER         0x08

#define HIDI2C_POWER_ON                 0x00
#define HIDI2C_POWER_SLEEP              0x01

/* ACPI _DSM {3CDFF6F7-4267-4555-AD05-B30A3D8938DE} */
#define HIDI2C_DSM_REVISION             1
#define HIDI2C_DSM_FUNCTION_QUERY       0
#define HIDI2C_DSM_FUNCTION_HID_DESC    1

/* Transfer timeouts, in seconds */
#define HIDI2C_TIMEOUT_DESCRIPTOR       10
#define HIDI2C_TIMEOUT_POWER            5
#define HIDI2C_TIMEOUT_SHORT            1

/* How long the default queue stays stopped waiting for the reset response */
#define HIDI2C_RESET_TIMEOUT_MS         4000

typedef struct _HIDI2C_CONTEXT
{
    WDFDEVICE Device;

    LARGE_INTEGER ConnectionId;
    WDFIOTARGET SpbTarget;

    USHORT DescriptorRegister;
    HIDI2C_DESCRIPTOR Descriptor;

    WDFINTERRUPT Interrupt;
    PUCHAR InputBuffer;
    ULONG InputBufferLength;

    WDFQUEUE DefaultQueue;
    WDFQUEUE ReadQueue;

    /* Nonzero from the RESET command until the device answers or the timer fires */
    volatile LONG ResetPending;
    WDFTIMER ResetTimer;
} HIDI2C_CONTEXT, *PHIDI2C_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(HIDI2C_CONTEXT, HidI2cGetContext)

/* transport.c */

NTSTATUS
NTAPI
HidI2cOpenSpbTarget(
    _In_ PHIDI2C_CONTEXT Context);

VOID
NTAPI
HidI2cCloseSpbTarget(
    _In_ PHIDI2C_CONTEXT Context);

NTSTATUS
NTAPI
HidI2cSpbWrite(
    _In_ PHIDI2C_CONTEXT Context,
    _In_reads_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length,
    _In_ ULONG TimeoutSeconds);

NTSTATUS
NTAPI
HidI2cSpbRead(
    _In_ PHIDI2C_CONTEXT Context,
    _Out_writes_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length,
    _In_ ULONG TimeoutSeconds);

NTSTATUS
NTAPI
HidI2cSpbReadRegister(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ USHORT Register,
    _Out_writes_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length,
    _In_ ULONG TimeoutSeconds);

NTSTATUS
NTAPI
HidI2cAcpiGetDescriptorRegister(
    _In_ PHIDI2C_CONTEXT Context);

/* hid.c */

NTSTATUS
NTAPI
HidI2cStart(
    _In_ PHIDI2C_CONTEXT Context);

VOID
NTAPI
HidI2cStop(
    _In_ PHIDI2C_CONTEXT Context);

NTSTATUS
NTAPI
HidI2cSetPower(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ UCHAR PowerState);

NTSTATUS
NTAPI
HidI2cReset(
    _In_ PHIDI2C_CONTEXT Context);

EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL HidI2cEvtInternalDeviceControl;
EVT_WDF_INTERRUPT_ISR HidI2cEvtInterruptIsr;
EVT_WDF_TIMER HidI2cEvtResetTimer;
