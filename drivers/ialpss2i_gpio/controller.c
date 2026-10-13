/*
 * PROJECT:     ReactOS Intel LPSS GPIO controller driver
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Controller lifetime and geometry
 * COPYRIGHT:   Copyright 2026 Justin Miller <justin.miller@reactos.org>
 */

#include "gpiopriv.h"

#define NDEBUG
#include <debug.h>

/*
 * Pad groups per part. A part missing here must not be bound: the community
 * count, group count and pad offsets all differ, so a borrowed table would
 * write the wrong pads. Broxton, Gemini Lake and Lakefield expose one community
 * per ACPI device and are left out.
 */

/* Skylake-LP */
static const GPIO_BANK_DESCRIPTOR GpioBanksSkylakeLp[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      24    },   /* GPP_A */
    {     0,          1,       0x0C0,      24    },   /* GPP_B */
    {     1,          0,       0x000,      24    },   /* GPP_C */
    {     1,          1,       0x0C0,      24    },   /* GPP_D */
    {     1,          2,       0x180,      24    },   /* GPP_E */
    {     2,          0,       0x000,      24    },   /* GPP_F */
    {     2,          1,       0x0C0,       8    },   /* GPP_G */
};

/* Skylake-H */
static const GPIO_BANK_DESCRIPTOR GpioBanksSkylakeH[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      24    },   /* GPP_A */
    {     0,          1,       0x0C0,      24    },   /* GPP_B */
    {     1,          0,       0x000,      24    },   /* GPP_C */
    {     1,          1,       0x0C0,      24    },   /* GPP_D */
    {     1,          2,       0x180,      13    },   /* GPP_E */
    {     1,          3,       0x1E8,      24    },   /* GPP_F */
    {     1,          4,       0x2A8,      24    },   /* GPP_G */
    {     1,          5,       0x368,      24    },   /* GPP_H */
    {     2,          1,       0x000,      11    },   /* GPD   */
};

/* Cannon Lake-LP */
static const GPIO_BANK_DESCRIPTOR GpioBanksCannonLakeLp[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      25    },   /* GPP_A  */
    {     0,          1,       0x190,      26    },   /* GPP_B  */
    {     0,          2,       0x330,       8    },   /* GPP_G  */
    {     1,          0,       0x000,      25    },   /* GPP_D  */
    {     1,          1,       0x190,      24    },   /* GPP_F  */
    {     1,          2,       0x310,      24    },   /* GPP_H  */
    {     1,          3,       0x490,      32    },   /* vGPIO  */
    {     1,          4,       0x690,       8    },   /* vGPIO2 */
    {     2,          0,       0x000,      24    },   /* GPP_C  */
    {     2,          1,       0x180,      24    },   /* GPP_E  */
};

/* Cannon Lake-H */
static const GPIO_BANK_DESCRIPTOR GpioBanksCannonLakeH[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      25    },
    {     0,          1,       0x190,      26    },
    {     1,          0,       0x000,      24    },
    {     1,          1,       0x180,      24    },
    {     1,          2,       0x300,       8    },
    {     1,          4,       0x400,      32    },
    {     2,          0,       0x000,      24    },
    {     2,          1,       0x180,      24    },
    {     2,          2,       0x300,      13    },
    {     2,          3,       0x3D0,      24    },
    {     3,          2,       0x140,      18    },
    {     3,          3,       0x260,      12    },
};

/* Ice Lake-LP */
static const GPIO_BANK_DESCRIPTOR GpioBanksIceLakeLp[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,       8    },   /* GPP_G */
    {     0,          1,       0x080,      26    },   /* GPP_B */
    {     0,          2,       0x220,      25    },   /* GPP_A */
    {     1,          0,       0x000,      24    },   /* GPP_H */
    {     1,          1,       0x180,      21    },   /* GPP_D */
    {     1,          2,       0x2D0,      20    },   /* GPP_F */
    {     1,          3,       0x410,      29    },   /* GPP_V */
    {     2,          0,       0x000,      24    },   /* GPP_C */
    {     2,          2,       0x1E0,      24    },   /* GPP_E */
    {     3,          0,       0x000,       8    },   /* GPP_R */
    {     3,          1,       0x080,       8    },   /* GPP_S */
};

