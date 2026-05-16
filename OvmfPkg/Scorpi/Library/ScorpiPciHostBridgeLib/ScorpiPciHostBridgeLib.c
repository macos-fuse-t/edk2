/** @file
  Scorpi X64 PCI host bridge library.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciHostBridgeLib.h>
#include <Library/PciHostBridgeUtilityLib.h>
#include <Protocol/PciHostBridgeResourceAllocation.h>

STATIC PCI_ROOT_BRIDGE_APERTURE  mNoAperture = { MAX_UINT64, 0, 0 };

STATIC
VOID
ScorpiSetAperture (
  OUT PCI_ROOT_BRIDGE_APERTURE  *Aperture,
  IN  UINT64                    Base,
  IN  UINT64                    Size
  )
{
  if (Size == 0) {
    *Aperture = mNoAperture;
    return;
  }

  Aperture->Base        = Base;
  Aperture->Limit       = Base + Size - 1;
  Aperture->Translation = 0;
}

/**
  Return all root bridge instances in an array.

  @param Count  Return the count of root bridge instances.

  @return All root bridge instances in an array.
          The array should be passed into PciHostBridgeFreeRootBridges()
          when it is not used.
**/
PCI_ROOT_BRIDGE *
EFIAPI
PciHostBridgeGetRootBridges (
  UINTN  *Count
  )
{
  EFI_STATUS                Status;
  UINT64                    AllocationAttributes;
  PCI_ROOT_BRIDGE           *Bridge;
  PCI_ROOT_BRIDGE_APERTURE  Io;
  PCI_ROOT_BRIDGE_APERTURE  Mem;
  PCI_ROOT_BRIDGE_APERTURE  MemAbove4G;

  *Count = 0;

  ScorpiSetAperture (&Io, PcdGet64 (PcdPciIoBase), PcdGet64 (PcdPciIoSize));
  ScorpiSetAperture (&Mem, PcdGet64 (PcdPciMmio32Base), PcdGet64 (PcdPciMmio32Size));
  ScorpiSetAperture (
    &MemAbove4G,
    PcdGet64 (PcdPciMmio64Base),
    PcdGet64 (PcdPciMmio64Size)
    );

  AllocationAttributes = EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
  if (PcdGet64 (PcdPciMmio64Size) != 0) {
    AllocationAttributes |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
  }

  Bridge = AllocatePool (sizeof (*Bridge));
  if (Bridge == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: %r\n", __func__, EFI_OUT_OF_RESOURCES));
    return NULL;
  }

  Status = PciHostBridgeUtilityInitRootBridge (
             0,
             0,
             AllocationAttributes,
             FALSE,
             FALSE,
             0,
             PCI_MAX_BUS,
             &Io,
             &Mem,
             &MemAbove4G,
             &mNoAperture,
             &mNoAperture,
             Bridge
             );
  if (EFI_ERROR (Status)) {
    FreePool (Bridge);
    return NULL;
  }

  *Count = 1;
  return Bridge;
}

/**
  Free the root bridge instances array returned from
  PciHostBridgeGetRootBridges().

  @param Bridges  The root bridge instances array.
  @param Count    The count of the array.
**/
VOID
EFIAPI
PciHostBridgeFreeRootBridges (
  PCI_ROOT_BRIDGE  *Bridges,
  UINTN            Count
  )
{
  if ((Bridges == NULL) && (Count == 0)) {
    return;
  }

  ASSERT ((Bridges != NULL) && (Count == 1));

  PciHostBridgeUtilityUninitRootBridge (Bridges);
  FreePool (Bridges);
}

/**
  Inform the platform that a resource conflict happened.

  @param HostBridgeHandle  Handle of the Host Bridge.
  @param Configuration     Pointer to PCI I/O and PCI memory resource
                           descriptors.
**/
VOID
EFIAPI
PciHostBridgeResourceConflict (
  EFI_HANDLE  HostBridgeHandle,
  VOID        *Configuration
  )
{
  PciHostBridgeUtilityResourceConflict (Configuration);
}
