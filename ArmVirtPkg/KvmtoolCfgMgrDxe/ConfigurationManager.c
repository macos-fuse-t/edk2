/** @file
  Configuration Manager Dxe

  Copyright (c) 2021 - 2022, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - Cm or CM   - Configuration Manager
    - Obj or OBJ - Object
**/

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/IoRemappingTable.h>
#include <IndustryStandard/MemoryMappedConfigurationSpaceAccessTable.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>
#include <IndustryStandard/Tpm2Acpi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DynamicPlatRepoLib.h>
#include <Library/HobLib.h>
#include <Library/HwInfoParserLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TableHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/AcpiTable.h>
#include <Protocol/ConfigurationManagerProtocol.h>

#include "ConfigurationManager.h"

//
// The platform configuration repository information.
//
STATIC
EDKII_PLATFORM_REPOSITORY_INFO  mKvmtoolPlatRepositoryInfo = {
  //
  // Configuration Manager information
  //
  { CONFIGURATION_MANAGER_REVISION, CFG_MGR_OEM_ID },
  //
  // ACPI Table List
  //
  {
    //
    // FADT Table
    //
    {
      EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdFadt),
      NULL,
      FALSE,
    },
    //
    // GTDT Table
    //
    {
      EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_6_3_GENERIC_TIMER_DESCRIPTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdGtdt),
      NULL,
      FALSE
    },
    //
    // MADT Table
    //
    {
      EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_6_3_MULTIPLE_APIC_DESCRIPTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMadt),
      NULL,
      FALSE
    },
    //
    // SPCR Table
    //
    {
      EFI_ACPI_6_3_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE,
      EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr),
      NULL,
      TRUE,
    },
    //
    // DSDT Table
    //
    {
      EFI_ACPI_6_3_DIFFERENTIATED_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
      0, // Unused
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt),
      (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code,
      FALSE
    },
    //
    // SSDT Cpu Hierarchy Table
    //
    {
      EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
      0, // Unused
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtCpuTopology),
      NULL,
      FALSE
    },
    //
    // DBG2 Table
    //
    {
      EFI_ACPI_6_3_DEBUG_PORT_2_TABLE_SIGNATURE,
      EFI_ACPI_DBG2_DEBUG_DEVICE_INFORMATION_STRUCT_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDbg2),
      NULL,
      TRUE
    },
    //
    // PCI MCFG Table
    //
    {
      EFI_ACPI_6_3_PCI_EXPRESS_MEMORY_MAPPED_CONFIGURATION_SPACE_BASE_ADDRESS_DESCRIPTION_TABLE_SIGNATURE,
      EFI_ACPI_MEMORY_MAPPED_CONFIGURATION_SPACE_ACCESS_TABLE_REVISION,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg),
      NULL,
      FALSE
    },
    //
    // SSDT table describing the PCI root complex
    //
    {
      EFI_ACPI_6_3_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE,
      0, // Unused
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtPciExpress),
      NULL,
      FALSE
    },
    //
    // IORT Table
    //
    {
      EFI_ACPI_6_3_IO_REMAPPING_TABLE_SIGNATURE,
      EFI_ACPI_IO_REMAPPING_TABLE_REVISION_00,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdIort),
      NULL,
      TRUE
    },
    //
    // TPM2 Table
    //
    {
      EFI_ACPI_6_4_TRUSTED_COMPUTING_PLATFORM_2_TABLE_SIGNATURE,
      EFI_TPM2_ACPI_TABLE_REVISION_4,
      CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdTpm2),
      NULL,
      TRUE
    },
  },

  //
  // Filtered ACPI Table List
  //
  { { 0 } },

  //
  // Power management profile information
  //
  { EFI_ACPI_6_3_PM_PROFILE_ENTERPRISE_SERVER },    // PowerManagement Profile

  //
  // ITS group node
  //
  {
    //
    // Reference token for this Iort node
    //
    REFERENCE_TOKEN (ItsGroupInfo),
    //
    // The number of ITS identifiers in the ITS node.
    //
    1,
    //
    // Reference token for the ITS identifier array
    //
    REFERENCE_TOKEN (ItsIdentifierArray)
  },

  //
  // ITS identifier array
  //
  {
    { 0 },                            // The ITS Identifier
  },

  //
  // Root Complex node info
  //
  {
    //
    // Reference token for this Iort node
    //
    REFERENCE_TOKEN (RootComplexInfo),
    //
    // Number of ID mappings
    //
    1,
    //
    // Reference token for the ID mapping array
    //
    REFERENCE_TOKEN (DeviceIdMapping[0]),
    //
    // Memory access properties : Cache coherent attributes
    //
    EFI_ACPI_IORT_MEM_ACCESS_PROP_CCA,
    //
    // Memory access properties : Allocation hints
    //
    0,
    //
    // Memory access properties : Memory access flags
    //
    0,
    //
    // ATS attributes
    //
    EFI_ACPI_IORT_ROOT_COMPLEX_ATS_UNSUPPORTED,
    //
    // PCI segment number
    //
    0,
    ///
    /// Memory address size limit
    ///
    MEMORY_ADDRESS_SIZE_LIMIT
  },

  //
  // Array of Device ID mappings
  //
  {
    //
    // Device ID mapping for Root complex node
    // RootComplex -> ITS Group
    //
    {
      //
      // Input base
      //
      0x0,
      //
      // Number of input IDs
      //
      0x0000FFFF,
      //
      // Output Base
      //
      0x0,
      //
      // Output reference
      //
      REFERENCE_TOKEN (ItsGroupInfo),
      //
      // Flags
      //
      0
    },
  },
};

