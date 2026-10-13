/*
 * PROJECT:     ReactOS HID Stack
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     HID over I2C: SPB transport and the ACPI _DSM
 * COPYRIGHT:   Copyright 2026 Justin Miller <justinmiller100@gmail.com>
 */

#define NDEBUG
#include "hidi2cp.h"
#include <acpiioct.h>
#include <initguid.h>

DEFINE_GUID(GUID_HIDI2C_DSM,
            0x3CDFF6F7, 0x4267, 0x4555, 0xAD, 0x05, 0xB3, 0x0A, 0x3D, 0x89, 0x38, 0xDE);

/* Room for the header, the UUID buffer and three integer sized arguments */
#define HIDI2C_DSM_INPUT_SIZE 64

typedef union _HIDI2C_DSM_INPUT
{
    ACPI_EVAL_INPUT_BUFFER_COMPLEX Header;
    UCHAR Raw[HIDI2C_DSM_INPUT_SIZE];
} HIDI2C_DSM_INPUT, *PHIDI2C_DSM_INPUT;

static
VOID
NTAPI
HidI2cInitSendOptions(
    _Out_ PWDF_REQUEST_SEND_OPTIONS Options,
    _In_ ULONG TimeoutSeconds)
{
    WDF_REQUEST_SEND_OPTIONS_INIT(Options, 0);
    WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(Options, WDF_REL_TIMEOUT_IN_SEC(TimeoutSeconds));
}

/**
 * @brief Opens the resource hub path for our I2C connection id.
 */
NTSTATUS
NTAPI
HidI2cOpenSpbTarget(
    _In_ PHIDI2C_CONTEXT Context)
{
    WDF_IO_TARGET_OPEN_PARAMS OpenParams;
    WDF_OBJECT_ATTRIBUTES Attributes;
    WCHAR PathBuffer[RESOURCE_HUB_PATH_CHARS];
    UNICODE_STRING TargetPath;
    NTSTATUS Status;

    PAGED_CODE();

    RtlInitEmptyUnicodeString(&TargetPath, PathBuffer, sizeof(PathBuffer));

    Status = RESOURCE_HUB_CREATE_PATH_FROM_ID(&TargetPath,
                                              Context->ConnectionId.LowPart,
                                              Context->ConnectionId.HighPart);
    if (!NT_SUCCESS(Status))
        return Status;

    WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
    Attributes.ParentObject = Context->Device;

    Status = WdfIoTargetCreate(Context->Device, &Attributes, &Context->SpbTarget);
    if (!NT_SUCCESS(Status))
        return Status;

    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&OpenParams,
                                                &TargetPath,
                                                GENERIC_READ | GENERIC_WRITE);

    Status = WdfIoTargetOpen(Context->SpbTarget, &OpenParams);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("hidi2c: cannot open %wZ (0x%08lX)\n", &TargetPath, Status);
        WdfObjectDelete(Context->SpbTarget);
        Context->SpbTarget = NULL;
    }

    return Status;
}

VOID
NTAPI
HidI2cCloseSpbTarget(
    _In_ PHIDI2C_CONTEXT Context)
{
    if (Context->SpbTarget == NULL)
        return;

    WdfIoTargetClose(Context->SpbTarget);
    WdfObjectDelete(Context->SpbTarget);
    Context->SpbTarget = NULL;
}

NTSTATUS
NTAPI
HidI2cSpbWrite(
    _In_ PHIDI2C_CONTEXT Context,
    _In_reads_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length,
    _In_ ULONG TimeoutSeconds)
{
    WDF_REQUEST_SEND_OPTIONS Options;
    WDF_MEMORY_DESCRIPTOR Memory;

    HidI2cInitSendOptions(&Options, TimeoutSeconds);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&Memory, Buffer, Length);

    return WdfIoTargetSendWriteSynchronously(Context->SpbTarget,
                                             NULL,
                                             &Memory,
                                             NULL,
                                             &Options,
                                             NULL);
}

NTSTATUS
NTAPI
HidI2cSpbRead(
    _In_ PHIDI2C_CONTEXT Context,
    _Out_writes_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length,
    _In_ ULONG TimeoutSeconds)
{
    WDF_REQUEST_SEND_OPTIONS Options;
    WDF_MEMORY_DESCRIPTOR Memory;

    HidI2cInitSendOptions(&Options, TimeoutSeconds);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&Memory, Buffer, Length);

    return WdfIoTargetSendReadSynchronously(Context->SpbTarget,
                                            NULL,
                                            &Memory,
                                            NULL,
                                            &Options,
                                            NULL);
}

/**
 * @brief Writes a register address and reads its contents with a repeated start.
 */
