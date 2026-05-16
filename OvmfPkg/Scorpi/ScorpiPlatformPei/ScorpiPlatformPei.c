/** @file
  Scorpi X64 platform PEI driver.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiPei.h>
#include <Uefi/UefiMultiPhase.h>

#include <Guid/MemoryTypeInformation.h>
#include <IndustryStandard/ScorpiX64HwInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/PlatformInitLib.h>
#include <Library/ResourcePublicationLib.h>
#include <Library/ScorpiHwInfoLib.h>
#include <Ppi/MasterBootMode.h>

#define SCORPI_PCI_IO_BASE    0xC000
#define SCORPI_PCI_IO_SIZE    0x4000
#define SCORPI_MIN_PHYS_BITS  36

#define MEMORY_TYPE_INFO_DEFAULT(Type) \
  { Type, FixedPcdGet32 (PcdMemoryType ## Type) }

STATIC EFI_MEMORY_TYPE_INFORMATION  mMemoryTypeInformation[] = {
  MEMORY_TYPE_INFO_DEFAULT (EfiACPIMemoryNVS),
  MEMORY_TYPE_INFO_DEFAULT (EfiACPIReclaimMemory),
  MEMORY_TYPE_INFO_DEFAULT (EfiReservedMemoryType),
  MEMORY_TYPE_INFO_DEFAULT (EfiRuntimeServicesCode),
  MEMORY_TYPE_INFO_DEFAULT (EfiRuntimeServicesData),
  { EfiMaxMemoryType, 0 }
};

STATIC EFI_PEI_PPI_DESCRIPTOR  mPpiBootMode[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
    &gEfiPeiMasterBootModePpiGuid,
    NULL
  }
};

STATIC
EFI_HOB_PLATFORM_INFO *
ScorpiBuildPlatformInfoHob (
  VOID
  )
{
  EFI_HOB_PLATFORM_INFO  PlatformInfoHob;
  EFI_HOB_GUID_TYPE      *GuidHob;

  ZeroMem (&PlatformInfoHob, sizeof (PlatformInfoHob));
  BuildGuidDataHob (
    &gUefiOvmfPkgPlatformInfoGuid,
    &PlatformInfoHob,
    sizeof (PlatformInfoHob)
    );

  GuidHob = GetFirstGuidHob (&gUefiOvmfPkgPlatformInfoGuid);
  return (EFI_HOB_PLATFORM_INFO *)GET_GUID_HOB_DATA (GuidHob);
}

STATIC
UINT64
ScorpiRangeEnd (
  IN UINT64  Base,
  IN UINT64  Size
  )
{
  if (Size > MAX_UINT64 - Base) {
    return MAX_UINT64;
  }

  return Base + Size;
}

STATIC
UINT8
ScorpiPhysBits (
  IN UINT64  Top
  )
{
  INTN  HighBit;

  if (Top <= 1) {
    return SCORPI_MIN_PHYS_BITS;
  }

  HighBit = HighBitSet64 (Top - 1);
  if (HighBit + 1 < SCORPI_MIN_PHYS_BITS) {
    return SCORPI_MIN_PHYS_BITS;
  }

  return (UINT8)(HighBit + 1);
}

STATIC
UINT32
ScorpiPeiMemoryCap (
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  UINT64  MemoryCap;

  MemoryCap = SIZE_64MB +
              (UINT64)PlatformInfoHob->PcdCpuMaxLogicalProcessorNumber *
              PcdGet32 (PcdCpuApStackSize);
  if (MemoryCap > SIZE_256MB) {
    MemoryCap = SIZE_256MB;
  }

  return (UINT32)MemoryCap;
}

STATIC
VOID
ScorpiPublishPeiMemory (
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  EFI_STATUS            Status;
  EFI_PHYSICAL_ADDRESS  MemoryBase;
  UINT64                MemorySize;
  UINT32                PeiMemoryCap;

  MemoryBase = PcdGet32 (PcdOvmfDxeMemFvBase) +
               PcdGet32 (PcdOvmfDxeMemFvSize);
  ASSERT (PlatformInfoHob->LowMemory > MemoryBase);

  MemorySize   = PlatformInfoHob->LowMemory - MemoryBase;
  PeiMemoryCap = ScorpiPeiMemoryCap (PlatformInfoHob);
  if (MemorySize > PeiMemoryCap) {
    MemoryBase = PlatformInfoHob->LowMemory - PeiMemoryCap;
    MemorySize = PeiMemoryCap;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: PeiMemoryBase=0x%Lx PeiMemorySize=0x%Lx\n",
    __func__,
    MemoryBase,
    MemorySize
    ));

  Status = PublishSystemMemory (MemoryBase, MemorySize);
  ASSERT_EFI_ERROR (Status);
}

STATIC
VOID
ScorpiPublishRange (
  IN CONST SCORPI_X64_HWINFO_RANGE  *Range
  )
{
  if (Range->Size == 0) {
    return;
  }

  switch (Range->Entry.Type) {
    case SCORPI_X64_ENTRY_RAM_RANGE:
      if (Range->RangeType == SCORPI_X64_RANGE_USABLE) {
        PlatformAddMemoryBaseSizeHob (Range->Base, Range->Size);
      }

      break;
    case SCORPI_X64_ENTRY_RESERVED_RANGE:
      PlatformAddReservedMemoryBaseSizeHob (Range->Base, Range->Size, FALSE);
      break;
    default:
      break;
  }
}

STATIC
VOID
ScorpiInitializeMemory (
  IN CONST SCORPI_HWINFO    *HwInfo,
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  UINT64                         FirstNonAddress;
  UINT64                         LowMemory;

  FirstNonAddress = 0;
  LowMemory       = 0;
  Entry           = NULL;

  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_RAM_RANGE, Entry)) != NULL) {
    CONST SCORPI_X64_HWINFO_RANGE  *Range;
    UINT64                         End;

    Range = (CONST SCORPI_X64_HWINFO_RANGE *)Entry;
    End   = ScorpiRangeEnd (Range->Base, Range->Size);

    if ((Range->RangeType == SCORPI_X64_RANGE_USABLE) &&
        (Range->Base < BASE_4GB) &&
        (End > LowMemory))
    {
      LowMemory = MIN (End, BASE_4GB);
    }

    if (End > FirstNonAddress) {
      FirstNonAddress = End;
    }

    ScorpiPublishRange (Range);
  }

  Entry = NULL;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_RESERVED_RANGE, Entry)) != NULL) {
    ScorpiPublishRange ((CONST SCORPI_X64_HWINFO_RANGE *)Entry);
  }

  ASSERT (LowMemory <= MAX_UINT32);
  PlatformInfoHob->LowMemory           = (UINT32)LowMemory;
  PlatformInfoHob->FirstNonAddress     = FirstNonAddress;
  PlatformInfoHob->PhysMemAddressWidth = ScorpiPhysBits (FirstNonAddress);
}

STATIC
VOID
ScorpiInitializeCpus (
  IN CONST SCORPI_HWINFO    *HwInfo,
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  UINT32                         CpuCount;
  RETURN_STATUS                  Status;

  CpuCount = 0;
  Entry    = NULL;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_CPU, Entry)) != NULL) {
    CpuCount++;
  }

  if (CpuCount == 0) {
    CpuCount = 1;
  }

  PlatformInfoHob->PcdCpuBootLogicalProcessorNumber = CpuCount;
  PlatformInfoHob->PcdCpuMaxLogicalProcessorNumber  = CpuCount;

  Status = PcdSet32S (PcdCpuBootLogicalProcessorNumber, CpuCount);
  ASSERT_RETURN_ERROR (Status);
  Status = PcdSet32S (PcdCpuMaxLogicalProcessorNumber, CpuCount);
  ASSERT_RETURN_ERROR (Status);
}

STATIC
VOID
ScorpiInitializeApic (
  IN CONST SCORPI_HWINFO  *HwInfo
  )
{
  CONST SCORPI_X64_HWINFO_APIC   *Apic;
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;

  Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_APIC, NULL);
  if (Entry == NULL) {
    return;
  }

  Apic = (CONST SCORPI_X64_HWINFO_APIC *)Entry;
  ASSERT (Apic->LocalApicBase == PcdGet32 (PcdCpuLocalApicBaseAddress));
  PlatformAddIoMemoryBaseSizeHob (Apic->IoApicBase, SIZE_4KB);
  PlatformAddIoMemoryBaseSizeHob (Apic->LocalApicBase, SIZE_4KB);
}

STATIC
VOID
ScorpiInitializePci (
  IN CONST SCORPI_HWINFO    *HwInfo,
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  RETURN_STATUS                  Status;

  Status = PcdSet16S (PcdOvmfHostBridgePciDevId, 0);
  ASSERT_RETURN_ERROR (Status);

  Status = PcdSet64S (PcdPciIoBase, SCORPI_PCI_IO_BASE);
  ASSERT_RETURN_ERROR (Status);
  Status = PcdSet64S (PcdPciIoSize, SCORPI_PCI_IO_SIZE);
  ASSERT_RETURN_ERROR (Status);

  Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_PCIE_ECAM, NULL);
  if (Entry != NULL) {
    CONST SCORPI_X64_HWINFO_PCIE_ECAM  *Ecam;

    Ecam = (CONST SCORPI_X64_HWINFO_PCIE_ECAM *)Entry;
    ASSERT (Ecam->Base == PcdGet64 (PcdPciExpressBaseAddress));
    PlatformAddReservedMemoryBaseSizeHob (Ecam->Base, Ecam->Size, FALSE);
    BuildMemoryAllocationHob (Ecam->Base, Ecam->Size, EfiReservedMemoryType);
  }

  Entry = NULL;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_PCI_WINDOW, Entry)) != NULL) {
    CONST SCORPI_X64_HWINFO_PCI_WINDOW  *Window;

    Window = (CONST SCORPI_X64_HWINFO_PCI_WINDOW *)Entry;
    PlatformAddIoMemoryBaseSizeHob (Window->CpuBase, Window->Size);

    if (Window->WindowType == SCORPI_X64_PCI_WINDOW_MMIO32) {
      ASSERT (Window->CpuBase <= MAX_UINT32);
      ASSERT (Window->Size <= MAX_UINT32);
      PlatformInfoHob->PcdPciMmio32Base = (UINT32)Window->CpuBase;
      PlatformInfoHob->PcdPciMmio32Size = (UINT32)Window->Size;
      Status = PcdSet64S (PcdPciMmio32Base, Window->CpuBase);
      ASSERT_RETURN_ERROR (Status);
      Status = PcdSet64S (PcdPciMmio32Size, Window->Size);
      ASSERT_RETURN_ERROR (Status);
    } else if (Window->WindowType == SCORPI_X64_PCI_WINDOW_MMIO64) {
      PlatformInfoHob->PcdPciMmio64Base = Window->CpuBase;
      PlatformInfoHob->PcdPciMmio64Size = Window->Size;
      Status = PcdSet64S (PcdPciMmio64Base, Window->CpuBase);
      ASSERT_RETURN_ERROR (Status);
      Status = PcdSet64S (PcdPciMmio64Size, Window->Size);
      ASSERT_RETURN_ERROR (Status);
    }
  }
}

STATIC
VOID
ScorpiInitializeReset (
  IN CONST SCORPI_HWINFO  *HwInfo
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  CONST SCORPI_X64_HWINFO_RESET  *Reset;

  Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_RESET, NULL);
  if (Entry == NULL) {
    return;
  }

  Reset = (CONST SCORPI_X64_HWINFO_RESET *)Entry;
  PlatformAddIoMemoryBaseSizeHob (Reset->Base, Reset->Size);
}

STATIC
VOID
ScorpiPeiFvInitialization (
  VOID
  )
{
  BuildMemoryAllocationHob (
    PcdGet32 (PcdOvmfPeiMemFvBase),
    PcdGet32 (PcdOvmfPeiMemFvSize),
    EfiBootServicesData
    );

  BuildFvHob (PcdGet32 (PcdOvmfDxeMemFvBase), PcdGet32 (PcdOvmfDxeMemFvSize));
  BuildMemoryAllocationHob (
    PcdGet32 (PcdOvmfDxeMemFvBase),
    PcdGet32 (PcdOvmfDxeMemFvSize),
    EfiBootServicesData
    );

  PeiServicesInstallFvInfoPpi (
    NULL,
    (VOID *)(UINTN)PcdGet32 (PcdOvmfDxeMemFvBase),
    PcdGet32 (PcdOvmfDxeMemFvSize),
    NULL,
    NULL
    );
}

STATIC
VOID
ScorpiMemTypeInfoInitialization (
  VOID
  )
{
  BuildGuidDataHob (
    &gEfiMemoryTypeInformationGuid,
    mMemoryTypeInformation,
    sizeof (mMemoryTypeInformation)
    );
}

STATIC
VOID
ScorpiReserveEmuVariableNvStore (
  VOID
  )
{
  EFI_PHYSICAL_ADDRESS  VariableStore;
  RETURN_STATUS         PcdStatus;

  VariableStore = (EFI_PHYSICAL_ADDRESS)(UINTN)PlatformReserveEmuVariableNvStore ();
  PcdStatus     = PcdSet64S (PcdEmuVariableNvStoreReserved, VariableStore);

  if (FeaturePcdGet (PcdSecureBootSupported)) {
    PlatformInitEmuVariableNvStore ((VOID *)(UINTN)VariableStore);
  }

  ASSERT_RETURN_ERROR (PcdStatus);
}

EFI_STATUS
EFIAPI
ScorpiPlatformPeiEntry (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_HOB_PLATFORM_INFO  *PlatformInfoHob;
  SCORPI_HWINFO          HwInfo;
  RETURN_STATUS          ReturnStatus;
  EFI_STATUS             Status;

  DEBUG ((DEBUG_INFO, "Scorpi X64 Platform PEIM Loaded\n"));

  PlatformInfoHob                         = ScorpiBuildPlatformInfoHob ();
  PlatformInfoHob->BootMode              = BOOT_WITH_FULL_CONFIGURATION;
  PlatformInfoHob->S3Supported           = FALSE;
  PlatformInfoHob->SmmSmramRequire       = FALSE;
  PlatformInfoHob->PcdSetNxForStack      = TRUE;
  PlatformInfoHob->DefaultMaxCpuNumber   = PcdGet32 (PcdCpuMaxLogicalProcessorNumber);
  PlatformInfoHob->PcdPciIoBase          = SCORPI_PCI_IO_BASE;
  PlatformInfoHob->PcdPciIoSize          = SCORPI_PCI_IO_SIZE;
  PlatformInfoHob->PcdPciMmio64Size      = PcdGet64 (PcdPciMmio64Size);
  PlatformInfoHob->QemuFwCfgChecked      = TRUE;
  PlatformInfoHob->QemuFwCfgSupported    = TRUE;
  PlatformInfoHob->QemuFwCfgDmaSupported = FALSE;

  ReturnStatus = ScorpiHwInfoRead (&HwInfo);
  if (RETURN_ERROR (ReturnStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: ScorpiHwInfoRead: %r\n", __func__, ReturnStatus));
    ASSERT_RETURN_ERROR (ReturnStatus);
    CpuDeadLoop ();
  }

  Status = PeiServicesSetBootMode (PlatformInfoHob->BootMode);
  ASSERT_EFI_ERROR (Status);
  Status = PeiServicesInstallPpi (mPpiBootMode);
  ASSERT_EFI_ERROR (Status);

  ScorpiInitializeMemory (&HwInfo, PlatformInfoHob);
  ScorpiInitializeCpus (&HwInfo, PlatformInfoHob);
  ScorpiInitializeApic (&HwInfo);
  BuildCpuHob (PlatformInfoHob->PhysMemAddressWidth, 16);

  ScorpiPublishPeiMemory (PlatformInfoHob);
  ScorpiInitializePci (&HwInfo, PlatformInfoHob);
  ScorpiInitializeReset (&HwInfo);
  ScorpiPeiFvInitialization ();
  ScorpiMemTypeInfoInitialization ();

  ReturnStatus = PcdSetBoolS (PcdSetNxForStack, PlatformInfoHob->PcdSetNxForStack);
  ASSERT_RETURN_ERROR (ReturnStatus);

  ScorpiReserveEmuVariableNvStore ();
  ScorpiHwInfoRelease (&HwInfo);

  return EFI_SUCCESS;
}