#define DSDT_TPM_BASE_PLACEHOLDER  0x54504D30
#define DSDT_TPM_SIZE_PLACEHOLDER  0x54504D31

STATIC
EFI_STATUS
PatchDsdtSta (
  IN OUT EFI_ACPI_DESCRIPTION_HEADER  *Dsdt
  )
{
  CONST UINT8  StaPattern[] = { 0x08, 'T', 'S', 'T', 'A', 0x11 };
  UINT8        *Data;
  UINT32       Index;
  UINT32       Offset;
  UINT32       PatchCount;

  Data       = (UINT8 *)Dsdt;
  PatchCount = 0;

  for (Index = 0; Index + sizeof (StaPattern) <= Dsdt->Length; Index++) {
    if (CompareMem (&Data[Index], StaPattern, sizeof (StaPattern)) == 0) {
      for (Offset = sizeof (StaPattern); (Offset < 16) && (Index + Offset < Dsdt->Length); Offset++) {
        if (Data[Index + Offset] == 0) {
          Data[Index + Offset] = 0x0F;
          PatchCount++;
          break;
        }
      }
    }
  }

  if (PatchCount != 1) {
    DEBUG ((DEBUG_ERROR, "%a: found %u TPM _STA patch sites\n", __func__, PatchCount));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PatchDsdtUint32 (
  IN OUT EFI_ACPI_DESCRIPTION_HEADER  *Dsdt,
  IN     UINT32                       OldValue,
  IN     UINT32                       NewValue
  )
{
  UINT8   OldBytes[sizeof (UINT32)];
  UINT8   *Data;
  UINT32  Index;
  UINT32  PatchCount;

  OldBytes[0] = (UINT8)OldValue;
  OldBytes[1] = (UINT8)(OldValue >> 8);
  OldBytes[2] = (UINT8)(OldValue >> 16);
  OldBytes[3] = (UINT8)(OldValue >> 24);

  Data       = (UINT8 *)Dsdt;
  PatchCount = 0;

  for (Index = 0; Index + sizeof (OldBytes) <= Dsdt->Length; Index++) {
    if (CompareMem (&Data[Index], OldBytes, sizeof (OldBytes)) == 0) {
      Data[Index]     = (UINT8)NewValue;
      Data[Index + 1] = (UINT8)(NewValue >> 8);
      Data[Index + 2] = (UINT8)(NewValue >> 16);
      Data[Index + 3] = (UINT8)(NewValue >> 24);
      PatchCount++;
    }
  }

  if (PatchCount != 1) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: found %u patch sites for 0x%08x\n",
      __func__,
      PatchCount,
      OldValue
      ));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

STATIC
VOID
UpdateAcpiChecksum (
  IN OUT EFI_ACPI_DESCRIPTION_HEADER  *Table
  )
{
  UINT8   *Data;
  UINT8   Sum;
  UINT32  Index;

  Data            = (UINT8 *)Table;
  Sum             = 0;
  Table->Checksum = 0;

  for (Index = 0; Index < Table->Length; Index++) {
    Sum = (UINT8)(Sum + Data[Index]);
  }

  Table->Checksum = (UINT8)(0 - Sum);
}

/**
  A helper function for returning the Configuration Manager Objects.

  @param [in]       CmObjectId     The Configuration Manager Object ID.
  @param [in]       Object         Pointer to the Object(s).
  @param [in]       ObjectSize     Total size of the Object(s).
  @param [in]       ObjectCount    Number of Objects.
  @param [in, out]  CmObjectDesc   Pointer to the Configuration Manager Object
                                   descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
**/
STATIC
EFI_STATUS
EFIAPI
HandleCmObject (
  IN  CONST CM_OBJECT_ID                CmObjectId,
  IN        VOID                        *Object,
  IN  CONST UINTN                       ObjectSize,
  IN  CONST UINTN                       ObjectCount,
  IN  OUT   CM_OBJ_DESCRIPTOR   *CONST  CmObjectDesc
  )
{
  CmObjectDesc->ObjectId = CmObjectId;
  CmObjectDesc->Size     = ObjectSize;
  CmObjectDesc->Data     = Object;
  CmObjectDesc->Count    = ObjectCount;
  DEBUG ((
    DEBUG_INFO,
    "INFO: CmObjectId = " FMT_CM_OBJECT_ID ", "
                                           "Ptr = 0x%p, Size = %lu, Count = %lu\n",
    CmObjectId,
    CmObjectDesc->Data,
    CmObjectDesc->Size,
    CmObjectDesc->Count
    ));
  return EFI_SUCCESS;
}

/**
  A helper function for returning the Configuration Manager Objects that
  match the token.

  @param [in]  This               Pointer to the Configuration Manager Protocol.
  @param [in]  CmObjectId         The Configuration Manager Object ID.
  @param [in]  Object             Pointer to the Object(s).
  @param [in]  ObjectSize         Total size of the Object(s).
  @param [in]  ObjectCount        Number of Objects.
  @param [in]  Token              A token identifying the object.
  @param [in]  HandlerProc        A handler function to search the object
                                  referenced by the token.
  @param [in, out]  CmObjectDesc  Pointer to the Configuration Manager Object
                                  descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
STATIC
EFI_STATUS
EFIAPI
HandleCmObjectRefByToken (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN        VOID                                          *Object,
  IN  CONST UINTN                                         ObjectSize,
  IN  CONST UINTN                                         ObjectCount,
  IN  CONST CM_OBJECT_TOKEN                               Token,
  IN  CONST CM_OBJECT_HANDLER_PROC                        HandlerProc,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObjectDesc
  )
{
  EFI_STATUS  Status;

  CmObjectDesc->ObjectId = CmObjectId;
  if (Token == CM_NULL_TOKEN) {
    CmObjectDesc->Size  = ObjectSize;
    CmObjectDesc->Data  = Object;
    CmObjectDesc->Count = ObjectCount;
    Status              = EFI_SUCCESS;
  } else {
    Status = HandlerProc (This, CmObjectId, Token, CmObjectDesc);
  }

  DEBUG ((
    DEBUG_INFO,
    "INFO: Token = 0x%p, CmObjectId = " FMT_CM_OBJECT_ID ", "
                                                         "Ptr = 0x%p, Size = %lu, Count = %lu\n",
    (VOID *)Token,
    CmObjectId,
    CmObjectDesc->Data,
    CmObjectDesc->Size,
    CmObjectDesc->Count
    ));
  return Status;
}

/**
  Return an ITS identifier array.

  @param [in]  This        Pointer to the Configuration Manager Protocol.
  @param [in]  CmObjectId  The Configuration Manager Object ID.
  @param [in]  Token       A token for identifying the object
  @param [out] CmObject    Pointer to the Configuration Manager Object
                           descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetItsIdentifierArray (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token,
  OUT       CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  PlatformRepo = This->PlatRepoInfo;

  if (Token != (CM_OBJECT_TOKEN)&PlatformRepo->ItsIdentifierArray) {
    return EFI_NOT_FOUND;
  }

  CmObject->ObjectId = CmObjectId;
  CmObject->Size     = sizeof (PlatformRepo->ItsIdentifierArray);
  CmObject->Data     = (VOID *)&PlatformRepo->ItsIdentifierArray;
  CmObject->Count    = ARRAY_SIZE (PlatformRepo->ItsIdentifierArray);
  return EFI_SUCCESS;
}

/**
  Return a device Id mapping array.

  @param [in]  This        Pointer to the Configuration Manager Protocol.
  @param [in]  CmObjectId  The Configuration Manager Object ID.
  @param [in]  Token       A token for identifying the object
  @param [out] CmObject    Pointer to the Configuration Manager Object
                           descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetDeviceIdMappingArray (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token,
  OUT       CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  PlatformRepo = This->PlatRepoInfo;

  if (Token != (CM_OBJECT_TOKEN)&PlatformRepo->DeviceIdMapping[0]) {
    return EFI_NOT_FOUND;
  }

  CmObject->ObjectId = CmObjectId;
  CmObject->Size     = sizeof (CM_ARM_ID_MAPPING);
  CmObject->Data     = (VOID *)Token;
  CmObject->Count    = 1;
  return EFI_SUCCESS;
}

/**
  Function pointer called by the parser to add information.

  Callback function that the parser can use to add new
  CmObj. This function must copy the CmObj data and not rely on
  the parser preserving the CmObj memory.
  This function is responsible of the Token allocation.

  @param  [in]  ParserHandle  A handle to the parser instance.
  @param  [in]  Context       A pointer to the caller's context provided in
                              HwInfoParserInit ().
  @param  [in]  CmObjDesc     CM_OBJ_DESCRIPTOR containing the CmObj(s) to add.
  @param  [out] Token         If provided and success, contain the token
                              generated for the CmObj.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
**/
STATIC
EFI_STATUS
EFIAPI
HwInfoAdd (
  IN        HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        VOID                   *Context,
  IN  CONST CM_OBJ_DESCRIPTOR      *CmObjDesc,
  OUT       CM_OBJECT_TOKEN        *Token OPTIONAL
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;

  if ((ParserHandle == NULL)  ||
      (Context == NULL)       ||
      (CmObjDesc == NULL))
  {
    ASSERT (ParserHandle != NULL);
    ASSERT (Context != NULL);
    ASSERT (CmObjDesc != NULL);
    return EFI_INVALID_PARAMETER;
  }

  PlatformRepo = (EDKII_PLATFORM_REPOSITORY_INFO *)Context;

  DEBUG_CODE_BEGIN ();
  //
  // Print the received objects.
  //
  ParseCmObjDesc (CmObjDesc);
  DEBUG_CODE_END ();

  Status = DynPlatRepoAddObject (
             PlatformRepo->DynamicPlatformRepo,
             CmObjDesc,
             Token
             );
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}

/**
  Cleanup the platform configuration repository.

  @param [in]  This        Pointer to the Configuration Manager Protocol.

  @retval EFI_SUCCESS             Success
  @retval EFI_INVALID_PARAMETER   A parameter is invalid.
**/
STATIC
EFI_STATUS
EFIAPI
CleanupPlatformRepository (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;

  if (This == NULL) {
    ASSERT (This != NULL);
    return EFI_INVALID_PARAMETER;
  }

  PlatformRepo = This->PlatRepoInfo;

  if (PlatformRepo->DsdtTable != NULL) {
    FreePool (PlatformRepo->DsdtTable);
    PlatformRepo->DsdtTable = NULL;
  }

  //
  // Shutdown the dynamic repo and free all objects.
  //
  Status = DynamicPlatRepoShutdown (PlatformRepo->DynamicPlatformRepo);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  //
  // Shutdown parser.
  //
  Status = HwInfoParserShutdown (PlatformRepo->FdtParserHandle);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}

/**
  Build a DSDT describing the TPM2 ACPI device.

  @param [in] PlatformRepo  Pointer to the platform repository.

  @retval EFI_SUCCESS           Success, including when no TPM is present.
  @retval EFI_OUT_OF_RESOURCES  An allocation has failed.
**/
STATIC
EFI_STATUS
EFIAPI
BuildDsdtTable (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo
  )
{
  EFI_STATUS                   Status;
  EFI_ACPI_DESCRIPTION_HEADER  *Dsdt;
  EFI_ACPI_DESCRIPTION_HEADER  *DsdtTemplate;
  UINT64                       TpmBase;
  UINT64                       TpmSize;

  if ((PlatformRepo == NULL) || !FeaturePcdGet (PcdTpm2SupportEnabled)) {
    return EFI_SUCCESS;
  }

  TpmBase = PcdGet64 (PcdTpmBaseAddress);
  TpmSize = PcdGet64 (PcdTpmMmioSize);
  if ((TpmBase == 0) || (TpmSize == 0)) {
    return EFI_SUCCESS;
  }

  DsdtTemplate = (EFI_ACPI_DESCRIPTION_HEADER *)dsdt_aml_code;
  Dsdt         = AllocateCopyPool (DsdtTemplate->Length, DsdtTemplate);
  if (Dsdt == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PatchDsdtSta (Dsdt);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto Exit;
  }

  Status = PatchDsdtUint32 (Dsdt, DSDT_TPM_BASE_PLACEHOLDER, (UINT32)TpmBase);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto Exit;
  }

  Status = PatchDsdtUint32 (Dsdt, DSDT_TPM_SIZE_PLACEHOLDER, (UINT32)TpmSize);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto Exit;
  }

  UpdateAcpiChecksum (Dsdt);
  PlatformRepo->DsdtTable = Dsdt;
  Dsdt                    = NULL;

  DEBUG ((DEBUG_INFO, "%a: TPM2 DSDT device @ 0x%lx size 0x%lx\n", __func__, TpmBase, TpmSize));

