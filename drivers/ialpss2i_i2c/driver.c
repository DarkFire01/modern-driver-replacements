/*
 * PROJECT:     ReactOS Simple Peripheral Bus
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Intel LPSS I2C controller driver and SpbCx callbacks
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

#include "i2cpriv.h"

/* initguid first so GUID_BUS_INTERFACE_STANDARD gets storage here */
#include <initguid.h>
#include <wdmguid.h>

#define NDEBUG
#include <debug.h>

DRIVER_INITIALIZE DriverEntry;
static EVT_WDF_DRIVER_DEVICE_ADD I2cEvtDeviceAdd;
static EVT_WDF_DEVICE_PREPARE_HARDWARE I2cEvtPrepareHardware;
static EVT_WDF_DEVICE_RELEASE_HARDWARE I2cEvtReleaseHardware;
static EVT_WDF_DEVICE_D0_ENTRY I2cEvtD0Entry;
static EVT_WDF_DEVICE_D0_EXIT I2cEvtD0Exit;
static EVT_SPB_TARGET_CONNECT I2cEvtTargetConnect;
static EVT_SPB_TARGET_DISCONNECT I2cEvtTargetDisconnect;

/*
 * SpbCx hands over the resource hub's output buffer: PropertiesLength, then
 * the descriptor. An I2cSerialBus descriptor carries the connection speed and
 * slave address right after the generic serial bus header.
 */
#include <pshpack1.h>
typedef struct _PNP_I2C_SERIAL_BUS_DESCRIPTOR
{
    PNP_SERIAL_BUS_DESCRIPTOR SerialBusDescriptor;
    ULONG ConnectionSpeed;
    USHORT SlaveAddress;
} PNP_I2C_SERIAL_BUS_DESCRIPTOR, *PPNP_I2C_SERIAL_BUS_DESCRIPTOR;
#include <poppack.h>

/* I2C type specific flags: bit 0 selects ten bit addressing */
#define I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS 0x0001

/**
 * @brief Reads the slave address, speed and addressing mode of a target.
 *
 * The length is checked before the bus type, and the bus type before the
 * I2C specific fields.
 */
static
NTSTATUS
NTAPI
I2cTargetGetSettings(
    _In_ SPBTARGET SpbTarget,
    _Out_ PI2C_TARGET Settings)
{
    PRH_QUERY_CONNECTION_PROPERTIES_OUTPUT_BUFFER Connection;
    PPNP_I2C_SERIAL_BUS_DESCRIPTOR Descriptor;
    SPB_CONNECTION_PARAMETERS Parameters;

    RtlZeroMemory(Settings, sizeof(*Settings));

    SPB_CONNECTION_PARAMETERS_INIT(&Parameters);
    SpbTargetGetConnectionParameters(SpbTarget, &Parameters);

    Connection = Parameters.ConnectionParameters;
    if (Connection == NULL)
        return STATUS_INVALID_PARAMETER;

    if (Connection->PropertiesLength < sizeof(PNP_SERIAL_BUS_DESCRIPTOR))
    {
        DPRINT1("i2c: connection properties are %lu bytes, need at least %u\n",
                Connection->PropertiesLength,
                (ULONG)sizeof(PNP_SERIAL_BUS_DESCRIPTOR));
        return STATUS_INVALID_PARAMETER;
    }

    Descriptor = (PPNP_I2C_SERIAL_BUS_DESCRIPTOR)Connection->ConnectionProperties;

    if (Descriptor->SerialBusDescriptor.SerialBusType != SERIAL_BUS_I2C_DESCRIPTOR_TYPE)
    {
        DPRINT1("i2c: connection is bus type %u, not I2C\n",
                Descriptor->SerialBusDescriptor.SerialBusType);
        return STATUS_INVALID_PARAMETER;
    }

    if (Connection->PropertiesLength < sizeof(*Descriptor))
        return STATUS_INVALID_PARAMETER;

    Settings->ConnectionSpeed = Descriptor->ConnectionSpeed;
    Settings->SlaveAddress = Descriptor->SlaveAddress;
    Settings->TenBitAddress = (Descriptor->SerialBusDescriptor.TypeSpecificFlags &
                               I2C_SERIAL_BUS_SPECIFIC_FLAG_10BIT_ADDRESS) != 0;

    DPRINT("i2c: target at 0x%02X, %lu Hz%s\n",
           Settings->SlaveAddress,
           Settings->ConnectionSpeed,
           Settings->TenBitAddress ? ", 10 bit" : "");

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
I2cEvtTargetConnect(
    _In_ WDFDEVICE Controller,
    _In_ SPBTARGET SpbTarget)
{
    WDF_OBJECT_ATTRIBUTES Attributes;
    PI2C_TARGET Target;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(Controller);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attributes, I2C_TARGET);

    Status = WdfObjectAllocateContext(SpbTargetGetFileObject(SpbTarget), &Attributes, (PVOID *)&Target);
    if (!NT_SUCCESS(Status))
        return Status;

    return I2cTargetGetSettings(SpbTarget, Target);
}

