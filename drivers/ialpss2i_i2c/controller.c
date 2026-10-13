/*
 * PROJECT:     ReactOS Simple Peripheral Bus
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     DesignWare I2C engine
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

#include "i2cpriv.h"

#define NDEBUG
#include <debug.h>

/* Bus timing every LPSS I2C controller starts with; registry and ACPI overrides are not read yet */
static const I2C_SPEED_TIMING I2cStandardSpeedDefault = { 0x210, 0x280, 0x1C, 0x1C, 1 };
static const I2C_SPEED_TIMING I2cFastSpeedDefault     = { 0x080, 0x0A0, 0x1C, 0x1C, 6 };
static const I2C_SPEED_TIMING I2cFastSpeedPlusDefault = { 0x01E, 0x050, 0x28, 0x28, 6 };
static const I2C_SPEED_TIMING I2cHighSpeedDefault     = { 0x00F, 0x028, 0x14, 0x14, 2 };

/**
 * @brief Enables or disables the core and waits for IC_ENABLE_STATUS to follow.
 *
 * Most DesignWare registers are only writable while the core is disabled.
 */
static
NTSTATUS
NTAPI
I2cSetCoreEnabled(
    _In_ PI2C_DEVICE Device,
    _In_ BOOLEAN Enable)
{
    ULONG Attempt;

    for (Attempt = 0; Attempt < 100; Attempt++)
    {
        I2cWrite(Device, DW_IC_ENABLE, Enable ? 1 : 0);

        if (((I2cRead(Device, DW_IC_ENABLE_STATUS) & 1) != 0) == (Enable != FALSE))
            return STATUS_SUCCESS;

        /* Ten bus cycles at 100 kHz is 100us */
        KeStallExecutionProcessor(25);
    }

    DPRINT1("i2c: core would not %s\n", Enable ? "enable" : "disable");
    return STATUS_IO_TIMEOUT;
}

VOID
NTAPI
I2cControllerLoadDefaultTiming(
    _Inout_ PI2C_DEVICE Device)
{
    Device->StandardSpeed = I2cStandardSpeedDefault;
    Device->FastSpeed = I2cFastSpeedDefault;
    Device->FastSpeedPlus = I2cFastSpeedPlusDefault;
    Device->HighSpeed = I2cHighSpeedDefault;
}

/**
 * @brief Programs one speed's counts into the standard or fast register pair.
 *
 * Fast mode plus uses the fast pair. IC_SDA_HOLD keeps the transmit hold in
 * bits 0-15 and the receive hold in bits 16-23.
 */
static
VOID
NTAPI
I2cApplyTiming(
    _In_ PI2C_DEVICE Device,
    _In_ PI2C_SPEED_TIMING Timing,
    _In_ BOOLEAN FastRegisters)
{
    if (FastRegisters)
    {
        I2cWrite(Device, DW_IC_FS_SCL_HCNT, Timing->SclHighCount);
        I2cWrite(Device, DW_IC_FS_SCL_LCNT, Timing->SclLowCount);
    }
    else
    {
        I2cWrite(Device, DW_IC_SS_SCL_HCNT, Timing->SclHighCount);
        I2cWrite(Device, DW_IC_SS_SCL_LCNT, Timing->SclLowCount);
    }

    I2cWrite(Device, DW_IC_FS_SPKLEN, Timing->SpikeLength);
    I2cWrite(Device,
             DW_IC_SDA_HOLD,
             (Timing->SdaWriteHoldCount & 0xFFFF) | ((Timing->SdaReadHoldCount & 0xFF) << 16));
}

VOID
NTAPI
I2cControllerDisableInterrupts(
    _In_ PI2C_DEVICE Device)
{
    I2cWrite(Device, DW_IC_INTR_MASK, 0);
}

VOID
NTAPI
I2cControllerEnableInterrupts(
    _In_ PI2C_DEVICE Device,
    _In_ ULONG Mask)
{
    I2cWrite(Device, DW_IC_INTR_MASK, Mask);
}

/**
 * @brief Takes the shell out of reset and brings the core to a known, disabled state.
 *
 * The core reads back zero while the shell holds it in reset, so the reset
 * pulse comes first.
 */