Exit:
  if (Dsdt != NULL) {
    FreePool (Dsdt);
  }

  return Status;
}

/**
  Add TPM2 interface information when TPM2 support is enabled and a TPM base
  address was discovered by platform initialization.

  @param [in] PlatformRepo  Pointer to the platform repository.

  @retval EFI_SUCCESS           Success, including when no TPM is present.
  @retval EFI_OUT_OF_RESOURCES  An allocation has failed.
**/
STATIC
EFI_STATUS
EFIAPI
AddTpm2InterfaceInfo (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo
  )
{
  CM_ARCH_COMMON_TPM2_INTERFACE_INFO  Tpm2Info;
  CM_OBJ_DESCRIPTOR                   CmObjDesc;
  UINT64                              TpmBase;
  UINT64                              TpmSize;

  if ((PlatformRepo == NULL) || !FeaturePcdGet (PcdTpm2SupportEnabled)) {
    return EFI_SUCCESS;
  }

  TpmBase = PcdGet64 (PcdTpmBaseAddress);
  TpmSize = PcdGet64 (PcdTpmMmioSize);
  if ((TpmBase == 0) || (TpmSize == 0)) {
    return EFI_SUCCESS;
  }

  ZeroMem (&Tpm2Info, sizeof (Tpm2Info));
  Tpm2Info.PlatformClass        = 0;
  Tpm2Info.AddressOfControlArea = TpmBase + 0x40;
  Tpm2Info.StartMethod          = EFI_TPM2_ACPI_TABLE_START_METHOD_COMMAND_RESPONSE_BUFFER_INTERFACE;
  Tpm2Info.StartMethodParametersSize = 0;

  CmObjDesc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjTpm2InterfaceInfo);
  CmObjDesc.Size     = sizeof (Tpm2Info);
  CmObjDesc.Data     = &Tpm2Info;
  CmObjDesc.Count    = 1;

  DEBUG ((DEBUG_INFO, "%a: TPM2 CRB @ 0x%lx\n", __func__, TpmBase));

  return DynPlatRepoAddObject (
           PlatformRepo->DynamicPlatformRepo,
           &CmObjDesc,
           NULL
           );
}