/* Ice Lake-N */
static const GPIO_BANK_DESCRIPTOR GpioBanksIceLakeN[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          1,       0x090,      26    },   /* GPP_B */
    {     0,          2,       0x230,      21    },   /* GPP_A */
    {     0,          3,       0x380,       8    },   /* GPP_S */
    {     0,          4,       0x400,       8    },   /* GPP_R */
    {     1,          0,       0x000,      24    },   /* GPP_H */
    {     1,          1,       0x180,      26    },   /* GPP_D */
    {     1,          2,       0x320,      29    },   /* GPP_V0 */
    {     1,          3,       0x4F0,      24    },   /* GPP_C */
    {     2,          1,       0x060,      24    },   /* GPP_E */
    {     2,          3,       0x270,       4    },   /* GPP_V4 */
    {     3,          0,       0x000,       8    },   /* GPP_G */
};

/* Ice Lake-H */
static const GPIO_BANK_DESCRIPTOR GpioBanksIceLakeH[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          1,       0x090,      25    },   /* GPP_A */
    {     0,          2,       0x220,      26    },   /* GPP_B */
    {     1,          0,       0x000,      24    },   /* GPP_C */
    {     1,          1,       0x180,      18    },   /* GPP_D */
    {     1,          2,       0x2A0,       8    },   /* GPP_R */
    {     1,          3,       0x320,       8    },   /* GPP_S */
    {     1,          4,       0x3A0,      16    },   /* GPP_G */
    {     1,          5,       0x4A0,      29    },   /* GPP_V */
    {     2,          0,       0x000,      13    },   /* GPP_E */
    {     2,          1,       0x0D0,      24    },   /* GPP_F */
    {     3,          0,       0x000,      24    },   /* GPP_H */
    {     3,          1,       0x180,      12    },   /* GPP_K */
    {     3,          2,       0x240,      10    },   /* GPP_J */
    {     4,          0,       0x000,      18    },   /* GPP_I */
};

/* Tiger Lake-LP */
static const GPIO_BANK_DESCRIPTOR GpioBanksTigerLakeLp[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      26    },   /* GPP_B */
    {     0,          1,       0x1A0,      16    },   /* GPP_T */
    {     0,          2,       0x2A0,      25    },   /* GPP_A */
    {     1,          0,       0x000,       8    },   /* GPP_S */
    {     1,          1,       0x080,      24    },   /* GPP_H */
    {     1,          2,       0x200,      21    },   /* GPP_D */
    {     1,          3,       0x350,      24    },   /* GPP_U */
    {     1,          4,       0x4D0,      27    },   /* GPP_V */
    {     2,          0,       0x000,      24    },   /* GPP_C */
    {     2,          1,       0x180,      25    },   /* GPP_F */
    {     2,          3,       0x370,      25    },   /* GPP_E */
    {     3,          0,       0x000,       8    },   /* GPP_R */
};

/* Tiger Lake-H */
static const GPIO_BANK_DESCRIPTOR GpioBanksTigerLakeH[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x080,      15    },   /* GPP_A */
    {     0,          1,       0x190,      20    },   /* GPP_R */
    {     0,          2,       0x2D0,      26    },   /* GPP_B */
    {     0,          3,       0x470,       8    },   /* GPP_v0 */
    {     1,          0,       0x000,      26    },   /* GPP_D */
    {     1,          1,       0x1A0,      24    },   /* GPP_C */
    {     1,          2,       0x320,       8    },   /* GPP_S */
    {     1,          3,       0x3A0,      17    },   /* GPP_G */
    {     1,          4,       0x4B0,      27    },   /* GPP_V */
    {     2,          0,       0x000,      13    },   /* GPP_E */
    {     2,          1,       0x0D0,      24    },   /* GPP_F */
    {     3,          0,       0x000,      24    },   /* GPP_H */
    {     3,          1,       0x180,      10    },   /* GPP_J */
    {     3,          2,       0x220,      15    },   /* GPP_k */
    {     4,          0,       0x000,      15    },   /* GPP_I */
};

