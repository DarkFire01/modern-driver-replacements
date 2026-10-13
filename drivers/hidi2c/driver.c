/*
 * PROJECT:     ReactOS HID Stack
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     HID over I2C: driver and PnP
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

#define NDEBUG
#include "hidi2cp.h"

DRIVER_INITIALIZE DriverEntry;
static EVT_WDF_DRIVER_DEVICE_ADD HidI2cEvtDeviceAdd;
static EVT_WDF_DEVICE_PREPARE_HARDWARE HidI2cEvtPrepareHardware;
static EVT_WDF_DEVICE_RELEASE_HARDWARE HidI2cEvtReleaseHardware;
static EVT_WDF_DEVICE_D0_ENTRY HidI2cEvtD0Entry;
static EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED HidI2cEvtD0EntryPostInterruptsEnabled;
static EVT_WDF_DEVICE_D0_EXIT HidI2cEvtD0Exit;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

/**
 * @brief Finds the I2C connection resource, opens it and locates the HID descriptor.
 */
static
NTSTATUS
NTAPI
HidI2cEvtPrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    PHIDI2C_CONTEXT Context = HidI2cGetContext(Device);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Resource;
    BOOLEAN FoundConnection = FALSE;
    ULONG Count;
    ULONG Index;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    PAGED_CODE();

    Count = WdfCmResourceListGetCount(ResourcesTranslated);

    for (Index = 0; Index < Count; Index++)
    {
        Resource = WdfCmResourceListGetDescriptor(ResourcesTranslated, Index);
        if (Resource == NULL || Resource->Type != CmResourceTypeConnection)
            continue;

        if (Resource->u.Connection.Class == CM_RESOURCE_CONNECTION_CLASS_SERIAL &&
            Resource->u.Connection.Type == CM_RESOURCE_CONNECTION_TYPE_SERIAL_I2C)
        {
            Context->ConnectionId.LowPart = Resource->u.Connection.IdLowPart;
            Context->ConnectionId.HighPart = Resource->u.Connection.IdHighPart;
            FoundConnection = TRUE;
        }
    }

    if (!FoundConnection)
    {
        DPRINT1("hidi2c: no I2C connection resource\n");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    Status = HidI2cOpenSpbTarget(Context);
    if (!NT_SUCCESS(Status))
        return Status;

    return HidI2cAcpiGetDescriptorRegister(Context);
}

static
NTSTATUS
NTAPI
HidI2cEvtReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    HidI2cCloseSpbTarget(HidI2cGetContext(Device));
    return STATUS_SUCCESS;
}

/**
 * @brief Reads the descriptor on first start, then powers the device on.
 */
static
NTSTATUS
NTAPI
HidI2cEvtD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    PHIDI2C_CONTEXT Context = HidI2cGetContext(Device);
    NTSTATUS Status;

    if (PreviousState == WdfPowerDeviceD3Final)
    {
        Status = HidI2cStart(Context);
        if (!NT_SUCCESS(Status))
        {
            HidI2cStop(Context);
            return Status;
        }
    }

    Status = HidI2cSetPower(Context, HIDI2C_POWER_ON);
    if (!NT_SUCCESS(Status))
        HidI2cStop(Context);

    return Status;
}

/* The reset response arrives as an interrupt, so RESET waits for them to be live */
static
NTSTATUS
NTAPI
HidI2cEvtD0EntryPostInterruptsEnabled(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    if (PreviousState != WdfPowerDeviceD3Final)
        return STATUS_SUCCESS;

    return HidI2cReset(HidI2cGetContext(Device));
}

static
NTSTATUS
NTAPI
HidI2cEvtD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    PHIDI2C_CONTEXT Context = HidI2cGetContext(Device);
    NTSTATUS Status;

    Status = HidI2cSetPower(Context, HIDI2C_POWER_SLEEP);

    if (!NT_SUCCESS(Status) || TargetState == WdfPowerDeviceD3Final)
        HidI2cStop(Context);

    return Status;
}