/**
  Initialize the platform configuration repository.

  @param [in]  This        Pointer to the Configuration Manager Protocol.

  @retval EFI_SUCCESS             Success
  @retval EFI_INVALID_PARAMETER   A parameter is invalid.
  @retval EFI_OUT_OF_RESOURCES    An allocation has failed.
**/
STATIC
EFI_STATUS
EFIAPI
InitializePlatformRepository (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;
  VOID                            *Hob;

  if (This == NULL) {
    ASSERT (This != NULL);
    return EFI_INVALID_PARAMETER;
  }

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    ASSERT (FALSE);
    ASSERT (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64));
    return EFI_NOT_FOUND;
  }

  PlatformRepo          = This->PlatRepoInfo;
  PlatformRepo->FdtBase = (VOID *)*(UINTN *)GET_GUID_HOB_DATA (Hob);

  //
  // Initialise the dynamic platform repository.
  //
  Status = DynamicPlatRepoInit (&PlatformRepo->DynamicPlatformRepo);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    return Status;
  }

  //
  // Initialise the FDT parser
  //
  Status = HwInfoParserInit (
             PlatformRepo->FdtBase,
             PlatformRepo,
             HwInfoAdd,
             &PlatformRepo->FdtParserHandle
             );
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto ErrorHandler;
  }

  Status = HwInfoParse (PlatformRepo->FdtParserHandle);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto ErrorHandler;
  }

  Status = AddTpm2InterfaceInfo (PlatformRepo);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto ErrorHandler;
  }

  Status = BuildDsdtTable (PlatformRepo);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto ErrorHandler;
  }

  Status = DynamicPlatRepoFinalise (PlatformRepo->DynamicPlatformRepo);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto ErrorHandler;
  }

  return EFI_SUCCESS;