/* Alder Lake-P */
static const GPIO_BANK_DESCRIPTOR GpioBanksAlderLakeP[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      26    },   /* GPP_B */
    {     0,          1,       0x1A0,      16    },   /* GPP_T */
    {     0,          2,       0x2A0,      25    },   /* GPP_A */
    {     1,          0,       0x000,       8    },   /* GPP_S */
    {     1,          1,       0x080,      24    },   /* GPP_H */
    {     1,          2,       0x200,      21    },   /* GPP_D */
    {     1,          3,       0x350,      24    },   /* GPP_U */
    {     1,          4,       0x4D0,      29    },   /* GPP_V */
    {     2,          0,       0x000,      24    },   /* GPP_C */
    {     2,          1,       0x180,      25    },   /* GPP_F */
    {     2,          3,       0x370,      25    },   /* GPP_E */
    {     3,          0,       0x000,       8    },   /* GPP_R */
};

/* Alder Lake-N */
static const GPIO_BANK_DESCRIPTOR GpioBanksAlderLakeN[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      26    },   /* GPP_B */
    {     0,          1,       0x1A0,      16    },   /* GPP_T */
    {     0,          2,       0x2A0,      25    },   /* GPP_A */
    {     1,          0,       0x000,       8    },   /* GPP_S */
    {     1,          1,       0x080,      20    },   /* GPP_I */
    {     1,          2,       0x1C0,      24    },   /* GPP_H */
    {     1,          3,       0x340,      21    },   /* GPP_D */
    {     1,          4,       0x490,      29    },   /* GPP_V */
    {     2,          0,       0x000,      24    },   /* GPP_C */
    {     2,          1,       0x180,      25    },   /* GPP_F */
    {     2,          3,       0x370,      25    },   /* GPP_E */
    {     3,          0,       0x000,       8    },   /* GPP_R */
};

/* Alder Lake-S and Raptor Lake-S */
static const GPIO_BANK_DESCRIPTOR GpioBanksAlderLakeS[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      25    },   /* GPP_I */
    {     0,          1,       0x190,      23    },   /* GPP_R */
    {     0,          2,       0x300,      12    },   /* GPP_J */
    {     0,          3,       0x3C0,      27    },   /* GPP_V */
    {     0,          4,       0x570,       8    },   /* GPP_V_USB */
    {     1,          0,       0x000,      24    },   /* GPP_B */
    {     1,          1,       0x180,       8    },   /* GPP_G */
    {     1,          2,       0x200,      24    },   /* GPP_H */
    {     2,          1,       0x090,      16    },   /* GPP_A */
    {     2,          2,       0x190,      24    },   /* GPP_C */
    {     3,          0,       0x000,       8    },   /* GPP_S */
    {     3,          1,       0x080,      23    },   /* GPP_E */
    {     3,          2,       0x1F0,      15    },   /* GPP_K */
    {     3,          3,       0x2E0,      24    },   /* GPP_F */
    {     4,          0,       0x000,      25    },   /* GPP_D */
};

/* Meteor Lake-M */
static const GPIO_BANK_DESCRIPTOR GpioBanksMeteorLakeM[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      28    },   /* GPP_A */
    {     0,          1,       0x1C0,      19    },   /* vGPIO */
    {     0,          2,       0x2F0,      27    },   /* GPP_C */
    {     1,          0,       0x000,      20    },   /* GPP_B */
    {     1,          1,       0x140,       2    },   /* vGPIO3 */
    {     1,          2,       0x160,      24    },   /* GPP_D */
    {     2,          0,       0x000,      16    },   /* JTAG */
    {     2,          1,       0x100,      12    },   /* vGPIO4 */
};

