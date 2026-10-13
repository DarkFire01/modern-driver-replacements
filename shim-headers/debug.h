/*
 * PROJECT:     modern-driver-replacements
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     ReactOS-style DPRINT macros
 *
 * Raise the level with, e.g.
 *     ed nt!Kd_IHVDRIVER_Mask 0xffffffff
 */

#ifndef _MDR_DEBUG_H
#define _MDR_DEBUG_H

#include <ntddk.h>

#ifndef DPFLTR_IHVDRIVER_ID
#define DPFLTR_IHVDRIVER_ID 77
#endif

#undef DPRINT
#undef DPRINT1

#define DPRINT1(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, __VA_ARGS__)

#ifdef NDEBUG
#define DPRINT(...) ((void)0)
#else
#define DPRINT(...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_TRACE_LEVEL, __VA_ARGS__)
#endif

#ifndef UNIMPLEMENTED
#define UNIMPLEMENTED       DPRINT1("%s is UNIMPLEMENTED\n", __FUNCTION__)
#endif

#endif /* _MDR_DEBUG_H */