ErrorHandler:
  CleanupPlatformRepository (This);
  return Status;
}

/**
  Check whether a GIC ITS node was described by the hardware information parser.

  @param [in] PlatformRepo  Pointer to the platform repository.

  @retval TRUE   A GIC ITS node is present.
  @retval FALSE  No GIC ITS node is present.
**/
STATIC
BOOLEAN
PlatformHasGicIts (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo
  )
{
  EFI_STATUS         Status;
  CM_OBJ_DESCRIPTOR  CmObjDesc;

  if (PlatformRepo == NULL) {
    return FALSE;
  }

  Status = DynamicPlatRepoGetObject (
             PlatformRepo->DynamicPlatformRepo,
             CREATE_CM_ARM_OBJECT_ID (EArmObjGicItsInfo),
             CM_NULL_TOKEN,
             &CmObjDesc
             );

  return (!EFI_ERROR (Status) && (CmObjDesc.Count != 0));
}

/**
  Check whether PCI configuration space was described by the hardware
  information parser.

  @param [in] PlatformRepo  Pointer to the platform repository.

  @retval TRUE   PCI configuration space is present.
  @retval FALSE  PCI configuration space is absent.
**/
STATIC
BOOLEAN
PlatformHasPciConfigSpace (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo
  )
{
  EFI_STATUS         Status;
  CM_OBJ_DESCRIPTOR  CmObjDesc;

  if (PlatformRepo == NULL) {
    return FALSE;
  }

  Status = DynamicPlatRepoGetObject (
             PlatformRepo->DynamicPlatformRepo,
             CREATE_CM_ARCH_COMMON_OBJECT_ID (
               EArchCommonObjPciConfigSpaceInfo
               ),
             CM_NULL_TOKEN,
             &CmObjDesc
             );

  return (!EFI_ERROR (Status) && (CmObjDesc.Count != 0));
}