/* Meteor Lake-P */
static const GPIO_BANK_DESCRIPTOR GpioBanksMeteorLakeP[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,       5    },   /* CPU */
    {     0,          1,       0x050,      24    },   /* GPP_V */
    {     0,          2,       0x1D0,      24    },   /* GPP_C */
    {     1,          0,       0x000,      25    },   /* GPP_A */
    {     1,          1,       0x190,      25    },   /* GPP_E */
    {     2,          0,       0x000,      26    },   /* GPP_H */
    {     2,          1,       0x1A0,      26    },   /* GPP_F */
    {     2,          2,       0x340,      15    },   /* SPI0 */
    {     2,          3,       0x430,      18    },   /* GPP_V3 */
    {     3,          0,       0x000,       8    },   /* GPP_S */
    {     3,          1,       0x080,      12    },   /* JTAG */
    {     4,          0,       0x000,      25    },   /* GPP_B */
    {     4,          1,       0x190,      25    },   /* GPP_D */
    {     4,          2,       0x320,      32    },   /* vGPIO0 */
    {     4,          3,       0x520,       3    },   /* vGPIO1 */
};

/* Meteor Lake-S */
static const GPIO_BANK_DESCRIPTOR GpioBanksMeteorLakeS[] =
{
    /* Community  HostSwOwn  PadOffset  PinCount */
    {     0,          0,       0x000,      25    },   /* GPP_D */
    {     0,          1,       0x190,      14    },   /* GPP_R */
    {     0,          2,       0x270,      18    },   /* GPP_J */
    {     0,          3,       0x390,      31    },   /* vGPIO */
    {     1,          0,       0x000,      15    },   /* GPP_A */
    {     1,          1,       0x0F0,      12    },   /* ESPI */
    {     1,          2,       0x1B0,      22    },   /* GPP_B */
    {     2,          0,       0x000,       9    },   /* SPI0 */
    {     2,          1,       0x090,      24    },   /* GPP_C */
    {     2,          2,       0x210,      20    },   /* GPP_H */
    {     2,          3,       0x350,       4    },   /* vGPIO3 */
    {     2,          4,       0x390,       8    },   /* vGPIO0 */
    {     2,          5,       0x410,      31    },   /* vGPIO4 */
    {     3,          0,       0x000,       8    },   /* GPP_S */
    {     3,          1,       0x080,      23    },   /* GPP_E */
    {     3,          2,       0x1F0,      14    },   /* GPP_K */
    {     3,          3,       0x2D0,      24    },   /* GPP_F */
    {     4,          0,       0x000,      21    },   /* GPP_I */
    {     4,          1,       0x150,      16    },   /* JTAG */
};

#define GPIO_LAYOUT_SKL(Id, Name, Table, Communities) \
    { Id, Name, Table, RTL_NUMBER_OF(Table), Communities, \
      GPIO_COMMUNITY_HOSTSW_OWN_SKL, GPIO_PAD_STRIDE_2REG, 24 }

#define GPIO_LAYOUT_CNL(Id, Name, Table, Communities, HostSwOwn) \
    { Id, Name, Table, RTL_NUMBER_OF(Table), Communities, \
      HostSwOwn, GPIO_PAD_STRIDE_4REG, 32 }

