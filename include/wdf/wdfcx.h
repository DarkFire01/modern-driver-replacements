/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     KMDF class extension device-init interface, which the public WDK omits
 *
 * Include after <wdf.h>. The table indexes come from the KMDF 1.15 wdffuncenum.h.
 */

#pragma once

#ifndef WDFAPI
#error Include wdf.h first
#endif

typedef
_Function_class_(EVT_WDFCX_DEVICE_FILE_CREATE)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
NTAPI
EVT_WDFCX_DEVICE_FILE_CREATE(
    _In_ WDFDEVICE Device,
    _In_ WDFREQUEST Request,
    _In_ WDFFILEOBJECT FileObject);

typedef EVT_WDFCX_DEVICE_FILE_CREATE *PFN_WDFCX_DEVICE_FILE_CREATE;

typedef struct _WDFCX_FILEOBJECT_CONFIG
{
    ULONG Size;
    PFN_WDFCX_DEVICE_FILE_CREATE EvtCxDeviceFileCreate;
    PFN_WDF_FILE_CLOSE EvtFileClose;
    PFN_WDF_FILE_CLEANUP EvtFileCleanup;
    WDF_TRI_STATE AutoForwardCleanupClose;
    WDF_FILEOBJECT_CLASS FileObjectClass;
} WDFCX_FILEOBJECT_CONFIG, *PWDFCX_FILEOBJECT_CONFIG;

FORCEINLINE
VOID
WDFCX_FILEOBJECT_CONFIG_INIT(
    _Out_ PWDFCX_FILEOBJECT_CONFIG FileEventCallbacks,
    _In_opt_ PFN_WDFCX_DEVICE_FILE_CREATE EvtCxDeviceFileCreate,
    _In_opt_ PFN_WDF_FILE_CLOSE EvtFileClose,
    _In_opt_ PFN_WDF_FILE_CLEANUP EvtFileCleanup)
{
    RtlZeroMemory(FileEventCallbacks, sizeof(*FileEventCallbacks));

    FileEventCallbacks->Size = sizeof(*FileEventCallbacks);
    FileEventCallbacks->EvtCxDeviceFileCreate = EvtCxDeviceFileCreate;
    FileEventCallbacks->EvtFileClose = EvtFileClose;
    FileEventCallbacks->EvtFileCleanup = EvtFileCleanup;
    FileEventCallbacks->FileObjectClass = WdfFileObjectWdfCannotUseFsContexts;
    FileEventCallbacks->AutoForwardCleanupClose = WdfUseDefault;
}

