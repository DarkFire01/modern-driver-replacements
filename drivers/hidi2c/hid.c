/*
 * PROJECT:     ReactOS HID Stack
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     HID over I2C: the wire protocol and the hidclass IOCTLs
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

#define NDEBUG
#include "hidi2cp.h"

static
BOOLEAN
NTAPI
HidI2cDescriptorIsValid(
    _In_ const HIDI2C_DESCRIPTOR *Descriptor)
{
    if (Descriptor->DescLength != HIDI2C_DESCRIPTOR_LENGTH ||
        Descriptor->BcdVersion != HIDI2C_BCD_VERSION ||
        Descriptor->Reserved != 0)
    {
        return FALSE;
    }

    /* Every register and length a working device needs must be filled in */
    return Descriptor->ReportDescLength != 0 &&
           Descriptor->InputRegister != 0 &&
           Descriptor->MaxInputLength >= sizeof(USHORT) &&
           Descriptor->CommandRegister != 0 &&
           Descriptor->DataRegister != 0 &&
           Descriptor->VendorId != 0;
}

static
NTSTATUS
NTAPI
HidI2cFetchDescriptor(
    _In_ PHIDI2C_CONTEXT Context)
{
    NTSTATUS Status;

    PAGED_CODE();

    RtlZeroMemory(&Context->Descriptor, sizeof(Context->Descriptor));

    Status = HidI2cSpbReadRegister(Context,
                                   Context->DescriptorRegister,
                                   &Context->Descriptor,
                                   sizeof(Context->Descriptor),
                                   HIDI2C_TIMEOUT_DESCRIPTOR);
    if (!NT_SUCCESS(Status))
        return Status;

    if (!HidI2cDescriptorIsValid(&Context->Descriptor))
    {
        DPRINT1("hidi2c: HID descriptor rejected (length %u, version 0x%04X)\n",
                Context->Descriptor.DescLength, Context->Descriptor.BcdVersion);
        return STATUS_DEVICE_PROTOCOL_ERROR;
    }

    DPRINT("hidi2c: VID %04X PID %04X, report desc %u bytes, input %u bytes\n",
           Context->Descriptor.VendorId, Context->Descriptor.ProductId,
           Context->Descriptor.ReportDescLength,
           Context->Descriptor.MaxInputLength);

    return STATUS_SUCCESS;
}

/**
 * @brief Writes a two byte command to the command register.
 */
static
NTSTATUS
NTAPI
HidI2cSendCommand(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ UCHAR Opcode,
    _In_ UCHAR Argument,
    _In_ ULONG TimeoutSeconds)
{
    UCHAR Packet[4];

    Packet[0] = (UCHAR)(Context->Descriptor.CommandRegister & 0xFF);
    Packet[1] = (UCHAR)(Context->Descriptor.CommandRegister >> 8);
    Packet[2] = Argument;
    Packet[3] = Opcode;

    return HidI2cSpbWrite(Context, Packet, sizeof(Packet), TimeoutSeconds);
}

NTSTATUS
NTAPI
HidI2cSetPower(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ UCHAR PowerState)
{
    NTSTATUS Status;

    Status = HidI2cSendCommand(Context,
                               HIDI2C_OPCODE_SET_POWER,
                               PowerState,
                               HIDI2C_TIMEOUT_POWER);
    if (!NT_SUCCESS(Status))
        DPRINT1("hidi2c: SET_POWER(%u) failed 0x%08lX\n", PowerState, Status);

    return Status;
}

/**
 * @brief Reads the HID descriptor and allocates the input report buffer.
 */
NTSTATUS
NTAPI
HidI2cStart(
    _In_ PHIDI2C_CONTEXT Context)
{
    NTSTATUS Status;

    PAGED_CODE();

    Status = HidI2cFetchDescriptor(Context);
    if (!NT_SUCCESS(Status))
        return Status;

    Context->InputBufferLength = Context->Descriptor.MaxInputLength;
    Context->InputBuffer = ExAllocatePoolWithTag(NonPagedPoolNx,
                                                 Context->InputBufferLength,
                                                 HIDI2C_POOL_TAG);
    if (Context->InputBuffer == NULL)
        return STATUS_INSUFFICIENT_RESOURCES;

    return STATUS_SUCCESS;
}

VOID
NTAPI
HidI2cStop(
    _In_ PHIDI2C_CONTEXT Context)
{
    if (Context->InputBuffer == NULL)
        return;

    ExFreePoolWithTag(Context->InputBuffer, HIDI2C_POOL_TAG);
    Context->InputBuffer = NULL;
    Context->InputBufferLength = 0;
}

