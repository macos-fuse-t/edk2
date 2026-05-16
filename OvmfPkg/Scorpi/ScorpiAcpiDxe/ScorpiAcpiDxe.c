/** @file
  Scorpi X64 ACPI configuration manager.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <AcpiTableGenerator.h>
#include <ArchCommonNameSpaceObjects.h>
#include <ConfigurationManagerObject.h>
#include <IndustryStandard/Acpi65.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <IndustryStandard/ScorpiX64HwInfo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/ScorpiHwInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/ConfigurationManagerProtocol.h>
#include <X64NameSpaceObjects.h>

extern CHAR8  dsdt_aml_code[];

#define SCORPI_CFG_MGR_REVISION  CREATE_REVISION (1, 0)
#define SCORPI_OEM_ID            { 'S', 'C', 'O', 'R', 'P', 'I' }
#define SCORPI_OEM_TABLE_ID      SIGNATURE_64 ('S', 'C', 'O', 'R', 'P', 'I', 'X', '6')
#define SCORPI_CREATOR_ID        SIGNATURE_32 ('S', 'C', 'P', 'I')

#define SCORPI_ACPI_TABLE_COUNT  5

#define SCORPI_PCI_SPACE_M32  2
#define SCORPI_PCI_SPACE_M64  3

typedef struct PlatformRepositoryInfo {
  CM_STD_OBJ_CONFIGURATION_MANAGER_INFO           CmInfo;
  CM_STD_OBJ_ACPI_TABLE_INFO                      AcpiTables[SCORPI_ACPI_TABLE_COUNT];
  CM_ARCH_COMMON_POWER_MANAGEMENT_PROFILE_INFO    PowerProfile;
  CM_ARCH_COMMON_FIXED_FEATURE_FLAGS              FixedFeatureFlags;
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO            PciConfigSpace;
  CM_ARCH_COMMON_OBJ_REF                          *PciAddressMapRefs;
  CM_ARCH_COMMON_PCI_ADDRESS_MAP_INFO             *PciAddressMaps;
  UINT32                                          PciAddressMapCount;
  CM_X64_FADT_SCI_INTERRUPT                       FadtSciInterrupt;
  CM_X64_FADT_SCI_CMD_INFO                        FadtSciCmdInfo;
  CM_X64_FADT_PM_BLOCK_INFO                       FadtPmBlockInfo;
  CM_X64_FADT_GPE_BLOCK_INFO                      FadtGpeBlockInfo;
  CM_X64_FADT_X_PM_BLOCK_INFO                     FadtXpmBlockInfo;
  CM_X64_FADT_X_GPE_BLOCK_INFO                    FadtXgpeBlockInfo;
  CM_X64_FADT_SLEEP_BLOCK_INFO                    FadtSleepBlockInfo;
  CM_X64_FADT_RESET_BLOCK_INFO                    FadtResetBlockInfo;
  CM_X64_FADT_MISC_INFO                           FadtMiscInfo;
  CM_X64_MADT_INFO                                MadtInfo;
  CM_X64_IO_APIC_INFO                             IoApicInfo;
  CM_X64_LOCAL_APIC_X2APIC_INFO                  *LocalApicInfo;
  UINT32                                         LocalApicCount;
  EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER
                                                 *MadtTable;
} EDKII_PLATFORM_REPOSITORY_INFO;

STATIC EDKII_PLATFORM_REPOSITORY_INFO  mScorpiAcpiRepository = {
  { SCORPI_CFG_MGR_REVISION, SCORPI_OEM_ID },
  {
    {
      EFI_ACPI_6_5_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_6_5_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
      NULL,
      FALSE,
      EFI_ACPI_6_5_FIXED_ACPI_DESCRIPTION_TABLE_MINOR_REVISION
    },
    {
      EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdRaw),
      NULL,
      FALSE
    },
    {
      EFI_ACPI_6_5_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
      0,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
      (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code,
      FALSE
    },
    {
      EFI_ACPI_6_5_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
      NULL,
      FALSE
    },
    {
      EFI_ACPI_6_5_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
      0,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtPciExpress),
      NULL,
      FALSE
    }
  },
  { EFI_ACPI_6_5_PM_PROFILE_ENTERPRISE_SERVER },
  { EFI_ACPI_6_5_HW_REDUCED_ACPI | EFI_ACPI_6_5_RESET_REG_SUP },
  { 0 }
};

STATIC
VOID
ScorpiAcpiChecksum (
  IN OUT EFI_ACPI_DESCRIPTION_HEADER  *Header
  )
{
  UINT8   *Data;
  UINT8   Sum;
  UINT32  Index;

  Header->Checksum = 0;
  Data             = (UINT8 *)Header;
  Sum              = 0;

  for (Index = 0; Index < Header->Length; Index++) {
    Sum = (UINT8)(Sum + Data[Index]);
  }

  Header->Checksum = (UINT8)(0 - Sum);
}

STATIC
VOID
ScorpiAcpiHeader (
  OUT EFI_ACPI_DESCRIPTION_HEADER  *Header,
  IN  UINT32                       Signature,
  IN  UINT32                       Length,
  IN  UINT8                        Revision
  )
{
  CONST UINT8  OemId[6] = SCORPI_OEM_ID;

  ZeroMem (Header, sizeof (*Header));
  Header->Signature       = Signature;
  Header->Length          = Length;
  Header->Revision        = Revision;
  Header->OemTableId      = SCORPI_OEM_TABLE_ID;
  Header->OemRevision     = 1;
  Header->CreatorId       = SCORPI_CREATOR_ID;
  Header->CreatorRevision = SCORPI_CFG_MGR_REVISION;
  CopyMem (Header->OemId, OemId, sizeof (OemId));
}

STATIC
EFI_STATUS
ScorpiHandleObject (
  IN  CONST CM_OBJECT_ID          CmObjectId,
  IN        VOID                  *Object,
  IN  CONST UINTN                 ObjectSize,
  IN  CONST UINTN                 ObjectCount,
  OUT       CM_OBJ_DESCRIPTOR     *CmObject
  )
{
  CmObject->ObjectId = CmObjectId;
  CmObject->Size     = ObjectSize;
  CmObject->Data     = Object;
  CmObject->Count    = ObjectCount;

  return EFI_SUCCESS;
}

STATIC
VOID
ScorpiFreeRepository (
  IN OUT EDKII_PLATFORM_REPOSITORY_INFO  *Repo
  )
{
  if (Repo->LocalApicInfo != NULL) {
    FreePool (Repo->LocalApicInfo);
    Repo->LocalApicInfo  = NULL;
    Repo->LocalApicCount = 0;
  }

  if (Repo->MadtTable != NULL) {
    FreePool (Repo->MadtTable);
    Repo->MadtTable = NULL;
  }

  if (Repo->PciAddressMapRefs != NULL) {
    FreePool (Repo->PciAddressMapRefs);
    Repo->PciAddressMapRefs = NULL;
  }

  if (Repo->PciAddressMaps != NULL) {
    FreePool (Repo->PciAddressMaps);
    Repo->PciAddressMaps = NULL;
  }

  Repo->PciAddressMapCount             = 0;
  Repo->PciConfigSpace.AddressMapToken = CM_NULL_TOKEN;
}

STATIC
EFI_STATUS
ScorpiBuildLocalApicInfo (
  IN CONST SCORPI_HWINFO              *HwInfo,
  IN EDKII_PLATFORM_REPOSITORY_INFO  *Repo
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  UINT32                         Count;
  UINT32                         Index;
  BOOLEAN                        UseX2Apic;

  Count = 0;
  Entry = NULL;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_CPU, Entry)) != NULL) {
    Count++;
  }

  if (Count == 0) {
    return EFI_NOT_FOUND;
  }

  Repo->LocalApicInfo = AllocateZeroPool (sizeof (*Repo->LocalApicInfo) * Count);
  if (Repo->LocalApicInfo == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  UseX2Apic = FALSE;
  Entry     = NULL;
  Index     = 0;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_CPU, Entry)) != NULL) {
    CONST SCORPI_X64_HWINFO_CPU  *Cpu;

    Cpu = (CONST SCORPI_X64_HWINFO_CPU *)Entry;
    Repo->LocalApicInfo[Index].ApicId           = Cpu->ApicId;
    Repo->LocalApicInfo[Index].AcpiProcessorUid = Cpu->AcpiProcessorUid;
    Repo->LocalApicInfo[Index].Flags            = EFI_ACPI_6_5_LOCAL_APIC_ENABLED;

    if ((Cpu->ApicId > MAX_UINT8) || (Cpu->AcpiProcessorUid > MAX_UINT8)) {
      UseX2Apic = TRUE;
    }

    Index++;
  }

  Repo->LocalApicCount    = Count;
  Repo->MadtInfo.Flags    = 0;
  Repo->MadtInfo.ApicMode = UseX2Apic ? LocalApicModeX2Apic : LocalApicModeXApic;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ScorpiPciWindowSpaceCode (
  IN  CONST SCORPI_X64_HWINFO_PCI_WINDOW  *Window,
  OUT       UINT8                          *SpaceCode
  )
{
  if ((Window->Size == 0) ||
      (Window->CpuBase > MAX_UINT64 - Window->Size + 1) ||
      (Window->PciBase > MAX_UINT64 - Window->Size + 1) ||
      (Window->CpuBase < Window->PciBase))
  {
    return EFI_UNSUPPORTED;
  }

  switch (Window->WindowType) {
    case SCORPI_X64_PCI_WINDOW_MMIO32:
      if ((Window->PciBase > MAX_UINT32) ||
          (Window->CpuBase > MAX_UINT32) ||
          (Window->Size > MAX_UINT32) ||
          (Window->PciBase + Window->Size - 1 > MAX_UINT32) ||
          (Window->CpuBase + Window->Size - 1 > MAX_UINT32))
      {
        return EFI_UNSUPPORTED;
      }

      *SpaceCode = SCORPI_PCI_SPACE_M32;
      return EFI_SUCCESS;

    case SCORPI_X64_PCI_WINDOW_MMIO64:
      *SpaceCode = SCORPI_PCI_SPACE_M64;
      return EFI_SUCCESS;

    default:
      return EFI_UNSUPPORTED;
  }
}

STATIC
EFI_STATUS
ScorpiBuildPciAddressMaps (
  IN CONST SCORPI_HWINFO              *HwInfo,
  IN EDKII_PLATFORM_REPOSITORY_INFO  *Repo
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY       *Entry;
  CONST SCORPI_X64_HWINFO_PCI_WINDOW  *Window;
  UINT32                              Count;
  UINT32                              Index;
  UINT8                               SpaceCode;
  EFI_STATUS                          Status;

  Count = 0;
  Entry = NULL;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_PCI_WINDOW, Entry)) != NULL) {
    Count++;
  }

  if (Count == 0) {
    return EFI_NOT_FOUND;
  }

  Repo->PciAddressMaps = AllocateZeroPool (sizeof (*Repo->PciAddressMaps) * Count);
  if (Repo->PciAddressMaps == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Repo->PciAddressMapRefs = AllocateZeroPool (sizeof (*Repo->PciAddressMapRefs) * Count);
  if (Repo->PciAddressMapRefs == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Entry = NULL;
  Index = 0;
  while ((Entry = ScorpiHwInfoFind (HwInfo, SCORPI_X64_ENTRY_PCI_WINDOW, Entry)) != NULL) {
    Window = (CONST SCORPI_X64_HWINFO_PCI_WINDOW *)Entry;
    Status = ScorpiPciWindowSpaceCode (Window, &SpaceCode);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Repo->PciAddressMaps[Index].SpaceCode   = SpaceCode;
    Repo->PciAddressMaps[Index].PciAddress  = Window->PciBase;
    Repo->PciAddressMaps[Index].CpuAddress  = Window->CpuBase;
    Repo->PciAddressMaps[Index].AddressSize = Window->Size;

    Repo->PciAddressMapRefs[Index].ReferenceToken =
      (CM_OBJECT_TOKEN)&Repo->PciAddressMaps[Index];

    Index++;
  }

  Repo->PciAddressMapCount             = Count;
  Repo->PciConfigSpace.AddressMapToken = (CM_OBJECT_TOKEN)Repo->PciAddressMapRefs;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ScorpiBuildMadtTable (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *Repo
  )
{
  UINT32                                                        Index;
  UINT32                                                        LocalApicSize;
  UINT32                                                        TableSize;
  UINT8                                                         *Ptr;
  EFI_ACPI_6_5_IO_APIC_STRUCTURE                               *IoApic;
  EFI_ACPI_6_5_PROCESSOR_LOCAL_APIC_STRUCTURE                  *LocalApic;
  EFI_ACPI_6_5_PROCESSOR_LOCAL_X2APIC_STRUCTURE                *LocalX2Apic;
  EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_HEADER          *Madt;

  if (Repo->LocalApicCount == 0) {
    return EFI_NOT_FOUND;
  }

  LocalApicSize = (Repo->MadtInfo.ApicMode == LocalApicModeX2Apic) ?
                  sizeof (EFI_ACPI_6_5_PROCESSOR_LOCAL_X2APIC_STRUCTURE) :
                  sizeof (EFI_ACPI_6_5_PROCESSOR_LOCAL_APIC_STRUCTURE);
  TableSize = sizeof (*Madt) +
              sizeof (EFI_ACPI_6_5_IO_APIC_STRUCTURE) +
              (Repo->LocalApicCount * LocalApicSize);

  Madt = AllocateZeroPool (TableSize);
  if (Madt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ScorpiAcpiHeader (
    &Madt->Header,
    EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
    TableSize,
    EFI_ACPI_6_5_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION
    );
  Madt->LocalApicAddress = Repo->MadtInfo.LocalApicAddress;
  Madt->Flags            = Repo->MadtInfo.Flags;

  Ptr    = (UINT8 *)Madt + sizeof (*Madt);
  IoApic = (EFI_ACPI_6_5_IO_APIC_STRUCTURE *)Ptr;
  IoApic->Type                      = EFI_ACPI_6_5_IO_APIC;
  IoApic->Length                    = sizeof (*IoApic);
  IoApic->IoApicId                  = Repo->IoApicInfo.IoApicId;
  IoApic->IoApicAddress             = Repo->IoApicInfo.IoApicAddress;
  IoApic->GlobalSystemInterruptBase = Repo->IoApicInfo.GlobalSystemInterruptBase;

  Ptr += sizeof (*IoApic);
  if (Repo->MadtInfo.ApicMode == LocalApicModeX2Apic) {
    LocalX2Apic = (EFI_ACPI_6_5_PROCESSOR_LOCAL_X2APIC_STRUCTURE *)Ptr;
    for (Index = 0; Index < Repo->LocalApicCount; Index++) {
      LocalX2Apic[Index].Type             = EFI_ACPI_6_5_PROCESSOR_LOCAL_X2APIC;
      LocalX2Apic[Index].Length           = sizeof (LocalX2Apic[Index]);
      LocalX2Apic[Index].X2ApicId         = Repo->LocalApicInfo[Index].ApicId;
      LocalX2Apic[Index].Flags            = Repo->LocalApicInfo[Index].Flags;
      LocalX2Apic[Index].AcpiProcessorUid = Repo->LocalApicInfo[Index].AcpiProcessorUid;
    }
  } else {
    LocalApic = (EFI_ACPI_6_5_PROCESSOR_LOCAL_APIC_STRUCTURE *)Ptr;
    for (Index = 0; Index < Repo->LocalApicCount; Index++) {
      LocalApic[Index].Type             = EFI_ACPI_6_5_PROCESSOR_LOCAL_APIC;
      LocalApic[Index].Length           = sizeof (LocalApic[Index]);
      LocalApic[Index].AcpiProcessorUid = (UINT8)Repo->LocalApicInfo[Index].AcpiProcessorUid;
      LocalApic[Index].ApicId           = (UINT8)Repo->LocalApicInfo[Index].ApicId;
      LocalApic[Index].Flags            = Repo->LocalApicInfo[Index].Flags;
    }
  }

  ScorpiAcpiChecksum (&Madt->Header);
  Repo->MadtTable = Madt;
  Repo->AcpiTables[1].AcpiTableData = &Madt->Header;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ScorpiLoadHwInfo (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *Repo
  )
{
  SCORPI_HWINFO                   HwInfo;
  RETURN_STATUS                   ReturnStatus;
  EFI_STATUS                      Status;
  CONST SCORPI_X64_HWINFO_ENTRY   *Entry;
  CONST SCORPI_X64_HWINFO_APIC    *Apic;
  CONST SCORPI_X64_HWINFO_PCIE_ECAM  *Ecam;
  CONST SCORPI_X64_HWINFO_RESET   *Reset;

  ReturnStatus = ScorpiHwInfoRead (&HwInfo);
  if (RETURN_ERROR (ReturnStatus)) {
    DEBUG ((DEBUG_ERROR, "%a: ScorpiHwInfoRead: %r\n", __func__, ReturnStatus));
    return (EFI_STATUS)ReturnStatus;
  }

  Entry = ScorpiHwInfoFind (&HwInfo, SCORPI_X64_ENTRY_APIC, NULL);
  if (Entry == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Apic = (CONST SCORPI_X64_HWINFO_APIC *)Entry;
  if ((Apic->LocalApicBase > MAX_UINT32) || (Apic->IoApicBase > MAX_UINT32)) {
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

  Repo->MadtInfo.LocalApicAddress              = (UINT32)Apic->LocalApicBase;
  Repo->IoApicInfo.IoApicId                    = (UINT8)Apic->IoApicId;
  Repo->IoApicInfo.IoApicAddress               = (UINT32)Apic->IoApicBase;
  Repo->IoApicInfo.GlobalSystemInterruptBase   = Apic->GsiBase;

  Entry = ScorpiHwInfoFind (&HwInfo, SCORPI_X64_ENTRY_PCIE_ECAM, NULL);
  if (Entry == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Ecam = (CONST SCORPI_X64_HWINFO_PCIE_ECAM *)Entry;
  if (Ecam->Base != PcdGet64 (PcdPciExpressBaseAddress)) {
    Status = EFI_UNSUPPORTED;
    goto Exit;
  }

  Repo->PciConfigSpace.BaseAddress           = Ecam->Base;
  Repo->PciConfigSpace.PciSegmentGroupNumber = Ecam->Segment;
  Repo->PciConfigSpace.StartBusNumber        = Ecam->StartBus;
  Repo->PciConfigSpace.EndBusNumber          = Ecam->EndBus;
  Repo->PciConfigSpace.AddressMapToken       = CM_NULL_TOKEN;
  Repo->PciConfigSpace.InterruptMapToken     = CM_NULL_TOKEN;

  Status = ScorpiBuildPciAddressMaps (&HwInfo, Repo);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Entry = ScorpiHwInfoFind (&HwInfo, SCORPI_X64_ENTRY_RESET, NULL);
  if (Entry == NULL) {
    Status = EFI_NOT_FOUND;
    goto Exit;
  }

  Reset = (CONST SCORPI_X64_HWINFO_RESET *)Entry;
  Repo->FadtResetBlockInfo.ResetReg.AddressSpaceId    = EFI_ACPI_6_5_SYSTEM_MEMORY;
  Repo->FadtResetBlockInfo.ResetReg.RegisterBitWidth   = (Reset->Size >= sizeof (UINT32)) ? 32 : 8;
  Repo->FadtResetBlockInfo.ResetReg.RegisterBitOffset  = 0;
  Repo->FadtResetBlockInfo.ResetReg.AccessSize         = (Reset->Size >= sizeof (UINT32)) ?
                                                         EFI_ACPI_6_5_DWORD :
                                                         EFI_ACPI_6_5_BYTE;
  Repo->FadtResetBlockInfo.ResetReg.Address            = Reset->Base + Reset->ResetOffset;
  Repo->FadtResetBlockInfo.ResetValue                  = (UINT8)Reset->ResetValue;

  Status = ScorpiBuildLocalApicInfo (&HwInfo, Repo);
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  Status = ScorpiBuildMadtTable (Repo);

Exit:
  ScorpiHwInfoRelease (&HwInfo);
  return Status;
}

STATIC
EFI_STATUS
ScorpiGetStandardObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  OUT       CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;

  Repo = This->PlatRepoInfo;
  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EStdObjCfgMgrInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->CmInfo, sizeof (Repo->CmInfo), 1, CmObject);
    case EStdObjAcpiTableList:
      return ScorpiHandleObject (
               CmObjectId,
               Repo->AcpiTables,
               sizeof (Repo->AcpiTables),
               ARRAY_SIZE (Repo->AcpiTables),
               CmObject
               );
    default:
      return EFI_NOT_FOUND;
  }
}

STATIC
EFI_STATUS
ScorpiGetArchCommonObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  OUT       CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;
  UINT32                          Index;

  Repo = This->PlatRepoInfo;
  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EArchCommonObjPowerManagementProfileInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->PowerProfile, sizeof (Repo->PowerProfile), 1, CmObject);
    case EArchCommonObjFixedFeatureFlags:
      return ScorpiHandleObject (CmObjectId, &Repo->FixedFeatureFlags, sizeof (Repo->FixedFeatureFlags), 1, CmObject);
    case EArchCommonObjCmRef:
      if (Token == (CM_OBJECT_TOKEN)Repo->PciAddressMapRefs) {
        return ScorpiHandleObject (
                 CmObjectId,
                 Repo->PciAddressMapRefs,
                 sizeof (*Repo->PciAddressMapRefs) * Repo->PciAddressMapCount,
                 Repo->PciAddressMapCount,
                 CmObject
                 );
      }

      return EFI_NOT_FOUND;
    case EArchCommonObjPciConfigSpaceInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->PciConfigSpace, sizeof (Repo->PciConfigSpace), 1, CmObject);
    case EArchCommonObjPciAddressMapInfo:
      if (Token == CM_NULL_TOKEN) {
        return ScorpiHandleObject (
                 CmObjectId,
                 Repo->PciAddressMaps,
                 sizeof (*Repo->PciAddressMaps) * Repo->PciAddressMapCount,
                 Repo->PciAddressMapCount,
                 CmObject
                 );
      }

      for (Index = 0; Index < Repo->PciAddressMapCount; Index++) {
        if (Token == (CM_OBJECT_TOKEN)&Repo->PciAddressMaps[Index]) {
          return ScorpiHandleObject (
                   CmObjectId,
                   &Repo->PciAddressMaps[Index],
                   sizeof (Repo->PciAddressMaps[Index]),
                   1,
                   CmObject
                   );
        }
      }

      return EFI_NOT_FOUND;
    default:
      return EFI_NOT_FOUND;
  }
}

STATIC
EFI_STATUS
ScorpiGetX64Object (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  OUT       CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *Repo;

  Repo = This->PlatRepoInfo;
  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EX64ObjFadtSciInterrupt:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtSciInterrupt, sizeof (Repo->FadtSciInterrupt), 1, CmObject);
    case EX64ObjFadtSciCmdInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtSciCmdInfo, sizeof (Repo->FadtSciCmdInfo), 1, CmObject);
    case EX64ObjFadtPmBlockInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtPmBlockInfo, sizeof (Repo->FadtPmBlockInfo), 1, CmObject);
    case EX64ObjFadtGpeBlockInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtGpeBlockInfo, sizeof (Repo->FadtGpeBlockInfo), 1, CmObject);
    case EX64ObjFadtXpmBlockInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtXpmBlockInfo, sizeof (Repo->FadtXpmBlockInfo), 1, CmObject);
    case EX64ObjFadtXgpeBlockInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtXgpeBlockInfo, sizeof (Repo->FadtXgpeBlockInfo), 1, CmObject);
    case EX64ObjFadtSleepBlockInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtSleepBlockInfo, sizeof (Repo->FadtSleepBlockInfo), 1, CmObject);
    case EX64ObjFadtResetBlockInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtResetBlockInfo, sizeof (Repo->FadtResetBlockInfo), 1, CmObject);
    case EX64ObjFadtMiscInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->FadtMiscInfo, sizeof (Repo->FadtMiscInfo), 1, CmObject);
    case EX64ObjMadtInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->MadtInfo, sizeof (Repo->MadtInfo), 1, CmObject);
    case EX64ObjIoApicInfo:
      return ScorpiHandleObject (CmObjectId, &Repo->IoApicInfo, sizeof (Repo->IoApicInfo), 1, CmObject);
    case EX64ObjLocalApicX2ApicInfo:
      return ScorpiHandleObject (
               CmObjectId,
               Repo->LocalApicInfo,
               sizeof (*Repo->LocalApicInfo) * Repo->LocalApicCount,
               Repo->LocalApicCount,
               CmObject
               );
    default:
      return EFI_NOT_FOUND;
  }
}

STATIC
EFI_STATUS
EFIAPI
ScorpiGetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  OUT       CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  if ((This == NULL) || (CmObject == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_NAMESPACE_ID (CmObjectId)) {
    case EObjNameSpaceStandard:
      return ScorpiGetStandardObject (This, CmObjectId, Token, CmObject);
    case EObjNameSpaceArchCommon:
      return ScorpiGetArchCommonObject (This, CmObjectId, Token, CmObject);
    case EObjNameSpaceX64:
      return ScorpiGetX64Object (This, CmObjectId, Token, CmObject);
    default:
      return EFI_NOT_FOUND;
  }
}

STATIC
EFI_STATUS
EFIAPI
ScorpiSetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN        CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  return EFI_UNSUPPORTED;
}

STATIC CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  mScorpiConfigurationManager = {
  CREATE_REVISION (1, 0),
  ScorpiGetObject,
  ScorpiSetObject,
  &mScorpiAcpiRepository
};

EFI_STATUS
EFIAPI
ScorpiAcpiDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = ScorpiLoadHwInfo (&mScorpiAcpiRepository);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: ScorpiLoadHwInfo: %r\n", __func__, Status));
    ScorpiFreeRepository (&mScorpiAcpiRepository);
    return Status;
  }

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEdkiiConfigurationManagerProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID *)&mScorpiConfigurationManager
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallProtocolInterface: %r\n", __func__, Status));
    ScorpiFreeRepository (&mScorpiAcpiRepository);
  }

  return Status;
}

EFI_STATUS
EFIAPI
ScorpiAcpiDxeUnloadImage (
  IN EFI_HANDLE  ImageHandle
  )
{
  ScorpiFreeRepository (&mScorpiAcpiRepository);
  return EFI_SUCCESS;
}