static
VOID
NTAPI
I2cEvtTargetDisconnect(
    _In_ WDFDEVICE Controller,
    _In_ SPBTARGET SpbTarget)
{
    UNREFERENCED_PARAMETER(Controller);
    UNREFERENCED_PARAMETER(SpbTarget);
}

/**
 * @brief Installs the bus timing and records the PCI device id for diagnostics.
 *
 * The timing is the same for every family. A part whose id cannot be read still starts.
 */
static
NTSTATUS
NTAPI
I2cQueryPlatformTiming(
    _Inout_ PI2C_DEVICE Context)
{
    BUS_INTERFACE_STANDARD BusInterface;
    USHORT DeviceId = 0;
    ULONG Read;
    NTSTATUS Status;

    PAGED_CODE();

    Status = WdfFdoQueryForInterface(Context->Device,
                                     &GUID_BUS_INTERFACE_STANDARD,
                                     (PINTERFACE)&BusInterface,
                                     sizeof(BusInterface),
                                     1,
                                     NULL);
    if (NT_SUCCESS(Status))
    {
        Read = BusInterface.GetBusData(BusInterface.Context,
                                       PCI_WHICHSPACE_CONFIG,
                                       &DeviceId,
                                       FIELD_OFFSET(PCI_COMMON_HEADER, DeviceID),
                                       sizeof(DeviceId));

        if (BusInterface.InterfaceDereference != NULL)
            BusInterface.InterfaceDereference(BusInterface.Context);

        if (Read == sizeof(DeviceId))
            Context->PciDeviceId = DeviceId;
    }

    DPRINT("i2c: controller is DEV_%04X\n", Context->PciDeviceId);

    I2cControllerLoadDefaultTiming(Context);
    return STATUS_SUCCESS;
}

/* The DesignWare core, then Intel's shell; older ACPI platforms omit the shell */
static
NTSTATUS
NTAPI
I2cEvtPrepareHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesRaw,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    PI2C_DEVICE Context = I2cGetDeviceContext(Device);
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    ULONG MemoryIndex = 0;
    ULONG Count;
    ULONG Index;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    PAGED_CODE();

    Count = WdfCmResourceListGetCount(ResourcesTranslated);

    for (Index = 0; Index < Count; Index++)
    {
        Descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, Index);
        if (Descriptor == NULL || Descriptor->Type != CmResourceTypeMemory)
            continue;

        if (MemoryIndex == 0)
        {
            Context->CoreLength = Descriptor->u.Memory.Length;
            Context->CoreBase = MmMapIoSpace(Descriptor->u.Memory.Start, Context->CoreLength, MmNonCached);
            if (Context->CoreBase == NULL)
                return STATUS_INSUFFICIENT_RESOURCES;
        }
        else if (MemoryIndex == 1)
        {
            /* Not fatal: the shell only carries reset and power hints */
            Context->PrivLength = Descriptor->u.Memory.Length;
            Context->PrivBase = MmMapIoSpace(Descriptor->u.Memory.Start, Context->PrivLength, MmNonCached);
        }

        MemoryIndex++;
    }

    if (Context->CoreBase == NULL)
    {
        DPRINT1("i2c: no memory resource for the controller core\n");
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return I2cQueryPlatformTiming(Context);
}