NTSTATUS
NTAPI
HidI2cSpbReadRegister(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ USHORT Register,
    _Out_writes_bytes_(Length) PVOID Buffer,
    _In_ ULONG Length,
    _In_ ULONG TimeoutSeconds)
{
    SPB_TRANSFER_LIST_AND_ENTRIES(2) Sequence;
    WDF_REQUEST_SEND_OPTIONS Options;
    WDF_MEMORY_DESCRIPTOR Memory;
    NTSTATUS Status;

    SPB_TRANSFER_LIST_INIT(&Sequence.List, 2);
    Sequence.List.Transfers[0] =
        SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionToDevice,
                                            0,
                                            &Register,
                                            sizeof(Register));
    Sequence.List.Transfers[1] =
        SPB_TRANSFER_LIST_ENTRY_INIT_SIMPLE(SpbTransferDirectionFromDevice,
                                            0,
                                            Buffer,
                                            Length);

    HidI2cInitSendOptions(&Options, TimeoutSeconds);
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&Memory, &Sequence, sizeof(Sequence));

    Status = WdfIoTargetSendIoctlSynchronously(Context->SpbTarget,
                                               NULL,
                                               IOCTL_SPB_EXECUTE_SEQUENCE,
                                               &Memory,
                                               NULL,
                                               &Options,
                                               NULL);
    if (!NT_SUCCESS(Status))
        DPRINT1("hidi2c: read of register 0x%04X failed 0x%08lX\n", Register, Status);

    return Status;
}

/**
 * @brief Evaluates one function of the HID over I2C _DSM through the ACPI PDO.
 */
static
NTSTATUS
NTAPI
HidI2cEvaluateDsm(
    _In_ PHIDI2C_CONTEXT Context,
    _In_ ULONG FunctionIndex,
    _Out_ PACPI_EVAL_OUTPUT_BUFFER Output)
{
    WDF_MEMORY_DESCRIPTOR InputMemory;
    WDF_MEMORY_DESCRIPTOR OutputMemory;
    PACPI_METHOD_ARGUMENT Argument;
    HIDI2C_DSM_INPUT Input;

    PAGED_CODE();

    RtlZeroMemory(&Input, sizeof(Input));
    Input.Header.Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
    RtlCopyMemory(Input.Header.MethodName, "_DSM", sizeof(Input.Header.MethodName));
    Input.Header.Size = sizeof(Input);
    Input.Header.ArgumentCount = 4;

    Argument = Input.Header.Argument;
    ACPI_METHOD_SET_ARGUMENT_BUFFER(Argument, &GUID_HIDI2C_DSM, sizeof(GUID));

    Argument = ACPI_METHOD_NEXT_ARGUMENT(Argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(Argument, HIDI2C_DSM_REVISION);

    Argument = ACPI_METHOD_NEXT_ARGUMENT(Argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(Argument, FunctionIndex);

    /* _DSM needs its fourth argument even though both functions take no data */
    Argument = ACPI_METHOD_NEXT_ARGUMENT(Argument);
    ACPI_METHOD_SET_ARGUMENT_INTEGER(Argument, 0);
    Argument->Type = ACPI_METHOD_ARGUMENT_PACKAGE;

    RtlZeroMemory(Output, sizeof(*Output));

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&InputMemory, &Input, sizeof(Input));
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&OutputMemory, Output, sizeof(*Output));

    return WdfIoTargetSendIoctlSynchronously(WdfDeviceGetIoTarget(Context->Device),
                                             NULL,
                                             IOCTL_ACPI_EVAL_METHOD,
                                             &InputMemory,
                                             &OutputMemory,
                                             NULL,
                                             NULL);
}

/**
 * @brief Reads the HID descriptor register address from _DSM function 1.
 */
NTSTATUS
NTAPI
HidI2cAcpiGetDescriptorRegister(
    _In_ PHIDI2C_CONTEXT Context)
{
    ACPI_EVAL_OUTPUT_BUFFER Output;
    PACPI_METHOD_ARGUMENT Result;
    NTSTATUS Status;

    PAGED_CODE();

    /* Function 0 answers with a bitmap of the implemented functions */
    Status = HidI2cEvaluateDsm(Context, HIDI2C_DSM_FUNCTION_QUERY, &Output);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("hidi2c: _DSM query failed 0x%08lX\n", Status);
        return Status;
    }

    Result = Output.Argument;
    if (Result->Type != ACPI_METHOD_ARGUMENT_BUFFER || Result->DataLength == 0)
        return STATUS_INVALID_PARAMETER;

    if (!(Result->Data[0] & (1 << HIDI2C_DSM_FUNCTION_HID_DESC)))
    {
        DPRINT1("hidi2c: _DSM does not implement the HID descriptor function\n");
        return STATUS_NOT_SUPPORTED;
    }

    Status = HidI2cEvaluateDsm(Context, HIDI2C_DSM_FUNCTION_HID_DESC, &Output);
    if (!NT_SUCCESS(Status))
    {
        DPRINT1("hidi2c: _DSM function 1 failed 0x%08lX\n", Status);
        return Status;
    }

    Result = Output.Argument;
    if (Result->Type != ACPI_METHOD_ARGUMENT_INTEGER || Result->DataLength == 0)
    {
        DPRINT1("hidi2c: _DSM returned type %u, expected an integer\n", Result->Type);
        return STATUS_UNSUCCESSFUL;
    }

    Context->DescriptorRegister = (USHORT)Result->Argument;

    DPRINT("hidi2c: HID descriptor register is 0x%04X\n", Context->DescriptorRegister);
    return STATUS_SUCCESS;
}
