/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     ReactOS ucxstub entry points, built against the WDK's ucxstub.lib
 *
 * ReactOS's wdfdriverentry does not walk .kmdfclassbind, so its clients bind
 * to UCX by hand. The WDK's FxDriverEntry binds every class whose stub is
 * linked in before DriverEntry runs, and unbinds after DriverUnload, so both
 * calls have nothing left to do here.
 */

#pragma once

_IRQL_requires_(PASSIVE_LEVEL)
__forceinline
NTSTATUS
UcxStubBindClass(VOID)
{
    return STATUS_SUCCESS;
}

_IRQL_requires_(PASSIVE_LEVEL)
__forceinline
VOID
UcxStubUnbindClass(VOID)
{
}