NTSTATUS
NTAPI
I2cControllerInitialize(
    _In_ PI2C_DEVICE Device)
{
    PULONG ResetRegister;
    ULONG Value;
    NTSTATUS Status;

    if (Device->PrivBase != NULL)
    {
        /* Pulse the function and DMA reset bits so firmware leftovers are cleared */
        ResetRegister = (PULONG)(Device->PrivBase + LPSS_PRIV_RESETS);
        Value = READ_REGISTER_ULONG(ResetRegister);

        WRITE_REGISTER_ULONG(ResetRegister, Value & ~(ULONG)LPSS_PRIV_RESETS_FUNC);
        KeStallExecutionProcessor(10);
        WRITE_REGISTER_ULONG(ResetRegister, Value | LPSS_PRIV_RESETS_FUNC);
        KeStallExecutionProcessor(10);

        /* All ones means the read never reached hardware; only traced */
        Device->Capabilities = READ_REGISTER_ULONG((PULONG)(Device->PrivBase + LPSS_PRIV_CAPS));
        if ((Device->Capabilities & 0xF0) != 0 || Device->Capabilities == MAXULONG)
            DPRINT1("i2c: LPSS capabilities read back 0x%08lX\n", Device->Capabilities);
    }

    Status = I2cSetCoreEnabled(Device, FALSE);
    if (!NT_SUCCESS(Status))
        return Status;

    I2cControllerDisableInterrupts(Device);

    /* The thresholds saturate at depth minus one, which reveals the FIFO depth */
    I2cWrite(Device, DW_IC_TX_TL, 0xFF);
    Device->TxFifoDepth = (I2cRead(Device, DW_IC_TX_TL) & 0xFF) + 1;
    I2cWrite(Device, DW_IC_RX_TL, 0xFF);
    Device->RxFifoDepth = (I2cRead(Device, DW_IC_RX_TL) & 0xFF) + 1;

    I2cWrite(Device, DW_IC_RX_TL, 0);
    I2cWrite(Device, DW_IC_TX_TL, 0);

    (VOID)I2cRead(Device, DW_IC_CLR_INTR);

    DPRINT("i2c: DesignWare core, TX FIFO %lu, RX FIFO %lu\n", Device->TxFifoDepth, Device->RxFifoDepth);

    return STATUS_SUCCESS;
}

VOID
NTAPI
I2cControllerUninitialize(
    _In_ PI2C_DEVICE Device)
{
    I2cControllerDisableInterrupts(Device);
    (VOID)I2cRead(Device, DW_IC_CLR_INTR);
    (VOID)I2cSetCoreEnabled(Device, FALSE);
}

/**
 * @brief Points the core at a target, at the speed its descriptor asked for.
 *
 * IC_CON and IC_TAR only take writes while the core is disabled.
 */
NTSTATUS
NTAPI
I2cControllerConfigureForTransfer(
    _In_ PI2C_DEVICE Device,
    _In_ PI2C_TARGET Target)
{
    ULONG Control;
    NTSTATUS Status;

    Status = I2cSetCoreEnabled(Device, FALSE);
    if (!NT_SUCCESS(Status))
        return Status;

    /* RESTART_EN gives a sequence its repeated starts */
    Control = DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE | DW_IC_CON_RESTART_EN;

    if (Target->ConnectionSpeed > 400000)
    {
        Control |= DW_IC_CON_SPEED_FAST;
        I2cApplyTiming(Device, &Device->FastSpeedPlus, TRUE);
    }
    else if (Target->ConnectionSpeed > 100000)
    {
        Control |= DW_IC_CON_SPEED_FAST;
        I2cApplyTiming(Device, &Device->FastSpeed, TRUE);
    }
    else
    {
        Control |= DW_IC_CON_SPEED_STD;
        I2cApplyTiming(Device, &Device->StandardSpeed, FALSE);
    }

    if (Target->TenBitAddress)
        Control |= DW_IC_CON_10BITADDR_MASTER;

    I2cWrite(Device, DW_IC_CON, Control);
    I2cWrite(Device, DW_IC_TAR, Target->SlaveAddress);

    I2cControllerDisableInterrupts(Device);
    (VOID)I2cRead(Device, DW_IC_CLR_INTR);

    return I2cSetCoreEnabled(Device, TRUE);
}
