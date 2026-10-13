/*
 * PROJECT:     ReactOS Intel LPSS GPIO controller driver
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Pad interrupt configuration
 * COPYRIGHT:   Copyright 2026 Justin Miller <justin.miller@reactos.org>
 */

/*
 * PADCFG0 decides what counts as an event on a pad; the community's GPI_IE and
 * GPI_IS arrays hold one armed bit and one latched bit per pad.
 */

#include "gpiopriv.h"

/**
 * @brief
 * Sets or clears pad bits in one of a group's interrupt words.
 */
static
VOID
NTAPI
GpioModifyGroupBits(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId,
    _In_ ULONG BlockOffset,
    _In_ ULONG Bits,
    _In_ BOOLEAN Set)
{
    PULONG Register;
    ULONG Value;

    Register = GpioGroupRegister(Controller, BankId, BlockOffset);
    if (Register == NULL)
        return;

    Value = READ_REGISTER_ULONG(Register);
    Value = Set ? (Value | Bits) : (Value & ~Bits);
    WRITE_REGISTER_ULONG(Register, Value);
}

/**
 * @brief
 * The RXEVCFG and RXINV bits for a trigger, or MAXULONG if the pad cannot do it.
 *
 * The pad only senses high levels and rising edges, so active low sources are inverted.
 */
static
ULONG
NTAPI
GpioTriggerBits(
    _In_ KINTERRUPT_MODE InterruptMode,
    _In_ KINTERRUPT_POLARITY Polarity)
{
    if (InterruptMode == LevelSensitive)
    {
        if (Polarity == InterruptActiveLow)
            return PADCFG0_RXEVCFG_LEVEL | PADCFG0_RXINV;

        return PADCFG0_RXEVCFG_LEVEL;
    }

    switch (Polarity)
    {
        case InterruptActiveHigh:
            return PADCFG0_RXEVCFG_EDGE;

        case InterruptActiveLow:
            return PADCFG0_RXEVCFG_EDGE | PADCFG0_RXINV;

        case InterruptActiveBoth:
            return PADCFG0_RXEVCFG_EDGE_BOTH;

        default:
            return MAXULONG;
    }
}

/**
 * @brief
 * Makes a pad a GPIO input that raises the given trigger.
 */
