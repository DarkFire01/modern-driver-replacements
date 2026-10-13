/*
 * PROJECT:     ReactOS Simple Peripheral Bus
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     DesignWare I2C PIO transfer engine
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

/*
 * PIO only. Every byte is a write to IC_DATA_CMD: the data byte for a write,
 * a READ command for a read, whose data comes back through the RX FIFO.
 * RESTART on the first command of a later transfer emits a repeated start and
 * STOP on the last command of the request releases the bus.
 */

#include "i2cpriv.h"

#define NDEBUG
#include <debug.h>

#define I2C_INTR_ERRORS (DW_IC_INTR_TX_ABRT | DW_IC_INTR_RX_OVER | DW_IC_INTR_RX_UNDER)

#define I2C_INTR_ACTIVE_MASK (DW_IC_INTR_DEFAULT_MASK | DW_IC_INTR_TX_EMPTY)

static
VOID
NTAPI
I2cCompleteCurrentTransfer(
    _Inout_ PI2C_DEVICE Device,
    _In_ NTSTATUS Status)
{
    SPBREQUEST Request = Device->CurrentRequest;
    ULONG Information = Device->BytesTransferred;

    I2cControllerDisableInterrupts(Device);
    (VOID)I2cRead(Device, DW_IC_CLR_INTR);

    Device->CurrentRequest = NULL;
    Device->CurrentTarget = NULL;
    Device->CurrentBuffer = NULL;
    Device->CurrentLength = 0;
    Device->BytesWritten = 0;
    Device->BytesRead = 0;
    Device->BytesTransferred = 0;

    if (Request == NULL)
        return;

    WdfRequestSetInformation((WDFREQUEST)Request, Information);
    SpbRequestComplete(Request, Status);
}

/**
 * @brief Loads the next non empty transfer of the request into the engine.
 *
 * @return FALSE once no transfers are left.
 */
static
BOOLEAN
NTAPI
I2cStartNextTransfer(
    _Inout_ PI2C_DEVICE Device)
{
    SPB_TRANSFER_DESCRIPTOR Descriptor;
    PMDL Mdl;

    for (; Device->TransferIndex < Device->TransferCount; Device->TransferIndex++)
    {
        Mdl = NULL;
        SPB_TRANSFER_DESCRIPTOR_INIT(&Descriptor);
        SpbRequestGetTransferParameters(Device->CurrentRequest, Device->TransferIndex, &Descriptor, &Mdl);

        Device->CurrentDirection = Descriptor.Direction;
        Device->BytesWritten = 0;
        Device->BytesRead = 0;
        Device->CurrentLength = 0;
        Device->CurrentBuffer = NULL;

        /* SpbCx locked the pages, so the mapping lives as long as the request */
        if (Mdl != NULL)
        {
            Device->CurrentBuffer = MmGetSystemAddressForMdlSafe(Mdl, NormalPagePriority | MdlMappingNoExecute);
            if (Device->CurrentBuffer != NULL)
                Device->CurrentLength = MmGetMdlByteCount(Mdl);
        }

        if (Device->CurrentLength != 0)
            return TRUE;
    }

    return FALSE;
}

/* Feeds the TX FIFO as far as it has room */
static
VOID
NTAPI
I2cPushCommands(
    _Inout_ PI2C_DEVICE Device)
{
    BOOLEAN LastTransfer = (Device->TransferIndex + 1) == Device->TransferCount;
    ULONG Room;
    ULONG Command;

    Room = Device->TxFifoDepth - I2cRead(Device, DW_IC_TXFLR);

    for (; Room != 0 && Device->BytesWritten < Device->CurrentLength; Room--)
    {
        if (Device->CurrentDirection == SpbTransferDirectionFromDevice)
            Command = DW_IC_DATA_CMD_READ;
        else
            Command = Device->CurrentBuffer[Device->BytesWritten];

        if (Device->BytesWritten == 0 && Device->TransferIndex != 0)
            Command |= DW_IC_DATA_CMD_RESTART;

        if (LastTransfer && (Device->BytesWritten + 1) == Device->CurrentLength)
            Command |= DW_IC_DATA_CMD_STOP;

        I2cWrite(Device, DW_IC_DATA_CMD, Command);
        Device->BytesWritten++;
    }
}

static
VOID
NTAPI
I2cDrainReceiveFifo(
    _Inout_ PI2C_DEVICE Device)
{
    ULONG Available = I2cRead(Device, DW_IC_RXFLR);

    for (; Available != 0 && Device->BytesRead < Device->CurrentLength; Available--)
    {
        Device->CurrentBuffer[Device->BytesRead] = (UCHAR)(I2cRead(Device, DW_IC_DATA_CMD) & 0xFF);
        Device->BytesRead++;
    }
}

/**
 * @brief Tells whether the transfer in flight is done.
 *
 * A write is only done once the FIFO drained, or the next RESTART would be
 * queued behind bytes still on the wire.
 */
static
BOOLEAN
NTAPI
I2cTransferComplete(
    _In_ PI2C_DEVICE Device)
{
    if (Device->CurrentDirection == SpbTransferDirectionFromDevice)
        return Device->BytesRead >= Device->CurrentLength;

    if (Device->BytesWritten < Device->CurrentLength)
        return FALSE;

    return (I2cRead(Device, DW_IC_STATUS) & DW_IC_STATUS_TFE) != 0;
}

/**
 * @brief Maps an abort to a completion status.
 *
 * An unanswered address means no device. A NACK on data ends the request
 * early but successfully, with the bytes moved so far.
 */
