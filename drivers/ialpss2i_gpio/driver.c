/*
 * PROJECT:     ReactOS Intel LPSS GPIO controller driver
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Driver entry and GpioClx registration
 * COPYRIGHT:   Copyright 2026 Justin Miller <justin.miller@reactos.org>
 */

#include "gpiopriv.h"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, GpioEvtDeviceAdd)
#pragma alloc_text(PAGE, GpioEvtDriverCleanup)
#endif

/**
 * @brief
 * Creates the controller device, with GpioClx attached on either side of WdfDeviceCreate.
 */
_Use_decl_annotations_
NTSTATUS
NTAPI
GpioEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit)
{
    WDF_OBJECT_ATTRIBUTES Attributes;
    WDFDEVICE Device;
    NTSTATUS Status;

    PAGED_CODE();

    Status = GPIO_CLX_ProcessAddDevicePreDeviceCreate(Driver, DeviceInit, &Attributes);
    if (!NT_SUCCESS(Status))
        return Status;

    Status = WdfDeviceCreate(&DeviceInit, &Attributes, &Device);
    if (!NT_SUCCESS(Status))
        return Status;

    return GPIO_CLX_ProcessAddDevicePostDeviceCreate(Driver, Device);
}

_Use_decl_annotations_
VOID
NTAPI
GpioEvtDriverCleanup(
    WDFOBJECT Object)
{
    PAGED_CODE();

    GPIO_CLX_UnregisterClient((WDFDRIVER)Object);
}

_Use_decl_annotations_
NTSTATUS
NTAPI
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath)
{
    GPIO_CLIENT_REGISTRATION_PACKET Packet;
    WDF_OBJECT_ATTRIBUTES Attributes;
    WDF_DRIVER_CONFIG Config;
    WDFDRIVER Driver;
    NTSTATUS Status;

    WDF_DRIVER_CONFIG_INIT(&Config, GpioEvtDeviceAdd);

    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.EvtCleanupCallback = GpioEvtDriverCleanup;

    Status = WdfDriverCreate(DriverObject, RegistryPath, &Attributes, &Config, &Driver);
    if (!NT_SUCCESS(Status))
        return Status;

    /* Slots left NULL are callbacks this controller does not offer */
    RtlZeroMemory(&Packet, sizeof(Packet));
    Packet.Version = GPIO_REGISTRATION_VERSION;
    Packet.Size = sizeof(Packet);
    Packet.ControllerContextSize = sizeof(GPIO_CONTROLLER);

    Packet.CLIENT_PrepareController = GpioPrepareController;
    Packet.CLIENT_ReleaseController = GpioReleaseController;
    Packet.CLIENT_StartController = GpioStartController;
    Packet.CLIENT_StopController = GpioStopController;
    Packet.CLIENT_QueryControllerBasicInformation = GpioQueryControllerBasicInformation;

    Packet.CLIENT_EnableInterrupt = GpioEnableInterrupt;
    Packet.CLIENT_DisableInterrupt = GpioDisableInterrupt;
    Packet.CLIENT_UnmaskInterrupt = GpioUnmaskInterrupt;
    Packet.CLIENT_MaskInterrupts = GpioMaskInterrupts;
    Packet.CLIENT_QueryActiveInterrupts = GpioQueryActiveInterrupts;
    Packet.CLIENT_ClearActiveInterrupts = GpioClearActiveInterrupts;
    Packet.CLIENT_ReconfigureInterrupt = GpioReconfigureInterrupt;

    Packet.CLIENT_ConnectIoPins = GpioConnectIoPins;
    Packet.CLIENT_DisconnectIoPins = GpioDisconnectIoPins;
    Packet.CLIENT_ReadGpioPins = GpioReadGpioPins;
    Packet.CLIENT_WriteGpioPins = GpioWriteGpioPins;

    Packet.CLIENT_SaveBankHardwareContext = GpioSaveBankHardwareContext;
    Packet.CLIENT_RestoreBankHardwareContext = GpioRestoreBankHardwareContext;

    return GPIO_CLX_RegisterClient(Driver, &Packet, RegistryPath);
}
