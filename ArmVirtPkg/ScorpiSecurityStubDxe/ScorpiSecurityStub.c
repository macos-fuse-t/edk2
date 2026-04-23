/** @file
  Scorpi-specific security architectural protocol producer.

  This variant keeps the measured-boot callbacks, but it is intentionally
  fail-open so firmware image loading is never blocked by Secure Boot policy
  or TPM measurement failures.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Protocol/Security.h>
#include <Protocol/Security2.h>
#include <Library/DebugLib.h>
#include <Library/SecurityManagementLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

EFI_HANDLE  mSecurityArchProtocolHandle = NULL;

STATIC
EFI_STATUS
EFIAPI
ScorpiSecurityAuthenticateState (
  IN CONST EFI_SECURITY_ARCH_PROTOCOL  *This,
  IN UINT32                            AuthenticationStatus,
  IN CONST EFI_DEVICE_PATH_PROTOCOL    *File
  )
{
  EFI_STATUS  Status;

  Status = ExecuteSecurity2Handlers (
             EFI_AUTH_OPERATION_AUTHENTICATION_STATE,
             AuthenticationStatus,
             File,
             NULL,
             0,
             FALSE
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: ignoring Security2 authentication-state failure: %r\n", __func__, Status));
  }

  Status = ExecuteSecurityHandlers (AuthenticationStatus, File);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: ignoring Security authentication-state failure: %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
ScorpiSecurity2Authenticate (
  IN CONST EFI_SECURITY2_ARCH_PROTOCOL  *This,
  IN CONST EFI_DEVICE_PATH_PROTOCOL     *File  OPTIONAL,
  IN VOID                               *FileBuffer,
  IN UINTN                              FileSize,
  IN BOOLEAN                            BootPolicy
  )
{
  EFI_STATUS  Status;

  Status = ExecuteSecurity2Handlers (
             EFI_AUTH_OPERATION_VERIFY_IMAGE |
             EFI_AUTH_OPERATION_DEFER_IMAGE_LOAD |
             EFI_AUTH_OPERATION_MEASURE_IMAGE |
             EFI_AUTH_OPERATION_CONNECT_POLICY,
             0,
             File,
             FileBuffer,
             FileSize,
             BootPolicy
             );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: ignoring Security2 image failure: %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}

STATIC EFI_SECURITY_ARCH_PROTOCOL  mScorpiSecurityStub = {
  ScorpiSecurityAuthenticateState
};

STATIC EFI_SECURITY2_ARCH_PROTOCOL  mScorpiSecurity2Stub = {
  ScorpiSecurity2Authenticate
};

EFI_STATUS
EFIAPI
ScorpiSecurityStubInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;

  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gEfiSecurity2ArchProtocolGuid);
  ASSERT_PROTOCOL_ALREADY_INSTALLED (NULL, &gEfiSecurityArchProtocolGuid);

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &mSecurityArchProtocolHandle,
                  &gEfiSecurity2ArchProtocolGuid,
                  &mScorpiSecurity2Stub,
                  &gEfiSecurityArchProtocolGuid,
                  &mScorpiSecurityStub,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);
  return EFI_SUCCESS;
}