static
NTSTATUS
NTAPI
I2cAbortStatus(
    _Inout_ PI2C_DEVICE Device,
    _In_ ULONG Abort)
{
    if ((Abort & DW_IC_ABRT_ADDR_NOACK_MASK) != 0)
    {
        Device->BytesTransferred = 0;
        return STATUS_NO_SUCH_DEVICE;
    }

    if ((Abort & DW_IC_ABRT_TXDATA_NOACK) != 0)
    {
        Device->BytesTransferred += Device->BytesWritten;
        return STATUS_SUCCESS;
    }

    Device->BytesTransferred = 0;
    return STATUS_UNSUCCESSFUL;
}

/* IC_INTR_STAT is already masked, so a non zero read is ours */
BOOLEAN
NTAPI
I2cEvtInterruptIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ ULONG MessageId)
{
    PI2C_DEVICE Device = I2cGetDeviceContext(WdfInterruptGetDevice(Interrupt));
    ULONG Status;

    UNREFERENCED_PARAMETER(MessageId);

    Status = I2cRead(Device, DW_IC_INTR_STAT);
    if (Status == 0)
        return FALSE;

    /* The clear below wipes the abort source */
    if ((Status & DW_IC_INTR_TX_ABRT) != 0)
        Device->AbortSource = I2cRead(Device, DW_IC_TX_ABRT_SOURCE);

    Device->InterruptStatus |= Status;

    /* The DPC re-arms what it needs */
    I2cControllerDisableInterrupts(Device);
    (VOID)I2cRead(Device, DW_IC_CLR_INTR);

    WdfInterruptQueueDpcForIsr(Interrupt);
    return TRUE;
}

VOID
NTAPI
I2cEvtInterruptDpc(
    _In_ WDFINTERRUPT Interrupt,
    _In_ WDFOBJECT AssociatedObject)
{
    PI2C_DEVICE Device = I2cGetDeviceContext(WdfInterruptGetDevice(Interrupt));
    ULONG Status;
    ULONG Abort;

    UNREFERENCED_PARAMETER(AssociatedObject);

    Status = (ULONG)InterlockedExchange((volatile LONG *)&Device->InterruptStatus, 0);
    Abort = Device->AbortSource;
    Device->AbortSource = 0;

    if (Device->CurrentRequest == NULL)
        return;

    if ((Status & I2C_INTR_ERRORS) != 0)
    {
        DPRINT1("i2c: transfer aborted, IC_TX_ABRT_SOURCE 0x%08lX\n", Abort);
        I2cCompleteCurrentTransfer(Device, I2cAbortStatus(Device, Abort));
        return;
    }

    if (Device->CurrentDirection == SpbTransferDirectionFromDevice)
        I2cDrainReceiveFifo(Device);

    if (I2cTransferComplete(Device))
    {
        Device->BytesTransferred += Device->CurrentLength;
        Device->TransferIndex++;

        if (!I2cStartNextTransfer(Device))
        {
            I2cCompleteCurrentTransfer(Device, STATUS_SUCCESS);
            return;
        }
    }

    I2cPushCommands(Device);
    I2cControllerEnableInterrupts(Device, I2C_INTR_ACTIVE_MASK);
}

/* A read or a write is a one transfer sequence */
static
VOID
NTAPI
I2cBeginRequest(
    _In_ WDFDEVICE Controller,
    _In_ SPBTARGET SpbTarget,
    _In_ SPBREQUEST Request,
    _In_ ULONG TransferCount)
{
    PI2C_DEVICE Device = I2cGetDeviceContext(Controller);
    PI2C_TARGET Target;
    NTSTATUS Status;

    Target = I2cGetTargetContext(SpbTargetGetFileObject(SpbTarget));
    if (Target == NULL)
    {
        SpbRequestComplete(Request, STATUS_INVALID_DEVICE_REQUEST);
        return;
    }

    Device->CurrentTarget = SpbTarget;
    Device->CurrentRequest = Request;
    Device->TransferIndex = 0;
    Device->TransferCount = TransferCount;
    Device->BytesTransferred = 0;
    Device->InterruptStatus = 0;
    Device->AbortSource = 0;

    Status = I2cControllerConfigureForTransfer(Device, Target);
    if (!NT_SUCCESS(Status))
    {
        I2cCompleteCurrentTransfer(Device, Status);
        return;
    }

    /* A request of only empty transfers still succeeds */
    if (!I2cStartNextTransfer(Device))
    {
        I2cCompleteCurrentTransfer(Device, STATUS_SUCCESS);
        return;
    }

    I2cPushCommands(Device);
    I2cControllerEnableInterrupts(Device, I2C_INTR_ACTIVE_MASK);
}

VOID
NTAPI
I2cEvtIoSequence(
    _In_ WDFDEVICE Controller,
    _In_ SPBTARGET SpbTarget,
    _In_ SPBREQUEST Request,
    _In_ ULONG TransferCount)
{
    I2cBeginRequest(Controller, SpbTarget, Request, TransferCount);
}

VOID
NTAPI
I2cEvtIoRead(
    _In_ WDFDEVICE Controller,
    _In_ SPBTARGET SpbTarget,
    _In_ SPBREQUEST Request,
    _In_ size_t Length)
{
    UNREFERENCED_PARAMETER(Length);

    I2cBeginRequest(Controller, SpbTarget, Request, 1);
}

VOID
NTAPI
I2cEvtIoWrite(
    _In_ WDFDEVICE Controller,
    _In_ SPBTARGET SpbTarget,
    _In_ SPBREQUEST Request,
    _In_ size_t Length)
{
    UNREFERENCED_PARAMETER(Length);

    I2cBeginRequest(Controller, SpbTarget, Request, 1);
}