/**
  Check whether TPM2 interface information is present.

  @param [in] PlatformRepo  Pointer to the platform repository.

  @retval TRUE   TPM2 interface information is present.
  @retval FALSE  TPM2 interface information is absent.
**/
STATIC
BOOLEAN
PlatformHasTpm2 (
  IN EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo
  )
{
  EFI_STATUS         Status;
  CM_OBJ_DESCRIPTOR  CmObjDesc;

  if (PlatformRepo == NULL) {
    return FALSE;
  }

  Status = DynamicPlatRepoGetObject (
             PlatformRepo->DynamicPlatformRepo,
             CREATE_CM_ARCH_COMMON_OBJECT_ID (
               EArchCommonObjTpm2InterfaceInfo
               ),
             CM_NULL_TOKEN,
             &CmObjDesc
             );

  return (!EFI_ERROR (Status) && (CmObjDesc.Count != 0));
}

/**
  Return a standard namespace object.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetStandardNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;
  UINTN                           Index;
  UINTN                           AcpiTableCount;
  CM_OBJ_DESCRIPTOR               CmObjDesc;
  BOOLEAN                         HasPciConfigSpace;
  BOOLEAN                         HasTpm2;
  BOOLEAN                         IncludeIortTable;
  BOOLEAN                         SkipTable;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  Status       = EFI_NOT_FOUND;
  PlatformRepo = This->PlatRepoInfo;

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EStdObjCfgMgrInfo:
      Status = HandleCmObject (
                 CmObjectId,
                 &PlatformRepo->CmInfo,
                 sizeof (PlatformRepo->CmInfo),
                 1,
                 CmObject
                 );
      break;

    case EStdObjAcpiTableList:
      AcpiTableCount   = 0;
      HasPciConfigSpace = PlatformHasPciConfigSpace (PlatformRepo);
      HasTpm2          = PlatformHasTpm2 (PlatformRepo) &&
                         (PlatformRepo->DsdtTable != NULL);
      IncludeIortTable = FALSE;

      if (HasPciConfigSpace) {
        //
        // IORT is only needed when describing an ITS-backed PCI MSI topology.
        // A GIC MSI frame is advertised through MADT instead.
        //
        Status = DynamicPlatRepoGetObject (
                   PlatformRepo->DynamicPlatformRepo,
                   CREATE_CM_ARM_OBJECT_ID (EArmObjGicDInfo),
                   CM_NULL_TOKEN,
                   &CmObjDesc
                   );
        if (EFI_ERROR (Status)) {
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        IncludeIortTable =
          (((CM_ARM_GICD_INFO *)CmObjDesc.Data)->GicVersion >= 3) &&
          PlatformHasGicIts (PlatformRepo);
      }

      for (Index = 0; Index < ARRAY_SIZE (PlatformRepo->CmAcpiTableList); Index++) {
        SkipTable = FALSE;

        switch (PlatformRepo->CmAcpiTableList[Index].TableGeneratorId) {
          case CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt):
            break;

          case CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdMcfg):
          case CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtPciExpress):
            SkipTable = !HasPciConfigSpace;
            break;

          case CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdIort):
            SkipTable = !IncludeIortTable;
            break;

          case CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdTpm2):
            SkipTable = !HasTpm2;
            break;

          default:
            break;
        }

        if (SkipTable) {
          continue;
        }

        PlatformRepo->CmAcpiTableListFiltered[AcpiTableCount] =
          PlatformRepo->CmAcpiTableList[Index];
        if (HasTpm2 &&
            (PlatformRepo->CmAcpiTableList[Index].TableGeneratorId ==
             CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDsdt)))
        {
          PlatformRepo->CmAcpiTableListFiltered[AcpiTableCount].AcpiTableData =
            PlatformRepo->DsdtTable;
        }

        AcpiTableCount++;
      }

      Status = HandleCmObject (
                 CmObjectId,
                 PlatformRepo->CmAcpiTableListFiltered,
                 (sizeof (PlatformRepo->CmAcpiTableListFiltered[0]) * AcpiTableCount),
                 AcpiTableCount,
                 CmObject
                 );
      break;

    default:
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: CmObjectId " FMT_CM_OBJECT_ID ". Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
  }

  return Status;
}

/**
  Return an ArchCommon namespace object.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetArchCommonNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  Status       = EFI_NOT_FOUND;
  PlatformRepo = This->PlatRepoInfo;

  //
  // First check among the static objects.
  //
  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EArchCommonObjPowerManagementProfileInfo:
      Status = HandleCmObject (
                 CmObjectId,
                 &PlatformRepo->PmProfileInfo,
                 sizeof (PlatformRepo->PmProfileInfo),
                 1,
                 CmObject
                 );
      break;

    default:
      //
      // No match found among the static objects.
      // Check the dynamic objects.
      //
      Status = DynamicPlatRepoGetObject (
                 PlatformRepo->DynamicPlatformRepo,
                 CmObjectId,
                 Token,
                 CmObject
                 );
      break;
  } // switch

  if (Status == EFI_NOT_FOUND) {
    DEBUG ((
      DEBUG_INFO,
      "INFO: CmObjectId " FMT_CM_OBJECT_ID ". Status = %r\n",
      CmObjectId,
      Status
      ));
  } else {
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}

/**
  Return an ARM namespace object.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetArmNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS                      Status;
  EDKII_PLATFORM_REPOSITORY_INFO  *PlatformRepo;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  Status       = EFI_NOT_FOUND;
  PlatformRepo = This->PlatRepoInfo;

  //
  // First check among the static objects.
  //
  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    case EArmObjItsGroup:
      if (!PlatformHasGicIts (PlatformRepo)) {
        break;
      }

      Status = HandleCmObject (
                 CmObjectId,
                 &PlatformRepo->ItsGroupInfo,
                 sizeof (PlatformRepo->ItsGroupInfo),
                 1,
                 CmObject
                 );
      break;

    case EArmObjGicItsIdentifierArray:
      if (!PlatformHasGicIts (PlatformRepo)) {
        break;
      }

      Status = HandleCmObjectRefByToken (
                 This,
                 CmObjectId,
                 PlatformRepo->ItsIdentifierArray,
                 sizeof (PlatformRepo->ItsIdentifierArray),
                 ARRAY_SIZE (PlatformRepo->ItsIdentifierArray),
                 Token,
                 GetItsIdentifierArray,
                 CmObject
                 );
      break;

    case EArmObjRootComplex:
      if (!PlatformHasGicIts (PlatformRepo)) {
        break;
      }

      Status = HandleCmObject (
                 CmObjectId,
                 &PlatformRepo->RootComplexInfo,
                 sizeof (PlatformRepo->RootComplexInfo),
                 1,
                 CmObject
                 );
      break;

    case EArmObjIdMappingArray:
      if (!PlatformHasGicIts (PlatformRepo)) {
        break;
      }

      Status = HandleCmObjectRefByToken (
                 This,
                 CmObjectId,
                 PlatformRepo->DeviceIdMapping,
                 sizeof (PlatformRepo->DeviceIdMapping),
                 ARRAY_SIZE (PlatformRepo->DeviceIdMapping),
                 Token,
                 GetDeviceIdMappingArray,
                 CmObject
                 );
      break;

    default:
      //
      // No match found among the static objects.
      // Check the dynamic objects.
      //
      Status = DynamicPlatRepoGetObject (
                 PlatformRepo->DynamicPlatformRepo,
                 CmObjectId,
                 Token,
                 CmObject
                 );
      break;
  } // switch

  if (Status == EFI_NOT_FOUND) {
    DEBUG ((
      DEBUG_INFO,
      "INFO: CmObjectId " FMT_CM_OBJECT_ID ". Status = %r\n",
      CmObjectId,
      Status
      ));
  } else {
    ASSERT_EFI_ERROR (Status);
  }

  return Status;
}

/**
  Return an OEM namespace object.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
GetOemNameSpaceObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS  Status;

  Status = EFI_SUCCESS;
  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_OBJECT_ID (CmObjectId)) {
    default:
      Status = EFI_NOT_FOUND;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: CmObjectId " FMT_CM_OBJECT_ID ". Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
  }

  return Status;
}

/**
  The GetObject function defines the interface implemented by the
  Configuration Manager Protocol for returning the Configuration
  Manager Objects.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in, out] CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the requested Object.

  @retval EFI_SUCCESS           Success.
  @retval EFI_INVALID_PARAMETER A parameter is invalid.
  @retval EFI_NOT_FOUND         The required object information is not found.
**/
EFI_STATUS
EFIAPI
ArmKvmtoolPlatformGetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN  OUT   CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  EFI_STATUS  Status;

  if ((This == NULL) || (CmObject == NULL)) {
    ASSERT (This != NULL);
    ASSERT (CmObject != NULL);
    return EFI_INVALID_PARAMETER;
  }

  switch (GET_CM_NAMESPACE_ID (CmObjectId)) {
    case EObjNameSpaceStandard:
      Status = GetStandardNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceArchCommon:
      Status = GetArchCommonNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceArm:
      Status = GetArmNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    case EObjNameSpaceOem:
      Status = GetOemNameSpaceObject (This, CmObjectId, Token, CmObject);
      break;
    default:
      Status = EFI_INVALID_PARAMETER;
      DEBUG ((
        DEBUG_ERROR,
        "ERROR: Unknown Namespace CmObjectId " FMT_CM_OBJECT_ID ". "
                                                                "Status = %r\n",
        CmObjectId,
        Status
        ));
      break;
  }

  return Status;
}

