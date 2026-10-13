/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     wdfldr.sys class-library interface, which the public WDK omits
 *
 * Only what a class extension (ucx01000) and fxldr.h need. Layouts follow
 * Microsoft's open source WDF loader, since wdfldr.sys reads them directly.
 */

#ifndef __WDFLDR_H__
#define __WDFLDR_H__

#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef ULONG WDF_MAJOR_VERSION;
typedef ULONG WDF_MINOR_VERSION;
typedef ULONG WDF_BUILD_NUMBER;

typedef struct _WDF_BIND_INFO *PWDF_BIND_INFO;
typedef struct _WDF_CLASS_BIND_INFO *PWDF_CLASS_BIND_INFO;
typedef PVOID WDF_COMPONENT_GLOBALS, *PWDF_COMPONENT_GLOBALS;

typedef
NTSTATUS
(NTAPI *PWDF_LDR_DIAGNOSTICS_VALUE_BY_NAME_AS_ULONG)(
    _In_ PUNICODE_STRING ValueName,
    _Out_ PULONG Value);

typedef struct _WDF_INTERFACE_HEADER {
    const GUID *InterfaceType;
    ULONG InterfaceSize;
} WDF_INTERFACE_HEADER, *PWDF_INTERFACE_HEADER;

typedef struct _WDF_CLASS_VERSION {
    WDF_MAJOR_VERSION Major;
    WDF_MINOR_VERSION Minor;
    WDF_BUILD_NUMBER Build;
} WDF_CLASS_VERSION, *PWDF_CLASS_VERSION;

typedef
NTSTATUS
(NTAPI *PFN_WDF_VERSION_BIND_CLASS)(
    _In_ PWDF_BIND_INFO BindInfo,
    _Inout_ PWDF_COMPONENT_GLOBALS *Globals,
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo);

typedef
VOID
(NTAPI *PFN_WDF_VERSION_UNBIND_CLASS)(
    _In_ PWDF_BIND_INFO BindInfo,
    _In_ PWDF_COMPONENT_GLOBALS Globals,
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo);

typedef
NTSTATUS
(NTAPI *PFN_CLIENT_BIND_CLASS)(
    _In_ PFN_WDF_VERSION_BIND_CLASS BindFunction,
    _In_ PWDF_BIND_INFO BindInfo,
    _In_ PWDF_COMPONENT_GLOBALS Globals,
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo);

typedef
VOID
(NTAPI *PFN_CLIENT_UNBIND_CLASS)(
    _In_ PFN_WDF_VERSION_UNBIND_CLASS UnbindFunction,
    _In_ PWDF_BIND_INFO BindInfo,
    _In_ PWDF_COMPONENT_GLOBALS Globals,
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo);

typedef struct _WDF_CLASS_BIND_INFO {
    ULONG Size;
    PWCHAR ClassName;
    WDF_CLASS_VERSION Version;
    VOID (NTAPI **FunctionTable)(VOID);
    ULONG FunctionTableCount;
    PVOID ClassBindInfo;
    PFN_CLIENT_BIND_CLASS ClientBindClass;
    PFN_CLIENT_UNBIND_CLASS ClientUnbindClass;
    PVOID ClassModule;
} WDF_CLASS_BIND_INFO;

/* Callbacks a class extension hands wdfldr in WdfRegisterClassLibrary */

typedef
NTSTATUS
(NTAPI *PFN_CLASS_LIBRARY_INIT)(VOID);

typedef
VOID
(NTAPI *PFN_CLASS_LIBRARY_DEINIT)(VOID);

typedef
NTSTATUS
(NTAPI *PFN_CLASS_LIBRARY_BIND_CLIENT)(
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo,
    _Out_ PWDF_COMPONENT_GLOBALS *Globals);

typedef
VOID
(NTAPI *PFN_CLASS_LIBRARY_UNBIND_CLIENT)(
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo,
    _In_ PWDF_COMPONENT_GLOBALS *Globals);

typedef struct _WDF_CLASS_LIBRARY_INFO {
    ULONG Size;
    WDF_CLASS_VERSION Version;
    PFN_CLASS_LIBRARY_INIT ClassLibraryInitialize;
    PFN_CLASS_LIBRARY_DEINIT ClassLibraryDeinitialize;
    PFN_CLASS_LIBRARY_BIND_CLIENT ClassLibraryBindClient;
    PFN_CLASS_LIBRARY_UNBIND_CLIENT ClassLibraryUnbindClient;
} WDF_CLASS_LIBRARY_INFO, *PWDF_CLASS_LIBRARY_INFO;

/* wdfldr.sys exports, all in the WDK's wdfldr.lib */

_Must_inspect_result_
NTSTATUS
NTAPI
WdfRegisterClassLibrary(
    _In_ PWDF_CLASS_LIBRARY_INFO ClassLibInfo,
    _In_ PUNICODE_STRING SourceString,
    _In_ PUNICODE_STRING ObjectName);

_Must_inspect_result_
NTSTATUS
NTAPI
WdfVersionBindClass(
    _In_ PWDF_BIND_INFO BindInfo,
    _Inout_ PWDF_COMPONENT_GLOBALS *Globals,
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo);

VOID
NTAPI
WdfVersionUnbindClass(
    _In_ PWDF_BIND_INFO BindInfo,
    _In_ PWDF_COMPONENT_GLOBALS Globals,
    _In_ PWDF_CLASS_BIND_INFO ClassBindInfo);

NTSTATUS
NTAPI
WdfLdrQueryInterface(
    _In_ PWDF_INTERFACE_HEADER LoaderInterface);

#ifdef __cplusplus
}
#endif

#endif /* __WDFLDR_H__ */