/* Forward the IRP onward with WdfDeviceWdmDispatchIrp and the same DispatchContext */
typedef
_Function_class_(EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
NTSTATUS
NTAPI
EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS(
    _In_ WDFDEVICE Device,
    _Inout_ PIRP Irp,
    _In_ WDFCONTEXT DispatchContext);

typedef EVT_WDFCXDEVICE_WDM_IRP_PREPROCESS *PFN_WDFCXDEVICE_WDM_IRP_PREPROCESS;

typedef
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
PWDFCXDEVICE_INIT
(NTAPI *PFN_WDFCXDEVICEINITALLOCATE)(
    _In_ PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_ PWDFDEVICE_INIT DeviceInit);

typedef
_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
NTSTATUS
(NTAPI *PFN_WDFCXDEVICEINITASSIGNWDMIRPPREPROCESSCALLBACK)(
    _In_ PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PFN_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtCxDeviceWdmIrpPreprocess,
    _In_ UCHAR MajorFunction,
    _When_(NumMinorFunctions > 0, _In_reads_bytes_(NumMinorFunctions))
    _When_(NumMinorFunctions == 0, _In_opt_)
    PUCHAR MinorFunctions,
    _In_ ULONG NumMinorFunctions);

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
(NTAPI *PFN_WDFCXDEVICEINITSETIOINCALLERCONTEXTCALLBACK)(
    _In_ PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PFN_WDF_IO_IN_CALLER_CONTEXT EvtIoInCallerContext);

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
(NTAPI *PFN_WDFCXDEVICEINITSETREQUESTATTRIBUTES)(
    _In_ PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PWDF_OBJECT_ATTRIBUTES RequestAttributes);

typedef
_IRQL_requires_max_(DISPATCH_LEVEL)
WDFAPI
VOID
(NTAPI *PFN_WDFCXDEVICEINITSETFILEOBJECTCONFIG)(
    _In_ PWDF_DRIVER_GLOBALS DriverGlobals,
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PWDFCX_FILEOBJECT_CONFIG CxFileObjectConfig,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES FileObjectAttributes);

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
FORCEINLINE
PWDFCXDEVICE_INIT
WdfCxDeviceInitAllocate(
    _In_ PWDFDEVICE_INIT DeviceInit)
{
    return ((PFN_WDFCXDEVICEINITALLOCATE)WdfFunctions[WdfCxDeviceInitAllocateTableIndex])(
        WdfDriverGlobals,
        DeviceInit);
}

_Must_inspect_result_
_IRQL_requires_max_(DISPATCH_LEVEL)
FORCEINLINE
NTSTATUS
WdfCxDeviceInitAssignWdmIrpPreprocessCallback(
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PFN_WDFCXDEVICE_WDM_IRP_PREPROCESS EvtCxDeviceWdmIrpPreprocess,
    _In_ UCHAR MajorFunction,
    _When_(NumMinorFunctions > 0, _In_reads_bytes_(NumMinorFunctions))
    _When_(NumMinorFunctions == 0, _In_opt_)
    PUCHAR MinorFunctions,
    _In_ ULONG NumMinorFunctions)
{
    return ((PFN_WDFCXDEVICEINITASSIGNWDMIRPPREPROCESSCALLBACK)
                WdfFunctions[WdfCxDeviceInitAssignWdmIrpPreprocessCallbackTableIndex])(
        WdfDriverGlobals,
        CxDeviceInit,
        EvtCxDeviceWdmIrpPreprocess,
        MajorFunction,
        MinorFunctions,
        NumMinorFunctions);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
FORCEINLINE
VOID
WdfCxDeviceInitSetIoInCallerContextCallback(
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PFN_WDF_IO_IN_CALLER_CONTEXT EvtIoInCallerContext)
{
    ((PFN_WDFCXDEVICEINITSETIOINCALLERCONTEXTCALLBACK)
         WdfFunctions[WdfCxDeviceInitSetIoInCallerContextCallbackTableIndex])(
        WdfDriverGlobals,
        CxDeviceInit,
        EvtIoInCallerContext);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
FORCEINLINE
VOID
WdfCxDeviceInitSetRequestAttributes(
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PWDF_OBJECT_ATTRIBUTES RequestAttributes)
{
    ((PFN_WDFCXDEVICEINITSETREQUESTATTRIBUTES)
         WdfFunctions[WdfCxDeviceInitSetRequestAttributesTableIndex])(
        WdfDriverGlobals,
        CxDeviceInit,
        RequestAttributes);
}

_IRQL_requires_max_(DISPATCH_LEVEL)
FORCEINLINE
VOID
WdfCxDeviceInitSetFileObjectConfig(
    _In_ PWDFCXDEVICE_INIT CxDeviceInit,
    _In_ PWDFCX_FILEOBJECT_CONFIG CxFileObjectConfig,
    _In_opt_ PWDF_OBJECT_ATTRIBUTES FileObjectAttributes)
{
    ((PFN_WDFCXDEVICEINITSETFILEOBJECTCONFIG)
         WdfFunctions[WdfCxDeviceInitSetFileObjectConfigTableIndex])(
        WdfDriverGlobals,
        CxDeviceInit,
        CxFileObjectConfig,
        FileObjectAttributes);
}