static
NTSTATUS
NTAPI
HidI2cCreateQueues(
    _In_ PHIDI2C_CONTEXT Context)
{
    WDF_IO_QUEUE_CONFIG QueueConfig;
    NTSTATUS Status;

    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&QueueConfig, WdfIoQueueDispatchParallel);
    QueueConfig.EvtIoInternalDeviceControl = HidI2cEvtInternalDeviceControl;

    Status = WdfIoQueueCreate(Context->Device,
                              &QueueConfig,
                              WDF_NO_OBJECT_ATTRIBUTES,
                              &Context->DefaultQueue);
    if (!NT_SUCCESS(Status))
        return Status;

    /* Reads wait here for the device to interrupt */
    WDF_IO_QUEUE_CONFIG_INIT(&QueueConfig, WdfIoQueueDispatchManual);

    return WdfIoQueueCreate(Context->Device,
                            &QueueConfig,
                            WDF_NO_OBJECT_ATTRIBUTES,
                            &Context->ReadQueue);
}

static
NTSTATUS
NTAPI
HidI2cEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_PNPPOWER_EVENT_CALLBACKS PnpCallbacks;
    WDF_INTERRUPT_CONFIG InterruptConfig;
    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_TIMER_CONFIG TimerConfig;
    WDF_DEVICE_STATE DeviceState;
    PHIDI2C_CONTEXT Context;
    WDFDEVICE Device;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    /* hidclass above owns power policy for the stack */
    WdfDeviceInitSetPowerPolicyOwnership(DeviceInit, FALSE);

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpCallbacks);
    PnpCallbacks.EvtDevicePrepareHardware = HidI2cEvtPrepareHardware;
    PnpCallbacks.EvtDeviceReleaseHardware = HidI2cEvtReleaseHardware;
    PnpCallbacks.EvtDeviceD0Entry = HidI2cEvtD0Entry;
    PnpCallbacks.EvtDeviceD0EntryPostInterruptsEnabled = HidI2cEvtD0EntryPostInterruptsEnabled;
    PnpCallbacks.EvtDeviceD0Exit = HidI2cEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attributes, HIDI2C_CONTEXT);

    Status = WdfDeviceCreate(&DeviceInit, &Attributes, &Device);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("hidi2c: WdfDeviceCreate failed 0x%08lX\n", Status);
        return Status;
    }

    Context = HidI2cGetContext(Device);
    RtlZeroMemory(Context, sizeof(*Context));
    Context->Device = Device;

    WDF_DEVICE_STATE_INIT(&DeviceState);
    DeviceState.NotDisableable = WdfFalse;
    WdfDeviceSetDeviceState(Device, &DeviceState);

    WDF_TIMER_CONFIG_INIT(&TimerConfig, HidI2cEvtResetTimer);
    TimerConfig.AutomaticSerialization = FALSE;
    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.ParentObject = Device;

    Status = WdfTimerCreate(&TimerConfig, &Attributes, &Context->ResetTimer);
    if (!NT_SUCCESS(Status))
        return Status;

    Status = HidI2cCreateQueues(Context);
    if (!NT_SUCCESS(Status))
        return Status;

    /* The ISR reads the bus synchronously, so it has to run at passive level */
    WDF_INTERRUPT_CONFIG_INIT(&InterruptConfig, HidI2cEvtInterruptIsr, NULL);
    InterruptConfig.PassiveHandling = TRUE;

    Status = WdfInterruptCreate(Device,
                                &InterruptConfig,
                                WDF_NO_OBJECT_ATTRIBUTES,
                                &Context->Interrupt);
    if (!NT_SUCCESS(Status))
        DPRINT1("hidi2c: WdfInterruptCreate failed 0x%08lX\n", Status);

    return Status;
}

NTSTATUS
NTAPI
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG Config;

    WDF_DRIVER_CONFIG_INIT(&Config, HidI2cEvtDeviceAdd);

    return WdfDriverCreate(DriverObject,
                           RegistryPath,
                           WDF_NO_OBJECT_ATTRIBUTES,
                           &Config,
                           WDF_NO_HANDLE);
}