static
NTSTATUS
NTAPI
I2cEvtReleaseHardware(
    _In_ WDFDEVICE Device,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    PI2C_DEVICE Context = I2cGetDeviceContext(Device);

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    PAGED_CODE();

    if (Context->PrivBase != NULL)
    {
        MmUnmapIoSpace(Context->PrivBase, Context->PrivLength);
        Context->PrivBase = NULL;
    }

    if (Context->CoreBase != NULL)
    {
        MmUnmapIoSpace(Context->CoreBase, Context->CoreLength);
        Context->CoreBase = NULL;
    }

    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
I2cEvtD0Entry(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE PreviousState)
{
    UNREFERENCED_PARAMETER(PreviousState);

    return I2cControllerInitialize(I2cGetDeviceContext(Device));
}

static
NTSTATUS
NTAPI
I2cEvtD0Exit(
    _In_ WDFDEVICE Device,
    _In_ WDF_POWER_DEVICE_STATE TargetState)
{
    UNREFERENCED_PARAMETER(TargetState);

    I2cControllerUninitialize(I2cGetDeviceContext(Device));
    return STATUS_SUCCESS;
}

/* SpbDeviceInitConfig has to precede our own device init calls */
static
NTSTATUS
NTAPI
I2cEvtDeviceAdd(
    _In_ WDFDRIVER Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit)
{
    WDF_PNPPOWER_EVENT_CALLBACKS PnpCallbacks;
    SPB_CONTROLLER_CONFIG ControllerConfig;
    WDF_INTERRUPT_CONFIG InterruptConfig;
    WDF_OBJECT_ATTRIBUTES Attributes;
    PI2C_DEVICE Context;
    WDFDEVICE Device;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    Status = SpbDeviceInitConfig(DeviceInit);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("i2c: SpbDeviceInitConfig failed 0x%lx\n", Status);
        return Status;
    }

    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpCallbacks);
    PnpCallbacks.EvtDevicePrepareHardware = I2cEvtPrepareHardware;
    PnpCallbacks.EvtDeviceReleaseHardware = I2cEvtReleaseHardware;
    PnpCallbacks.EvtDeviceD0Entry = I2cEvtD0Entry;
    PnpCallbacks.EvtDeviceD0Exit = I2cEvtD0Exit;
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpCallbacks);

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attributes, I2C_DEVICE);

    Status = WdfDeviceCreate(&DeviceInit, &Attributes, &Device);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("i2c: WdfDeviceCreate failed 0x%lx\n", Status);
        return Status;
    }

    Context = I2cGetDeviceContext(Device);
    Context->Device = Device;

    SPB_CONTROLLER_CONFIG_INIT(&ControllerConfig);
    ControllerConfig.EvtSpbTargetConnect = I2cEvtTargetConnect;
    ControllerConfig.EvtSpbTargetDisconnect = I2cEvtTargetDisconnect;
    ControllerConfig.EvtSpbIoRead = I2cEvtIoRead;
    ControllerConfig.EvtSpbIoWrite = I2cEvtIoWrite;
    ControllerConfig.EvtSpbIoSequence = I2cEvtIoSequence;

    Status = SpbDeviceInitialize(Device, &ControllerConfig);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("i2c: SpbDeviceInitialize failed 0x%lx\n", Status);
        return Status;
    }

    WDF_INTERRUPT_CONFIG_INIT(&InterruptConfig, I2cEvtInterruptIsr, I2cEvtInterruptDpc);

    Status = WdfInterruptCreate(Device, &InterruptConfig, WDF_NO_OBJECT_ATTRIBUTES, &Context->Interrupt);
    if (!NT_SUCCESS(Status))
        DPRINT1("i2c: WdfInterruptCreate failed 0x%lx\n", Status);

    return Status;
}

NTSTATUS
NTAPI
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG Config;

    WDF_DRIVER_CONFIG_INIT(&Config, I2cEvtDeviceAdd);

    return WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &Config, WDF_NO_HANDLE);
}