static
NTSTATUS
NTAPI
GpioProgramPadTrigger(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId,
    _In_ PIN_NUMBER PinNumber,
    _In_ KINTERRUPT_MODE InterruptMode,
    _In_ KINTERRUPT_POLARITY Polarity)
{
    ULONG Trigger;
    ULONG Value;
    PULONG Pad;

    Pad = GpioPadAddress(Controller, BankId, PinNumber);
    Trigger = GpioTriggerBits(InterruptMode, Polarity);
    if (Pad == NULL || Trigger == MAXULONG)
        return STATUS_INVALID_PARAMETER;

    Value = READ_REGISTER_ULONG(Pad);
    Value &= ~(PADCFG0_PMODE_MASK | PADCFG0_GPIORXDIS | PADCFG0_RXEVCFG_MASK | PADCFG0_RXINV);
    Value |= PADCFG0_GPIOTXDIS | Trigger;
    WRITE_REGISTER_ULONG(Pad, Value);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioEnableInterrupt(
    PVOID Context,
    PGPIO_ENABLE_INTERRUPT_PARAMETERS EnableParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    BANK_ID BankId = EnableParameters->BankId;
    PIN_NUMBER PinNumber = EnableParameters->PinNumber;
    PGPIO_PIN_STATE PinState;
    NTSTATUS Status;

    Status = GpioProgramPadTrigger(Controller,
                                   BankId,
                                   PinNumber,
                                   EnableParameters->InterruptMode,
                                   EnableParameters->Polarity);
    if (!NT_SUCCESS(Status))
        return Status;

    /* Drop whatever latched while the pad was being reprogrammed, then arm it */
    WRITE_REGISTER_ULONG(GpioGroupRegister(Controller, BankId, GPIO_COMMUNITY_GPI_STATUS),
                         1UL << PinNumber);
    GpioModifyGroupBits(Controller, BankId, GPIO_COMMUNITY_GPI_ENABLE, 1UL << PinNumber, TRUE);

    /* An armed pad is restored across power transitions like a connected one */
    PinState = GpioPinState(Controller, BankId, PinNumber);
    if (PinState != NULL)
        PinState->InUse = TRUE;

    return STATUS_SUCCESS;
}

/**
 * @brief
 * Changes the trigger of a pad without disarming it.
 */
_Use_decl_annotations_
NTSTATUS
NTAPI
GpioReconfigureInterrupt(
    PVOID Context,
    PGPIO_RECONFIGURE_INTERRUPTS_PARAMETERS ReconfigureParameters)
{
    return GpioProgramPadTrigger(Context,
                                 ReconfigureParameters->BankId,
                                 ReconfigureParameters->PinNumber,
                                 ReconfigureParameters->InterruptMode,
                                 ReconfigureParameters->Polarity);
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioDisableInterrupt(
    PVOID Context,
    PGPIO_DISABLE_INTERRUPT_PARAMETERS DisableParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    ULONG Value;
    PULONG Pad;

    GpioModifyGroupBits(Controller,
                        DisableParameters->BankId,
                        GPIO_COMMUNITY_GPI_ENABLE,
                        1UL << DisableParameters->PinNumber,
                        FALSE);

    /* Stop event detection too, so nothing latches while disarmed */
    Pad = GpioPadAddress(Controller, DisableParameters->BankId, DisableParameters->PinNumber);
    if (Pad != NULL)
    {
        Value = READ_REGISTER_ULONG(Pad);
        Value = (Value & ~PADCFG0_RXEVCFG_MASK) | PADCFG0_RXEVCFG_DISABLED;
        WRITE_REGISTER_ULONG(Pad, Value);
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioMaskInterrupts(
    PVOID Context,
    PGPIO_MASK_INTERRUPT_PARAMETERS MaskParameters)
{
    PGPIO_CONTROLLER Controller = Context;

    if (GpioGroupRegister(Controller, MaskParameters->BankId, GPIO_COMMUNITY_GPI_ENABLE) == NULL)
    {
        MaskParameters->FailedMask = MaskParameters->PinMask;
        return STATUS_INVALID_PARAMETER;
    }

    GpioModifyGroupBits(Controller,
                        MaskParameters->BankId,
                        GPIO_COMMUNITY_GPI_ENABLE,
                        (ULONG)MaskParameters->PinMask,
                        FALSE);
    MaskParameters->FailedMask = 0;
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioUnmaskInterrupt(
    PVOID Context,
    PGPIO_ENABLE_INTERRUPT_PARAMETERS UnmaskParameters)
{
    GpioModifyGroupBits(Context,
                        UnmaskParameters->BankId,
                        GPIO_COMMUNITY_GPI_ENABLE,
                        1UL << UnmaskParameters->PinNumber,
                        TRUE);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioQueryActiveInterrupts(
    PVOID Context,
    PGPIO_QUERY_ACTIVE_INTERRUPTS_PARAMETERS QueryActiveParameters)
{
    PULONG Latched;

    Latched = GpioGroupRegister(Context, QueryActiveParameters->BankId, GPIO_COMMUNITY_GPI_STATUS);
    if (Latched == NULL)
        return STATUS_INVALID_PARAMETER;

    /* GPI_IS latches unarmed pads too */
    QueryActiveParameters->ActiveMask = READ_REGISTER_ULONG(Latched) &
                                        QueryActiveParameters->EnabledMask;
    return STATUS_SUCCESS;
}

/**
 * @brief
 * Acknowledges latched pads. GPI_IS is write one to clear, so only the named bits are written.
 */
_Use_decl_annotations_
NTSTATUS
NTAPI
GpioClearActiveInterrupts(
    PVOID Context,
    PGPIO_CLEAR_ACTIVE_INTERRUPTS_PARAMETERS ClearParameters)
{
    PULONG Latched;

    Latched = GpioGroupRegister(Context, ClearParameters->BankId, GPIO_COMMUNITY_GPI_STATUS);
    if (Latched == NULL)
    {
        ClearParameters->FailedClearMask = ClearParameters->ClearActiveMask;
        return STATUS_INVALID_PARAMETER;
    }

    WRITE_REGISTER_ULONG(Latched, (ULONG)ClearParameters->ClearActiveMask);
    ClearParameters->FailedClearMask = 0;
    return STATUS_SUCCESS;
}