static const GPIO_LAYOUT GpioLayouts[] =
{
    GPIO_LAYOUT_SKL(L"ACPI\\INT344B", "Skylake-LP", GpioBanksSkylakeLp, 3),
    GPIO_LAYOUT_SKL(L"ACPI\\INT345D", "Skylake-H", GpioBanksSkylakeH, 3),
    GPIO_LAYOUT_SKL(L"ACPI\\INT3451", "Skylake-H", GpioBanksSkylakeH, 3),
    GPIO_LAYOUT_CNL(L"ACPI\\INT34BB", "Cannon Lake-LP", GpioBanksCannonLakeLp, 3, GPIO_COMMUNITY_HOSTSW_OWN_CNL),
    GPIO_LAYOUT_CNL(L"ACPI\\INT3450", "Cannon Lake-H", GpioBanksCannonLakeH, 4, GPIO_COMMUNITY_HOSTSW_OWN_CNL_H),
    GPIO_LAYOUT_CNL(L"ACPI\\INT3455", "Ice Lake-LP", GpioBanksIceLakeLp, 4, GPIO_COMMUNITY_HOSTSW_OWN_CNL),
    GPIO_LAYOUT_CNL(L"ACPI\\INT34C3", "Ice Lake-N", GpioBanksIceLakeN, 4, GPIO_COMMUNITY_HOSTSW_OWN_CNL),
    GPIO_LAYOUT_CNL(L"ACPI\\INT3456", "Ice Lake-H", GpioBanksIceLakeH, 5, GPIO_COMMUNITY_HOSTSW_OWN_CNL_H),
    GPIO_LAYOUT_CNL(L"ACPI\\INT34C5", "Tiger Lake-LP", GpioBanksTigerLakeLp, 4, GPIO_COMMUNITY_HOSTSW_OWN_CNL),
    GPIO_LAYOUT_CNL(L"ACPI\\INT34C6", "Tiger Lake-H", GpioBanksTigerLakeH, 5, GPIO_COMMUNITY_HOSTSW_OWN_CNL_H),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1055", "Alder Lake-P", GpioBanksAlderLakeP, 4, GPIO_COMMUNITY_HOSTSW_OWN_CNL),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1057", "Alder Lake-N", GpioBanksAlderLakeN, 4, GPIO_COMMUNITY_HOSTSW_OWN_CNL),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1085", "Alder Lake-S", GpioBanksAlderLakeS, 5, GPIO_COMMUNITY_HOSTSW_OWN_ADL_S),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1056", "Raptor Lake-S", GpioBanksAlderLakeS, 5, GPIO_COMMUNITY_HOSTSW_OWN_ADL_S),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1082", "Meteor Lake-M", GpioBanksMeteorLakeM, 3, GPIO_COMMUNITY_HOSTSW_OWN_MTL_M),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1083", "Meteor Lake-P", GpioBanksMeteorLakeP, 5, GPIO_COMMUNITY_HOSTSW_OWN_MTL_P),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC105E", "Meteor Lake-P", GpioBanksMeteorLakeP, 5, GPIO_COMMUNITY_HOSTSW_OWN_MTL_P),
    GPIO_LAYOUT_CNL(L"ACPI\\INTC1084", "Meteor Lake-S", GpioBanksMeteorLakeS, 5, GPIO_COMMUNITY_HOSTSW_OWN_ADL_S),
};

/**
 * @brief
 * The layout matching one of a device's hardware ids, or NULL.
 */
static
const GPIO_LAYOUT *
NTAPI
GpioMatchLayout(
    _In_ PCWSTR HardwareIds)
{
    PCWSTR Id;
    ULONG Index;

    /* The most specific id comes first in the multi string */
    for (Id = HardwareIds; *Id != UNICODE_NULL; Id += wcslen(Id) + 1)
    {
        for (Index = 0; Index < RTL_NUMBER_OF(GpioLayouts); Index++)
        {
            if (_wcsicmp(Id, GpioLayouts[Index].HardwareId) == 0)
                return &GpioLayouts[Index];
        }
    }

    return NULL;
}

/**
 * @brief
 * Finds the pad geometry for the part the device enumerated as.
 */
static
const GPIO_LAYOUT *
NTAPI
GpioFindLayout(
    _In_ WDFDEVICE Device)
{
    const GPIO_LAYOUT *Layout;
    PDEVICE_OBJECT Pdo;
    ULONG Length = 0;
    NTSTATUS Status;
    PWSTR Ids;

    PAGED_CODE();

    Pdo = WdfDeviceWdmGetPhysicalDevice(Device);
    Status = IoGetDeviceProperty(Pdo, DevicePropertyHardwareID, 0, NULL, &Length);
    if (Status != STATUS_BUFFER_TOO_SMALL || Length == 0)
    {
        DPRINT1("GPIO: no hardware id 0x%08lx\n", Status);
        return NULL;
    }

    /* Room for a second terminator, so a malformed list still ends */
    Ids = ExAllocatePoolZero(PagedPool, Length + 2 * sizeof(WCHAR), GPIO_POOL_TAG);
    if (Ids == NULL)
        return NULL;

    Layout = NULL;
    Status = IoGetDeviceProperty(Pdo, DevicePropertyHardwareID, Length, Ids, &Length);
    if (NT_SUCCESS(Status))
        Layout = GpioMatchLayout(Ids);
    else
        DPRINT1("GPIO: hardware id read failed 0x%08lx\n", Status);

    if (Layout != NULL)
    {
        DPRINT("GPIO: %s, %lu groups in %lu communities\n",
               Layout->Name,
               Layout->BankCount,
               Layout->CommunityCount);
    }
    else
    {
        DPRINT1("GPIO: no pad geometry for %S\n", Ids);
    }

    ExFreePoolWithTag(Ids, GPIO_POOL_TAG);
    return Layout;
}

