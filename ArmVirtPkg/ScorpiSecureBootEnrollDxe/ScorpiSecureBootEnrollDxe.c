/** @file
  Enroll Scorpi default Secure Boot keys on a blank variable store.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/ImageAuthentication.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/SecureBootVariableProvisionLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC CONST UINT8  mSha256OfEmptyFile[] = {
  0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8,
  0x99, 0x6f, 0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c,
  0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55
};

STATIC EFI_GUID  mScorpiDbxOwnerGuid = {
  0x4f9a451f, 0x0d89, 0x47e1, { 0x9c, 0x67, 0x13, 0x7e, 0x43, 0x86, 0x58, 0x4a }
};

STATIC EFI_GUID  mShimSbatVendorGuid = {
  0x605dab50, 0xe046, 0x4300, { 0xab, 0xb6, 0x3d, 0xd8, 0x10, 0xdd, 0x8b, 0x23 }
};

STATIC CONST CHAR16  mSbatLevelName[]   = L"SbatLevel";
STATIC CONST CHAR16  mSbatLevelRtName[] = L"SbatLevelRT";

STATIC
EFI_STATUS
CreatePlaceholderDbxSigList (
  OUT EFI_SIGNATURE_LIST  **SigList,
  OUT UINTN               *TotalSize
  )
{
  EFI_SIGNATURE_DATA  *SigData;
  UINTN               SigSize;

  if ((SigList == NULL) || (TotalSize == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *SigList = NULL;
  SigSize    = OFFSET_OF (EFI_SIGNATURE_DATA, SignatureData) + sizeof (mSha256OfEmptyFile);
  *TotalSize = sizeof (EFI_SIGNATURE_LIST) + SigSize;
  *SigList   = AllocateZeroPool (*TotalSize);
  if (*SigList == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  (*SigList)->SignatureType       = gEfiCertSha256Guid;
  (*SigList)->SignatureListSize   = (UINT32)(*TotalSize);
  (*SigList)->SignatureHeaderSize = 0;
  (*SigList)->SignatureSize       = (UINT32)SigSize;

  SigData = (EFI_SIGNATURE_DATA *)(*SigList + 1);
  CopyGuid (&SigData->SignatureOwner, &mScorpiDbxOwnerGuid);
  CopyMem (SigData->SignatureData, mSha256OfEmptyFile, sizeof (mSha256OfEmptyFile));

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
InitializeDefaultDbxVariable (
  VOID
  )
{
  EFI_SIGNATURE_LIST  *SigList;
  EFI_STATUS          Status;
  UINTN               TotalSize;

  Status = CreatePlaceholderDbxSigList (&SigList, &TotalSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = gRT->SetVariable (
                  EFI_DBX_DEFAULT_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_RUNTIME_ACCESS | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  TotalSize,
                  SigList
                  );
  DEBUG ((DEBUG_INFO, "%a: dbxDefault seed status: %r\n", __func__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot seed dbxDefault: %r\n", __func__, Status));
  }

  FreePool (SigList);
  return Status;
}

STATIC
EFI_STATUS
EnrollPlaceholderDbx (
  VOID
  )
{
  EFI_SIGNATURE_LIST  *SigList;
  EFI_STATUS          Status;
  UINTN               TotalSize;

  Status = CreatePlaceholderDbxSigList (&SigList, &TotalSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EnrollFromInput (
             EFI_IMAGE_SECURITY_DATABASE1,
             &gEfiImageSecurityDatabaseGuid,
             TotalSize,
             SigList
             );
  DEBUG ((DEBUG_INFO, "%a: placeholder dbx enroll status: %r\n", __func__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll placeholder dbx: %r\n", __func__, Status));
  }

  FreePool (SigList);
  return Status;
}

STATIC
EFI_STATUS
EnsureDbxPresent (
  VOID
  )
{
  VOID        *Data;
  UINTN       DataSize;
  EFI_STATUS  Status;

  Data     = NULL;
  DataSize = 0;
  Status   = GetVariable2 (
               EFI_IMAGE_SECURITY_DATABASE1,
               &gEfiImageSecurityDatabaseGuid,
               &Data,
               &DataSize
               );
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: dbx already present (%u bytes)\n", __func__, (UINT32)DataSize));
    FreePool (Data);
    return EFI_SUCCESS;
  }

  if (Status != EFI_NOT_FOUND) {
    return Status;
  }

  Status = EnrollPlaceholderDbx ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Data     = NULL;
  DataSize = 0;
  Status   = GetVariable2 (
               EFI_IMAGE_SECURITY_DATABASE1,
               &gEfiImageSecurityDatabaseGuid,
               &Data,
               &DataSize
               );
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: dbx present after repair (%u bytes)\n", __func__, (UINT32)DataSize));
    FreePool (Data);
  } else {
    DEBUG ((DEBUG_ERROR, "%a: dbx still absent after repair: %r\n", __func__, Status));
  }

  return Status;
}

STATIC
EFI_STATUS
EnsureSbatLevelRtPresent (
  VOID
  )
{
  VOID        *RuntimeData;
  VOID        *SbatData;
  UINTN       RuntimeDataSize;
  UINTN       SbatDataSize;
  EFI_STATUS  Status;

  RuntimeData     = NULL;
  RuntimeDataSize = 0;
  Status          = GetVariable2 (
                      mSbatLevelRtName,
                      &mShimSbatVendorGuid,
                      &RuntimeData,
                      &RuntimeDataSize
                      );
  if (!EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: SbatLevelRT already present (%u bytes)\n", __func__, (UINT32)RuntimeDataSize));
    FreePool (RuntimeData);
    return EFI_SUCCESS;
  }

  if (Status != EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: cannot read SbatLevelRT: %r\n", __func__, Status));
    return Status;
  }

  SbatData     = NULL;
  SbatDataSize = 0;
  Status       = GetVariable2 (
                   mSbatLevelName,
                   &mShimSbatVendorGuid,
                   &SbatData,
                   &SbatDataSize
                   );
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: SbatLevel not present, skipping SbatLevelRT seed\n", __func__));
    return EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot read SbatLevel: %r\n", __func__, Status));
    return Status;
  }

  Status = gRT->SetVariable (
                  (CHAR16 *)mSbatLevelRtName,
                  &mShimSbatVendorGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  SbatDataSize,
                  SbatData
                  );
  DEBUG ((DEBUG_INFO, "%a: SbatLevelRT seed status: %r\n", __func__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot seed SbatLevelRT: %r\n", __func__, Status));
  }

  FreePool (SbatData);
  return Status;
}

STATIC
EFI_STATUS
EFIAPI
InitializeDefaultVariables (
  VOID
  )
{
  EFI_STATUS  Status;

  Status = SecureBootInitPKDefault ();
  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED)) {
    return Status;
  }

  Status = SecureBootInitKEKDefault ();
  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED)) {
    return Status;
  }

  Status = SecureBootInitDbDefault ();
  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED)) {
    return Status;
  }

  Status = SecureBootInitDbxDefault ();
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_INFO, "%a: dbxDefault missing, using placeholder\n", __func__));
    Status = InitializeDefaultDbxVariable ();
  }

  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED)) {
    return Status;
  }

  Status = SecureBootInitDbtDefault ();
  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED) && (Status != EFI_NOT_FOUND)) {
    return Status;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
ScorpiSecureBootEnrollEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT8       SetupMode;

  Status = EnsureSbatLevelRtPresent ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: SbatLevelRT repair failed: %r\n", __func__, Status));
    return Status;
  }

  if (IsSecureBootEnabled ()) {
    DEBUG ((DEBUG_INFO, "%a: secure boot already enabled, skipping enrollment\n", __func__));
    return EFI_SUCCESS;
  }

  Status = GetSetupMode (&SetupMode);
  if (EFI_ERROR (Status) || (SetupMode == 0)) {
    DEBUG ((DEBUG_INFO, "%a: setup mode status=%r mode=%u\n", __func__, Status, SetupMode));
    return Status;
  }

  Status = InitializeDefaultVariables ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: default variable init failed: %r\n", __func__, Status));
    return Status;
  }

  Status = SetSecureBootMode (CUSTOM_SECURE_BOOT_MODE);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enter custom mode: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollDbFromDefault ();
  DEBUG ((DEBUG_INFO, "%a: db enrollment status: %r\n", __func__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll db: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollDbxFromDefault ();
  DEBUG ((DEBUG_INFO, "%a: dbx enrollment-from-default status: %r\n", __func__, Status));
  if (Status == EFI_NOT_FOUND) {
    Status = EFI_SUCCESS;
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll dbx: %r\n", __func__, Status));
    return Status;
  }

  Status = EnsureDbxPresent ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: dbx is still missing: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollDbtFromDefault ();
  DEBUG ((DEBUG_INFO, "%a: dbt enrollment status: %r\n", __func__, Status));
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll dbt: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollKEKFromDefault ();
  DEBUG ((DEBUG_INFO, "%a: KEK enrollment status: %r\n", __func__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll KEK: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollPKFromDefault ();
  DEBUG ((DEBUG_INFO, "%a: PK enrollment status: %r\n", __func__, Status));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll PK: %r\n", __func__, Status));
    return Status;
  }

  Status = SetSecureBootMode (STANDARD_SECURE_BOOT_MODE);
  DEBUG ((DEBUG_INFO, "%a: Secure Boot enrollment: %r\n", __func__, Status));
  return Status;
}