/**
 * @brief Issues RESET and holds the default queue until the device answers.
 *
 * The device acknowledges with a zero length input report. If it never does,
 * the timer releases the queue.
 */
NTSTATUS
NTAPI
HidI2cReset(
    _In_ PHIDI2C_CONTEXT Context)
{
    NTSTATUS Status;

    PAGED_CODE();

    WdfIoQueueStopSynchronously(Context->DefaultQueue);
    InterlockedExchange(&Context->ResetPending, 1);
    WdfTimerStart(Context->ResetTimer, WDF_REL_TIMEOUT_IN_MS(HIDI2C_RESET_TIMEOUT_MS));

    Status = HidI2cSendCommand(Context, HIDI2C_OPCODE_RESET, 0, HIDI2C_TIMEOUT_SHORT);
    if (NT_SUCCESS(Status))
        return STATUS_SUCCESS;

    DPRINT1("hidi2c: RESET failed 0x%08lX\n", Status);

    if (InterlockedExchange(&Context->ResetPending, 0) != 0)
    {
        WdfTimerStop(Context->ResetTimer, FALSE);
        WdfIoQueueStart(Context->DefaultQueue);
    }

    return Status;
}

/* Whichever of the ISR and the timer clears ResetPending restarts the queue */
static
BOOLEAN
NTAPI
HidI2cFinishReset(
    _In_ PHIDI2C_CONTEXT Context)
{
    if (InterlockedExchange(&Context->ResetPending, 0) == 0)
        return FALSE;

    WdfIoQueueStart(Context->DefaultQueue);
    return TRUE;
}

VOID
NTAPI
HidI2cEvtResetTimer(
    _In_ WDFTIMER Timer)
{
    PHIDI2C_CONTEXT Context = HidI2cGetContext(WdfTimerGetParentObject(Timer));

    if (HidI2cFinishReset(Context))
        DPRINT1("hidi2c: no reset response, releasing the queue\n");
}

/**
 * @brief Completes the oldest pending read with one input report.
 */
static
VOID
NTAPI
HidI2cDeliverReport(
    _In_ PHIDI2C_CONTEXT Context,
    _In_reads_bytes_(Length) PUCHAR Report,
    _In_ ULONG Length)
{
    WDFREQUEST Request;
    NTSTATUS Status;
    PVOID Buffer;

    /* With no read outstanding the report is dropped */
    Status = WdfIoQueueRetrieveNextRequest(Context->ReadQueue, &Request);
    if (!NT_SUCCESS(Status))
        return;

    Status = WdfRequestRetrieveOutputBuffer(Request, Length, &Buffer, NULL);
    if (!NT_SUCCESS(Status))
    {
        WdfRequestComplete(Request, Status);
        return;
    }

    RtlCopyMemory(Buffer, Report, Length);
    WdfRequestCompleteWithInformation(Request, STATUS_SUCCESS, Length);
}

/**
 * @brief Passive level ISR: reading the input register deasserts the line.
 */
BOOLEAN
NTAPI
HidI2cEvtInterruptIsr(
    _In_ WDFINTERRUPT Interrupt,
    _In_ ULONG MessageId)
{
    PHIDI2C_CONTEXT Context = HidI2cGetContext(WdfInterruptGetDevice(Interrupt));
    USHORT ReportLength;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(MessageId);

    if (Context->InputBuffer == NULL)
        return TRUE;

    RtlZeroMemory(Context->InputBuffer, Context->InputBufferLength);

    Status = HidI2cSpbRead(Context,
                           Context->InputBuffer,
                           Context->InputBufferLength,
                           HIDI2C_TIMEOUT_SHORT);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("hidi2c: input read failed 0x%08lX\n", Status);
        return TRUE;
    }

    /* The first two bytes are the report length, including themselves */
    ReportLength = (USHORT)(Context->InputBuffer[0] | (Context->InputBuffer[1] << 8));

    if (ReportLength == 0)
    {
        if (HidI2cFinishReset(Context))
            WdfTimerStop(Context->ResetTimer, FALSE);
        return TRUE;
    }

    /* Anything but the reset response is noise until the reset completes */
    if (Context->ResetPending != 0)
        return TRUE;

    if (ReportLength <= sizeof(USHORT) || ReportLength > Context->InputBufferLength)
    {
        DPRINT1("hidi2c: input report claims %u bytes, buffer is %lu\n",
                ReportLength, Context->InputBufferLength);
        return TRUE;
    }

    HidI2cDeliverReport(Context,
                        Context->InputBuffer + sizeof(USHORT),
                        ReportLength - sizeof(USHORT));
    return TRUE;
}