/**
 * @brief
 * Maps the community windows, one memory resource each, in resource order.
 */
static
NTSTATUS
NTAPI
GpioMapCommunities(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ WDFCMRESLIST ResourcesTranslated)
{
    PCM_PARTIAL_RESOURCE_DESCRIPTOR Descriptor;
    ULONG Wanted = Controller->Layout->CommunityCount;
    ULONG Count;
    ULONG Index;
    PUCHAR Base;

    Count = WdfCmResourceListGetCount(ResourcesTranslated);
    for (Index = 0; Index < Count && Controller->CommunityCount < Wanted; Index++)
    {
        Descriptor = WdfCmResourceListGetDescriptor(ResourcesTranslated, Index);
        if (Descriptor == NULL || Descriptor->Type != CmResourceTypeMemory)
            continue;

        Base = MmMapIoSpace(Descriptor->u.Memory.Start, Descriptor->u.Memory.Length, MmNonCached);
        if (Base == NULL)
            return STATUS_INSUFFICIENT_RESOURCES;

        Controller->CommunityBase[Controller->CommunityCount] = Base;
        Controller->CommunityLength[Controller->CommunityCount] = Descriptor->u.Memory.Length;
        Controller->CommunityCount++;
    }

    if (Controller->CommunityCount < Wanted)
    {
        DPRINT1("GPIO: %lu of %lu community windows\n", Controller->CommunityCount, Wanted);
        return STATUS_DEVICE_CONFIGURATION_ERROR;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioPrepareController(
    WDFDEVICE Device,
    PVOID Context,
    WDFCMRESLIST ResourcesRaw,
    WDFCMRESLIST ResourcesTranslated)
{
    PGPIO_CONTROLLER Controller = Context;
    const GPIO_LAYOUT *Layout;
    NTSTATUS Status;
    ULONG Index;

    UNREFERENCED_PARAMETER(ResourcesRaw);

    RtlZeroMemory(Controller, sizeof(*Controller));
    Controller->Device = Device;

    /* Guessing a geometry would drive real pads at the wrong addresses */
    Layout = GpioFindLayout(Device);
    if (Layout == NULL || Layout->BankCount > GPIO_MAX_BANKS)
        return STATUS_DEVICE_CONFIGURATION_ERROR;

    Controller->Layout = Layout;
    Controller->Banks = Layout->Banks;
    Controller->BankCount = Layout->BankCount;

    Status = GpioMapCommunities(Controller, ResourcesTranslated);
    if (!NT_SUCCESS(Status))
        goto Fail;

    /* Full banks, then however many pads the last group has */
    Controller->PinsPerBank = Layout->PinsPerBank;
    Controller->TotalPins = (USHORT)(Layout->PinsPerBank * (Layout->BankCount - 1) +
                                     Layout->Banks[Layout->BankCount - 1].PinCount);

    for (Index = 0; Index < Controller->BankCount; Index++)
    {
        Controller->BankState[Index].Pins =
            ExAllocatePoolZero(NonPagedPoolNx,
                               Controller->Banks[Index].PinCount * sizeof(GPIO_PIN_STATE),
                               GPIO_POOL_TAG);
        if (Controller->BankState[Index].Pins == NULL)
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Fail;
        }
    }

    return STATUS_SUCCESS;

Fail:
    GpioReleaseController(Device, Context);
    return Status;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioReleaseController(
    WDFDEVICE Device,
    PVOID Context)
{
    PGPIO_CONTROLLER Controller = Context;
    ULONG Index;

    UNREFERENCED_PARAMETER(Device);

    for (Index = 0; Index < Controller->CommunityCount; Index++)
    {
        MmUnmapIoSpace(Controller->CommunityBase[Index], Controller->CommunityLength[Index]);
        Controller->CommunityBase[Index] = NULL;
        Controller->CommunityLength[Index] = 0;
    }
    Controller->CommunityCount = 0;

    for (Index = 0; Index < RTL_NUMBER_OF(Controller->BankState); Index++)
    {
        if (Controller->BankState[Index].Pins == NULL)
            continue;

        ExFreePoolWithTag(Controller->BankState[Index].Pins, GPIO_POOL_TAG);
        Controller->BankState[Index].Pins = NULL;
    }

    return STATUS_SUCCESS;
}

/**
 * @brief
 * Leaves a group with nothing armed and nothing latched.
 */
static
VOID
NTAPI
GpioQuietBank(
    _In_ PGPIO_CONTROLLER Controller,
    _In_ BANK_ID BankId)
{
    PULONG Enable;
    PULONG Status;

    Enable = GpioGroupRegister(Controller, BankId, GPIO_COMMUNITY_GPI_ENABLE);
    Status = GpioGroupRegister(Controller, BankId, GPIO_COMMUNITY_GPI_STATUS);
    if (Enable == NULL || Status == NULL)
        return;

    WRITE_REGISTER_ULONG(Enable, 0);
    WRITE_REGISTER_ULONG(Status, MAXULONG);
}

/**
 * @brief
 * Records the configuration of a group's claimed pads before power is lost.
 *
 * A pad the firmware has since muxed to a native function is skipped, as is an unknown group.
 */
_Use_decl_annotations_
VOID
NTAPI
GpioSaveBankHardwareContext(
    PVOID Context,
    PGPIO_SAVE_RESTORE_BANK_HARDWARE_CONTEXT_PARAMETERS SaveParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    BANK_ID BankId = SaveParameters->BankId;
    PGPIO_BANK_STATE Bank;
    PGPIO_PIN_STATE Pin;
    PIN_NUMBER PinNumber;
    PULONG Enable;
    ULONG Config0;
    PULONG Pad;

    if (BankId >= Controller->BankCount)
        return;

    Bank = &Controller->BankState[BankId];

    for (PinNumber = 0; PinNumber < Controller->Banks[BankId].PinCount; PinNumber++)
    {
        Pin = &Bank->Pins[PinNumber];
        Pad = GpioPadAddress(Controller, BankId, PinNumber);
        if (!Pin->InUse || Pad == NULL)
            continue;

        Config0 = READ_REGISTER_ULONG(Pad);
        if ((Config0 & PADCFG0_PMODE_MASK) != 0)
            continue;

        Pin->SavedConfig0 = Config0;
        Pin->SavedConfig1 = READ_REGISTER_ULONG(Pad + 1);
        Pin->ContextSaved = TRUE;
    }

    /* One enable word covers the whole group */
    Enable = GpioGroupRegister(Controller, BankId, GPIO_COMMUNITY_GPI_ENABLE);
    if (Enable != NULL)
        Bank->SavedInterruptEnable = READ_REGISTER_ULONG(Enable);

    Bank->ContextSaved = TRUE;
}

/**
 * @brief
 * Puts back what the matching save recorded.
 *
 * PADCFG1 goes first so a pad never sits in GPIO mode with the wrong
 * termination, and the enable word goes last so nothing is armed early.
 */
_Use_decl_annotations_
VOID
NTAPI
GpioRestoreBankHardwareContext(
    PVOID Context,
    PGPIO_SAVE_RESTORE_BANK_HARDWARE_CONTEXT_PARAMETERS RestoreParameters)
{
    PGPIO_CONTROLLER Controller = Context;
    BANK_ID BankId = RestoreParameters->BankId;
    PGPIO_BANK_STATE Bank;
    PGPIO_PIN_STATE Pin;
    PIN_NUMBER PinNumber;
    PULONG Enable;
    PULONG Pad;

    if (BankId >= Controller->BankCount)
        return;

    Bank = &Controller->BankState[BankId];
    if (!Bank->ContextSaved)
        return;

    for (PinNumber = 0; PinNumber < Controller->Banks[BankId].PinCount; PinNumber++)
    {
        Pin = &Bank->Pins[PinNumber];
        Pad = GpioPadAddress(Controller, BankId, PinNumber);
        if (!Pin->ContextSaved || Pad == NULL)
            continue;

        WRITE_REGISTER_ULONG(Pad + 1, Pin->SavedConfig1);
        WRITE_REGISTER_ULONG(Pad, Pin->SavedConfig0);
        Pin->ContextSaved = FALSE;
    }

    Enable = GpioGroupRegister(Controller, BankId, GPIO_COMMUNITY_GPI_ENABLE);
    if (Enable != NULL)
        WRITE_REGISTER_ULONG(Enable, Bank->SavedInterruptEnable);

    Bank->ContextSaved = FALSE;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioStartController(
    PVOID Context,
    BOOLEAN RestoreContext,
    WDF_POWER_DEVICE_STATE PreviousPowerState)
{
    GPIO_SAVE_RESTORE_BANK_HARDWARE_CONTEXT_PARAMETERS Parameters;
    PGPIO_CONTROLLER Controller = Context;
    PGPIO_BANK_STATE Bank;
    BANK_ID BankId;

    UNREFERENCED_PARAMETER(PreviousPowerState);

    RtlZeroMemory(&Parameters, sizeof(Parameters));

    for (BankId = 0; BankId < Controller->BankCount; BankId++)
    {
        if (RestoreContext)
        {
            Parameters.BankId = BankId;
            GpioRestoreBankHardwareContext(Context, &Parameters);
            continue;
        }

        /* A cold start: nothing is connected, so no pad state carries over */
        GpioQuietBank(Controller, BankId);

        Bank = &Controller->BankState[BankId];
        RtlZeroMemory(Bank->Pins, Controller->Banks[BankId].PinCount * sizeof(GPIO_PIN_STATE));
        Bank->ContextSaved = FALSE;
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioStopController(
    PVOID Context,
    BOOLEAN SaveContext,
    WDF_POWER_DEVICE_STATE TargetState)
{
    GPIO_SAVE_RESTORE_BANK_HARDWARE_CONTEXT_PARAMETERS Parameters;
    PGPIO_CONTROLLER Controller = Context;
    BANK_ID BankId;

    UNREFERENCED_PARAMETER(TargetState);

    RtlZeroMemory(&Parameters, sizeof(Parameters));

    for (BankId = 0; BankId < Controller->BankCount; BankId++)
    {
        if (SaveContext)
        {
            Parameters.BankId = BankId;
            GpioSaveBankHardwareContext(Context, &Parameters);
        }
        else
        {
            /* Nothing will listen any more, so stop pads asserting the shared line */
            GpioQuietBank(Controller, BankId);
        }
    }

    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
NTAPI
GpioQueryControllerBasicInformation(
    PVOID Context,
    PCLIENT_CONTROLLER_BASIC_INFORMATION Information)
{
    PGPIO_CONTROLLER Controller = Context;

    Information->Version = GPIO_CONTROLLER_BASIC_INFORMATION_VERSION;
    Information->Size = sizeof(*Information);
    Information->TotalPins = Controller->TotalPins;
    Information->NumberOfPinsPerBank = Controller->PinsPerBank;

    /* The pads have no debounce or dual edge logic of their own */
    Information->Flags.AsULONG = 0;
    Information->Flags.MemoryMappedController = 1;
    Information->Flags.EmulateDebouncing = 1;
    Information->Flags.EmulateActiveBoth = 1;
    return STATUS_SUCCESS;
}
