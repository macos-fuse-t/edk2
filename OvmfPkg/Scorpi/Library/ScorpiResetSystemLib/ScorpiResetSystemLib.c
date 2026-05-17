/** @file
  Scorpi reset system library.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <PiDxe.h>
#include <Uefi/UefiSpec.h>

#include <IndustryStandard/ScorpiX64HwInfo.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/IoLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/ScorpiHwInfoLib.h>
#include <Library/UefiRuntimeLib.h>

#define SCORPI_RESET_BASE            0xF0000000ULL
#define SCORPI_RESET_SIZE            0x1000
#define SCORPI_RESET_OFFSET          0
#define SCORPI_SHUTDOWN_OFFSET       4
#define SCORPI_RESET_VALUE           1
#define SCORPI_SHUTDOWN_VALUE        1

STATIC UINT64  mResetBase      = SCORPI_RESET_BASE;
STATIC UINT32  mResetSize      = SCORPI_RESET_SIZE;
STATIC UINT32  mResetOffset    = SCORPI_RESET_OFFSET;
STATIC UINT32  mShutdownOffset = SCORPI_SHUTDOWN_OFFSET;
STATIC UINT32  mResetValue     = SCORPI_RESET_VALUE;
STATIC UINT32  mShutdownValue  = SCORPI_SHUTDOWN_VALUE;

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
                  mResetSize,
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
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  CONST SCORPI_X64_HWINFO_RESET  *Reset;
  SCORPI_HWINFO                  HwInfo;
  RETURN_STATUS                  Status;

  Status = ScorpiHwInfoRead (&HwInfo);
  if (RETURN_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: ScorpiHwInfoRead: %r\n", __func__, Status));
    return EFI_SUCCESS;
  }

  Entry = ScorpiHwInfoFind (&HwInfo, SCORPI_X64_ENTRY_RESET, NULL);
  if (Entry != NULL) {
    Reset           = (CONST SCORPI_X64_HWINFO_RESET *)Entry;
    mResetBase      = Reset->Base;
    mResetSize      = Reset->Size;
    mResetOffset    = Reset->ResetOffset;
    mShutdownOffset = Reset->ShutdownOffset;
    mResetValue     = Reset->ResetValue;
    mShutdownValue  = Reset->ShutdownValue;
  }

  ScorpiHwInfoRelease (&HwInfo);
  ScorpiResetMarkRuntime ();
  return EFI_SUCCESS;
}

STATIC
VOID
ScorpiResetWrite (
  IN UINT32  Offset,
  IN UINT32  Value
  )
{
  EFI_STATUS  Status;
  VOID        *Address;

  Address = (VOID *)(UINTN)(mResetBase + Offset);
  if (EfiGoneVirtual ()) {
    Status = EfiConvertPointer (0, &Address);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: EfiConvertPointer: %r\n", __func__, Status));
      CpuDeadLoop ();
    }
  }

  MmioWrite32 ((UINTN)Address, Value);
  CpuDeadLoop ();
}

VOID
EFIAPI
ResetCold (
  VOID
  )
{
  ScorpiResetWrite (mResetOffset, mResetValue);
}

VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  ScorpiResetWrite (mResetOffset, mResetValue);
}

VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  ScorpiResetWrite (mShutdownOffset, mShutdownValue);
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