/**
  The SetObject function defines the interface implemented by the
  Configuration Manager Protocol for updating the Configuration
  Manager Objects.

  @param [in]      This        Pointer to the Configuration Manager Protocol.
  @param [in]      CmObjectId  The Configuration Manager Object ID.
  @param [in]      Token       An optional token identifying the object. If
                               unused this must be CM_NULL_TOKEN.
  @param [in]      CmObject    Pointer to the Configuration Manager Object
                               descriptor describing the Object.

  @retval EFI_UNSUPPORTED  This operation is not supported.
**/
EFI_STATUS
EFIAPI
ArmKvmtoolPlatformSetObject (
  IN  CONST EDKII_CONFIGURATION_MANAGER_PROTOCOL  *CONST  This,
  IN  CONST CM_OBJECT_ID                                  CmObjectId,
  IN  CONST CM_OBJECT_TOKEN                               Token OPTIONAL,
  IN        CM_OBJ_DESCRIPTOR                     *CONST  CmObject
  )
{
  return EFI_UNSUPPORTED;
}

//
// A structure describing the configuration manager protocol interface.
//
STATIC
CONST
EDKII_CONFIGURATION_MANAGER_PROTOCOL  mKvmtoolPlatformConfigManagerProtocol = {
  CREATE_REVISION (1,          0),
  ArmKvmtoolPlatformGetObject,
  ArmKvmtoolPlatformSetObject,
  &mKvmtoolPlatRepositoryInfo
};

