/** @file
  Enroll Scorpi default Secure Boot keys on a blank variable store.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/ImageAuthentication.h>
#include <Library/DebugLib.h>
#include <UefiSecureBoot.h>
#include <Library/SecureBootVariableLib.h>
#include <Library/SecureBootVariableProvisionLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

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
  if (EFI_ERROR (Status) && (Status != EFI_UNSUPPORTED) && (Status != EFI_NOT_FOUND)) {
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
  UINT8       SecureBootEnable;
  UINT8       SetupMode;

  if (IsSecureBootEnabled ()) {
    return EFI_SUCCESS;
  }

  Status = GetSetupMode (&SetupMode);
  if (EFI_ERROR (Status) || (SetupMode == 0)) {
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

  SecureBootEnable = SECURE_BOOT_ENABLE;
  Status           = gRT->SetVariable (
                            EFI_SECURE_BOOT_ENABLE_NAME,
                            &gEfiSecureBootEnableDisableGuid,
                            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                            sizeof (SecureBootEnable),
                            &SecureBootEnable
                            );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot set SecureBootEnable: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollDbFromDefault ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll db: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollDbxFromDefault ();
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll dbx: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollDbtFromDefault ();
  if (EFI_ERROR (Status) && (Status != EFI_NOT_FOUND)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll dbt: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollKEKFromDefault ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll KEK: %r\n", __func__, Status));
    return Status;
  }

  Status = EnrollPKFromDefault ();
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: cannot enroll PK: %r\n", __func__, Status));
    return Status;
  }

  Status = SetSecureBootMode (STANDARD_SECURE_BOOT_MODE);
  DEBUG ((DEBUG_INFO, "%a: Secure Boot enrollment: %r\n", __func__, Status));
  return Status;
}
