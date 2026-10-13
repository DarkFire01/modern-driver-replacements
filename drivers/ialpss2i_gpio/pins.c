/*
 * PROJECT:     ReactOS Intel LPSS GPIO controller driver
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Pad configuration and pin I/O
 * COPYRIGHT:   Copyright 2026 Justin Miller <justin.miller@reactos.org>
 */

#include "gpiopriv.h"

/**
 * @brief
 * Whether host software owns a pad. A pad the firmware kept for a native function is not ours to drive.
 */
static
BOOLEAN
NTAPI
GpioIsHostOwned(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId,
    _In_ PIN_NUMBER PinNumber)
{
    PULONG Ownership;

    Ownership = GpioGroupRegister(Controller, BankId, Controller->Layout->HostSwOwnOffset);
    if (Ownership == NULL)
        return FALSE;

    return (READ_REGISTER_ULONG(Ownership) & (1UL << PinNumber)) ? TRUE : FALSE;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioConnectIoPins(
    PVOID Context,
    PGPIO_CONNECT_IO_PINS_PARAMETERS ConnectParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    BANK_ID BankId = ConnectParameters->BankId;
    PGPIO_PIN_STATE PinState;
    PIN_NUMBER PinNumber;
    ULONG Clear;
    ULONG Set;
    ULONG Value;
    USHORT Index;
    PULONG Pad;

    /* GPIO mode, input buffer on; outputs also enable the driver so they read back */
    Clear = PADCFG0_PMODE_MASK | PADCFG0_GPIORXDIS;
    Set = 0;
    if (ConnectParameters->ConnectMode == ConnectModeOutput)
        Clear |= PADCFG0_GPIOTXDIS;
    else
        Set |= PADCFG0_GPIOTXDIS;

    for (Index = 0; Index < ConnectParameters->PinCount; Index++)
    {
        PinNumber = ConnectParameters->PinNumberTable[Index];

        Pad = GpioPadAddress(Controller, BankId, PinNumber);
        if (Pad == NULL || !GpioIsHostOwned(Controller, BankId, PinNumber))
            return STATUS_INVALID_PARAMETER;

        Value = READ_REGISTER_ULONG(Pad);
        WRITE_REGISTER_ULONG(Pad, (Value & ~Clear) | Set);

        PinState = GpioPinState(Controller, BankId, PinNumber);
        if (PinState != NULL)
            PinState->InUse = TRUE;
    }

    return STATUS_SUCCESS;
}

/**
 * @brief
 * Stops driving a run of pads. They stay in GPIO mode; the earlier mux is not known.
 */
_Use_decl_annotations_
NTSTATUS
NTAPI
GpioDisconnectIoPins(
    PVOID Context,
    PGPIO_DISCONNECT_IO_PINS_PARAMETERS DisconnectParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    ULONG Index;
    PULONG Pad;

    for (Index = 0; Index < DisconnectParameters->PinCount; Index++)
    {
        Pad = GpioPadAddress(Controller,
                             DisconnectParameters->BankId,
                             DisconnectParameters->PinNumberTable[Index]);
        if (Pad != NULL)
            WRITE_REGISTER_ULONG(Pad, READ_REGISTER_ULONG(Pad) | PADCFG0_GPIOTXDIS);
    }

    return STATUS_SUCCESS;
}

/**
 * @brief
 * Reads a run of pads. Bit n of the result is the n-th pin of the table.
 */
_Use_decl_annotations_
NTSTATUS
NTAPI
GpioReadGpioPins(
    PVOID Context,
    PGPIO_READ_PINS_PARAMETERS ReadParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    ULONG64 Levels = 0;
    ULONG Index;
    PULONG Pad;

    for (Index = 0; Index < ReadParameters->PinCount; Index++)
    {
        Pad = GpioPadAddress(Controller,
                             ReadParameters->BankId,
                             ReadParameters->PinNumberTable[Index]);
        if (Pad == NULL)
            return STATUS_INVALID_PARAMETER;

        if (READ_REGISTER_ULONG(Pad) & PADCFG0_GPIORXSTATE)
            Levels |= 1ULL << Index;
    }

    *(PULONG64)ReadParameters->Buffer = Levels;
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioWriteGpioPins(
    PVOID Context,
    PGPIO_WRITE_PINS_PARAMETERS WriteParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    ULONG64 Levels = *(PULONG64)WriteParameters->Buffer;
    ULONG Value;
    ULONG Index;
    PULONG Pad;

    for (Index = 0; Index < WriteParameters->PinCount; Index++)
    {
        Pad = GpioPadAddress(Controller,
                             WriteParameters->BankId,
                             WriteParameters->PinNumberTable[Index]);
        if (Pad == NULL)
            return STATUS_INVALID_PARAMETER;

        Value = READ_REGISTER_ULONG(Pad) & ~PADCFG0_GPIOTXSTATE;
        if (Levels & (1ULL << Index))
            Value |= PADCFG0_GPIOTXSTATE;

        WRITE_REGISTER_ULONG(Pad, Value);
    }

    return STATUS_SUCCESS;
}