/**
  Entrypoint of Configuration Manager Dxe.

  @param  ImageHandle
  @param  SystemTable

  @retval EFI_SUCCESS
  @retval EFI_LOAD_ERROR
  @retval EFI_OUT_OF_RESOURCES
**/
EFI_STATUS
EFIAPI
ConfigurationManagerDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallProtocolInterface (
                  &ImageHandle,
                  &gEdkiiConfigurationManagerProtocolGuid,
                  EFI_NATIVE_INTERFACE,
                  (VOID *)&mKvmtoolPlatformConfigManagerProtocol
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to get Install Configuration Manager Protocol." \
      " Status = %r\n",
      Status
      ));
    return Status;
  }

  Status = InitializePlatformRepository (
             &mKvmtoolPlatformConfigManagerProtocol
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: Failed to initialize the Platform Configuration Repository." \
      " Status = %r\n",
      Status
      ));
    goto ErrorHandler;
  }

  return Status;

ErrorHandler:
  gBS->UninstallProtocolInterface (
         &ImageHandle,
         &gEdkiiConfigurationManagerProtocolGuid,
         (VOID *)&mKvmtoolPlatformConfigManagerProtocol
         );
  return Status;
}

/**
  Unload function for this image.

  @param ImageHandle   Handle for the image of this driver.

  @retval EFI_SUCCESS  Driver unloaded successfully.
  @retval other        Driver can not unloaded.
**/
EFI_STATUS
EFIAPI
ConfigurationManagerDxeUnloadImage (
  IN EFI_HANDLE  ImageHandle
  )
{
  return CleanupPlatformRepository (&mKvmtoolPlatformConfigManagerProtocol);
}
