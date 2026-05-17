/** @file
  Scorpi reset system library.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <PiDxe.h>
#include <Uefi/UefiSpec.h>

#include <IndustryStandard/ScorpiX64Platform.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeLib.h>

STATIC UINT64  mResetBase = SCORPI_X64_RESET_BASE;

STATIC
VOID
EFIAPI
ScorpiResetAddressChangeEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EfiConvertPointer (0, (VOID **)&mResetBase);
}

STATIC
EFI_STATUS
ScorpiResetMarkRuntime (
  VOID
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  Descriptor;

  Status = gDS->GetMemorySpaceDescriptor (mResetBase, &Descriptor);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: GetMemorySpaceDescriptor: %r\n", __func__, Status));
    return Status;
  }

  Status = gDS->SetMemorySpaceAttributes (
                  mResetBase,
                  SCORPI_X64_RESET_SIZE,
                  Descriptor.Attributes | EFI_MEMORY_RUNTIME
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: SetMemorySpaceAttributes: %r\n", __func__, Status));
  }

  return Status;
}

EFI_STATUS
EFIAPI
ScorpiResetSystemLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   Event;

  ScorpiResetMarkRuntime ();

  Status = gBS->CreateEvent (
                  EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE,
                  TPL_NOTIFY,
                  ScorpiResetAddressChangeEvent,
                  NULL,
                  &Event
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: CreateEvent: %r\n", __func__, Status));
  }

  return EFI_SUCCESS;
}

STATIC
VOID
ScorpiResetWrite (
  IN UINT32  Offset,
  IN UINT32  Value
  )
{
  MmioWrite32 ((UINTN)(mResetBase + Offset), Value);
  CpuDeadLoop ();
}

VOID
EFIAPI
ResetCold (
  VOID
  )
{
  ScorpiResetWrite (SCORPI_X64_RESET_OFFSET, SCORPI_X64_RESET_VALUE);
}

VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  ScorpiResetWrite (SCORPI_X64_RESET_OFFSET, SCORPI_X64_RESET_VALUE);
}

VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  ScorpiResetWrite (SCORPI_X64_SHUTDOWN_OFFSET, SCORPI_X64_SHUTDOWN_VALUE);
}

VOID
EFIAPI
ResetPlatformSpecific (
  IN UINTN  DataSize,
  IN VOID   *ResetData
  )
{
  ResetCold ();
}

VOID
EFIAPI
ResetSystem (
  IN EFI_RESET_TYPE  ResetType,
  IN EFI_STATUS      ResetStatus,
  IN UINTN           DataSize,
  IN VOID            *ResetData OPTIONAL
  )
{
  switch (ResetType) {
    case EfiResetShutdown:
      ResetShutdown ();
      break;
    case EfiResetWarm:
    case EfiResetCold:
    case EfiResetPlatformSpecific:
    default:
      ResetCold ();
      break;
  }
}