static
NTSTATUS
NTAPI
HidI2cGetDeviceDescriptor(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _Out_ PULONG_PTR Information)
{
    PHID_DESCRIPTOR HidDescriptor;
    NTSTATUS Status;

    Status = WdfRequestRetrieveOutputBuffer(Request,
                                            sizeof(*HidDescriptor),
                                            (PVOID *)&HidDescriptor,
                                            NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    /* hidclass only consumes the report descriptor length from this */
    RtlZeroMemory(HidDescriptor, sizeof(*HidDescriptor));
    HidDescriptor->bLength = sizeof(*HidDescriptor);
    HidDescriptor->bDescriptorType = HID_HID_DESCRIPTOR_TYPE;
    HidDescriptor->bcdHID = 0x0001;
    HidDescriptor->bNumDescriptors = 1;
    HidDescriptor->DescriptorList[0].bReportType = HID_REPORT_DESCRIPTOR_TYPE;
    HidDescriptor->DescriptorList[0].wReportLength = Context->Descriptor.ReportDescLength;

    *Information = sizeof(*HidDescriptor);
    return STATUS_SUCCESS;
}

static
NTSTATUS
NTAPI
HidI2cGetReportDescriptor(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _Out_ PULONG_PTR Information)
{
    ULONG Length = Context->Descriptor.ReportDescLength;
    NTSTATUS Status;
    PVOID Buffer;

    Status = WdfRequestRetrieveOutputBuffer(Request, Length, &Buffer, NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    Status = HidI2cSpbReadRegister(Context,
                                   Context->Descriptor.ReportDescRegister,
                                   Buffer,
                                   Length,
                                   HIDI2C_TIMEOUT_SHORT);
    if (NT_SUCCESS(Status))
        *Information = Length;

    return Status;
}

static
NTSTATUS
NTAPI
HidI2cGetDeviceAttributes(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ WDFREQUEST Request,
    _Out_ PULONG_PTR Information)
{
    PHID_DEVICE_ATTRIBUTES Attributes;
    NTSTATUS Status;

    Status = WdfRequestRetrieveOutputBuffer(Request,
                                            sizeof(*Attributes),
                                            (PVOID *)&Attributes,
                                            NULL);
    if (!NT_SUCCESS(Status))
        return Status;

    RtlZeroMemory(Attributes, sizeof(*Attributes));
    Attributes->Size = sizeof(*Attributes);
    Attributes->VendorID = Context->Descriptor.VendorId;
    Attributes->ProductID = Context->Descriptor.ProductId;
    Attributes->VersionNumber = Context->Descriptor.VersionId;

    *Information = sizeof(*Attributes);
    return STATUS_SUCCESS;
}

/**
 * @brief Handles the IOCTL_HID_* requests mshidkmdf forwards from hidclass.
 */
VOID
NTAPI
HidI2cEvtInternalDeviceControl(
    _In_ WDFQUEUE Queue,
    _In_ WDFREQUEST Request,
    _In_ size_t OutputBufferLength,
    _In_ size_t InputBufferLength,
    _In_ ULONG IoControlCode)
{
    PHIDI2C_CONTEXT Context = HidI2cGetContext(WdfIoQueueGetDevice(Queue));
    ULONG_PTR Information = 0;
    NTSTATUS Status;

    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    if (IoControlCode == IOCTL_HID_READ_REPORT)
    {
        /* Parked until the ISR has a report for it */
        Status = WdfRequestForwardToIoQueue(Request, Context->ReadQueue);
        if (!NT_SUCCESS(Status))
            WdfRequestComplete(Request, Status);
        return;
    }

    switch (IoControlCode)
    {
        case IOCTL_HID_GET_DEVICE_DESCRIPTOR:
            Status = HidI2cGetDeviceDescriptor(Context, Request, &Information);
            break;

        case IOCTL_HID_GET_REPORT_DESCRIPTOR:
            Status = HidI2cGetReportDescriptor(Context, Request, &Information);
            break;

        case IOCTL_HID_GET_DEVICE_ATTRIBUTES:
            Status = HidI2cGetDeviceAttributes(Context, Request, &Information);
            break;

        default:
            /* Feature, output report, string and idle requests are not implemented */
            Status = STATUS_NOT_SUPPORTED;
            break;
    }

    WdfRequestCompleteWithInformation(Request, Status, Information);
}
