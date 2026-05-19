/** @file
  Scorpi x64 NOR flash discovery for VirtNorFlashDxe.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/ScorpiHwInfoLib.h>
#include <Library/VirtNorFlashPlatformLib.h>

#define SCORPI_MAX_FLASH_DEVICES  1

STATIC VIRT_NOR_FLASH_DESCRIPTION  mNorFlashDevices[SCORPI_MAX_FLASH_DEVICES];
STATIC UINT32                      mNorFlashDeviceCount;

STATIC
EFI_STATUS
ScorpiSetPcd32 (
  IN UINTN  TokenNumber,
  IN UINTN  Value
  )
{
  RETURN_STATUS  Status;

  if (Value > MAX_UINT32) {
    return EFI_UNSUPPORTED;
  }

  Status = LibPcdSet32S (TokenNumber, (UINT32)Value);
  return RETURN_ERROR (Status) ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}

STATIC
EFI_STATUS
ScorpiSetupVariableStore (
  IN CONST VIRT_NOR_FLASH_DESCRIPTION  *FlashDevice
  )
{
  EFI_STATUS     Status;
  RETURN_STATUS  PcdStatus;
  UINTN          VariableSize;
  UINTN          FtwWorkingSize;
  UINTN          FtwSpareSize;
  UINTN          RequiredSize;
  UINTN          VariableBase;
  UINTN          FtwWorkingBase;
  UINTN          FtwSpareBase;

  VariableSize   = PcdGet32 (PcdFlashNvStorageVariableSize);
  FtwWorkingSize = PcdGet32 (PcdFlashNvStorageFtwWorkingSize);
  FtwSpareSize   = PcdGet32 (PcdFlashNvStorageFtwSpareSize);
  RequiredSize   = VariableSize + FtwWorkingSize + FtwSpareSize;

  if ((VariableSize == 0) ||
      (FtwWorkingSize == 0) ||
      (FtwSpareSize == 0) ||
      (RequiredSize > FlashDevice->Size))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid variable flash sizes var=0x%lx work=0x%lx spare=0x%lx flash=0x%lx\n",
      __func__,
      VariableSize,
      FtwWorkingSize,
      FtwSpareSize,
      FlashDevice->Size
      ));
    return EFI_INVALID_PARAMETER;
  }

  VariableBase   = FlashDevice->RegionBaseAddress;
  FtwWorkingBase = VariableBase + VariableSize;
  FtwSpareBase   = FtwWorkingBase + FtwWorkingSize;

  Status = ScorpiSetPcd32 (PcdToken (PcdFlashNvStorageVariableBase), VariableBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ScorpiSetPcd32 (PcdToken (PcdFlashNvStorageFtwWorkingBase), FtwWorkingBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = ScorpiSetPcd32 (PcdToken (PcdFlashNvStorageFtwSpareBase), FtwSpareBase);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  PcdStatus = PcdSet64S (PcdFlashNvStorageVariableBase64, VariableBase);
  if (RETURN_ERROR (PcdStatus)) {
    return EFI_DEVICE_ERROR;
  }

  PcdStatus = PcdSet64S (PcdFlashNvStorageFtwWorkingBase64, FtwWorkingBase);
  if (RETURN_ERROR (PcdStatus)) {
    return EFI_DEVICE_ERROR;
  }

  PcdStatus = PcdSet64S (PcdFlashNvStorageFtwSpareBase64, FtwSpareBase);
  if (RETURN_ERROR (PcdStatus)) {
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DEBUG_INFO, "NOR0 : Base = 0x%lx, Size = 0x%lx\n", FlashDevice->RegionBaseAddress, FlashDevice->Size));
  DEBUG ((DEBUG_INFO, "PcdFlashNvStorageVariableBase = 0x%lx\n", VariableBase));
  DEBUG ((DEBUG_INFO, "PcdFlashNvStorageVariableSize = 0x%lx\n", VariableSize));
  DEBUG ((DEBUG_INFO, "PcdFlashNvStorageFtwWorkingBase = 0x%lx\n", FtwWorkingBase));
  DEBUG ((DEBUG_INFO, "PcdFlashNvStorageFtwWorkingSize = 0x%lx\n", FtwWorkingSize));
  DEBUG ((DEBUG_INFO, "PcdFlashNvStorageFtwSpareBase = 0x%lx\n", FtwSpareBase));
  DEBUG ((DEBUG_INFO, "PcdFlashNvStorageFtwSpareSize = 0x%lx\n", FtwSpareSize));
  return EFI_SUCCESS;
}

EFI_STATUS
VirtNorFlashPlatformInitialization (
  VOID
  )
{
  if (mNorFlashDeviceCount == 0) {
    DEBUG ((DEBUG_ERROR, "Flash device for UEFI variable storage not found\n"));
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
VirtNorFlashPlatformGetDevices (
  OUT VIRT_NOR_FLASH_DESCRIPTION  **NorFlashDescriptions,
  OUT UINT32                      *Count
  )
{
  if (mNorFlashDeviceCount == 0) {
    return EFI_NOT_FOUND;
  }

  *NorFlashDescriptions = mNorFlashDevices;
  *Count                = mNorFlashDeviceCount;
  return EFI_SUCCESS;
}

RETURN_STATUS
EFIAPI
ScorpiVirtNorFlashPlatformLibConstructor (
  VOID
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;
  CONST SCORPI_X64_HWINFO_FLASH  *Flash;
  SCORPI_HWINFO                  HwInfo;
  RETURN_STATUS                  ReturnStatus;
  EFI_STATUS                     Status;

  if ((mNorFlashDeviceCount != 0) || PcdGetBool (PcdEmuVariableNvModeEnable)) {
    return EFI_SUCCESS;
  }

  ReturnStatus = ScorpiHwInfoRead (&HwInfo);
  if (RETURN_ERROR (ReturnStatus)) {
    DEBUG ((DEBUG_INFO, "%a: ScorpiHwInfoRead: %r\n", __func__, ReturnStatus));
    return EFI_SUCCESS;
  }

  Entry = ScorpiHwInfoFind (&HwInfo, SCORPI_X64_ENTRY_FLASH, NULL);
  if (Entry == NULL) {
    RETURN_STATUS  PcdStatus;

    DEBUG ((DEBUG_INFO, "%a: no flash entry, using emulated variables\n", __func__));
    PcdStatus = PcdSetBoolS (PcdEmuVariableNvModeEnable, TRUE);
    ASSERT_RETURN_ERROR (PcdStatus);
    ScorpiHwInfoRelease (&HwInfo);
    return EFI_SUCCESS;
  }

  Flash = (CONST SCORPI_X64_HWINFO_FLASH *)Entry;
  if ((Flash->Base > MAX_UINTN) ||
      (Flash->Size > MAX_UINTN) ||
      (Flash->BlockSize == 0) ||
      ((Flash->BlockSize & (Flash->BlockSize - 1)) != 0) ||
      ((Flash->Base & (Flash->BlockSize - 1)) != 0) ||
      ((Flash->Size & (Flash->BlockSize - 1)) != 0))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: invalid flash entry base=0x%Lx size=0x%Lx block=0x%x\n",
      __func__,
      Flash->Base,
      Flash->Size,
      Flash->BlockSize
      ));
    ScorpiHwInfoRelease (&HwInfo);
    return EFI_INVALID_PARAMETER;
  }

  mNorFlashDevices[0].DeviceBaseAddress = (UINTN)Flash->Base;
  mNorFlashDevices[0].RegionBaseAddress = (UINTN)Flash->Base;
  mNorFlashDevices[0].Size              = (UINTN)Flash->Size;
  mNorFlashDevices[0].BlockSize         = Flash->BlockSize;

  Status = ScorpiSetupVariableStore (&mNorFlashDevices[0]);
  if (!EFI_ERROR (Status)) {
    mNorFlashDeviceCount = 1;
  }

  ScorpiHwInfoRelease (&HwInfo);
  return Status;
}
