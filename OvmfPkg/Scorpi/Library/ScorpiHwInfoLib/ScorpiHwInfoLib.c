/** @file
  Scorpi hardware-info access helpers.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/ScorpiHwInfoLib.h>

STATIC
UINTN
ScorpiHwInfoEntryMinSize (
  IN UINT16  Type
  )
{
  switch (Type) {
    case SCORPI_X64_ENTRY_RAM_RANGE:
    case SCORPI_X64_ENTRY_RESERVED_RANGE:
      return sizeof (SCORPI_X64_HWINFO_RANGE);
    case SCORPI_X64_ENTRY_CPU:
      return sizeof (SCORPI_X64_HWINFO_CPU);
    case SCORPI_X64_ENTRY_APIC:
      return sizeof (SCORPI_X64_HWINFO_APIC);
    case SCORPI_X64_ENTRY_PCIE_ECAM:
      return sizeof (SCORPI_X64_HWINFO_PCIE_ECAM);
    case SCORPI_X64_ENTRY_PCI_WINDOW:
      return sizeof (SCORPI_X64_HWINFO_PCI_WINDOW);
    case SCORPI_X64_ENTRY_DEVICE:
      return sizeof (SCORPI_X64_HWINFO_DEVICE);
    case SCORPI_X64_ENTRY_FRAMEBUFFER:
      return sizeof (SCORPI_X64_HWINFO_FRAMEBUFFER);
    case SCORPI_X64_ENTRY_TPM:
      return sizeof (SCORPI_X64_HWINFO_TPM);
    case SCORPI_X64_ENTRY_RESET:
      return sizeof (SCORPI_X64_HWINFO_RESET);
    default:
      return sizeof (SCORPI_X64_HWINFO_ENTRY);
  }
}

STATIC
UINT32
ScorpiHwInfoChecksum (
  IN CONST UINT8  *Blob,
  IN UINTN        Size
  )
{
  UINTN   ChecksumOffset;
  UINT32  Checksum;
  UINTN   Index;

  ChecksumOffset = OFFSET_OF (SCORPI_X64_HWINFO_HEADER, Checksum32);
  Checksum       = 0;

  for (Index = 0; Index < Size; Index++) {
    if ((Index >= ChecksumOffset) &&
        (Index < ChecksumOffset + sizeof (UINT32)))
    {
      continue;
    }

    Checksum += Blob[Index];
  }

  return Checksum;
}

STATIC
RETURN_STATUS
ScorpiHwInfoValidateEntries (
  IN CONST SCORPI_X64_HWINFO_HEADER  *Header,
  IN CONST UINT8                     *Blob
  )
{
  UINT64                         Offset;
  UINT32                         Index;
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;

  Offset = Header->EntriesOffset;
  if ((Offset < Header->HeaderSize) || (Offset > Header->TotalSize)) {
    return RETURN_COMPROMISED_DATA;
  }

  for (Index = 0; Index < Header->EntryCount; Index++) {
    if (Offset > Header->TotalSize - sizeof (SCORPI_X64_HWINFO_ENTRY)) {
      return RETURN_COMPROMISED_DATA;
    }

    Entry = (CONST SCORPI_X64_HWINFO_ENTRY *)(Blob + Offset);
    if ((Entry->Size < ScorpiHwInfoEntryMinSize (Entry->Type)) ||
        ((Entry->Size & 7) != 0))
    {
      return RETURN_COMPROMISED_DATA;
    }

    if (Offset > Header->TotalSize - Entry->Size) {
      return RETURN_COMPROMISED_DATA;
    }

    Offset += Entry->Size;
  }

  if (Offset != Header->TotalSize) {
    return RETURN_COMPROMISED_DATA;
  }

  return RETURN_SUCCESS;
}

RETURN_STATUS
EFIAPI
ScorpiHwInfoValidate (
  IN CONST VOID  *Blob,
  IN UINTN       Size
  )
{
  CONST SCORPI_X64_HWINFO_HEADER  *Header;

  if ((Blob == NULL) || (Size == 0)) {
    return RETURN_INVALID_PARAMETER;
  }

  if ((Size < sizeof (*Header)) || (Size > MAX_UINT32)) {
    return RETURN_COMPROMISED_DATA;
  }

  Header = (CONST SCORPI_X64_HWINFO_HEADER *)Blob;
  if (CompareMem (
        Header->Magic,
        SCORPI_X64_HWINFO_MAGIC,
        sizeof (Header->Magic)
        ) != 0)
  {
    return RETURN_UNSUPPORTED;
  }

  if (Header->Major != SCORPI_X64_HWINFO_MAJOR) {
    return RETURN_UNSUPPORTED;
  }

  if ((Header->HeaderSize != sizeof (*Header)) ||
      (Header->TotalSize != Size))
  {
    return RETURN_COMPROMISED_DATA;
  }

  if (Header->Checksum32 != ScorpiHwInfoChecksum (Blob, Size)) {
    return RETURN_COMPROMISED_DATA;
  }

  return ScorpiHwInfoValidateEntries (Header, Blob);
}

RETURN_STATUS
EFIAPI
ScorpiHwInfoRead (
  OUT SCORPI_HWINFO  *HwInfo
  )
{
  FIRMWARE_CONFIG_ITEM  Item;
  UINTN                 Size;
  VOID                  *Blob;
  RETURN_STATUS         Status;

  if (HwInfo == NULL) {
    return RETURN_INVALID_PARAMETER;
  }

  ZeroMem (HwInfo, sizeof (*HwInfo));

  if (!QemuFwCfgIsAvailable ()) {
    return RETURN_UNSUPPORTED;
  }

  Status = QemuFwCfgFindFile (SCORPI_X64_HWINFO_FWCFG_FILE, &Item, &Size);
  if (RETURN_ERROR (Status)) {
    return Status;
  }

  Blob = AllocatePool (Size);
  if (Blob == NULL) {
    return RETURN_OUT_OF_RESOURCES;
  }

  QemuFwCfgSelectItem (Item);
  QemuFwCfgReadBytes (Size, Blob);

  Status = ScorpiHwInfoValidate (Blob, Size);
  if (RETURN_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: invalid hardware info: %r\n", __func__, Status));
    FreePool (Blob);
    return Status;
  }

  HwInfo->Blob = Blob;
  HwInfo->Size = Size;
  return RETURN_SUCCESS;
}

VOID
EFIAPI
ScorpiHwInfoRelease (
  IN OUT SCORPI_HWINFO  *HwInfo
  )
{
  if (HwInfo == NULL) {
    return;
  }

  if (HwInfo->Blob != NULL) {
    FreePool ((VOID *)HwInfo->Blob);
  }

  ZeroMem (HwInfo, sizeof (*HwInfo));
}

CONST SCORPI_X64_HWINFO_HEADER *
EFIAPI
ScorpiHwInfoHeader (
  IN CONST SCORPI_HWINFO  *HwInfo
  )
{
  if ((HwInfo == NULL) || (HwInfo->Blob == NULL)) {
    return NULL;
  }

  return (CONST SCORPI_X64_HWINFO_HEADER *)HwInfo->Blob;
}

CONST SCORPI_X64_HWINFO_ENTRY *
EFIAPI
ScorpiHwInfoFirst (
  IN CONST SCORPI_HWINFO  *HwInfo
  )
{
  CONST SCORPI_X64_HWINFO_HEADER  *Header;

  Header = ScorpiHwInfoHeader (HwInfo);
  if ((Header == NULL) || (Header->EntryCount == 0)) {
    return NULL;
  }

  return (CONST SCORPI_X64_HWINFO_ENTRY *)((CONST UINT8 *)HwInfo->Blob +
                                           Header->EntriesOffset);
}

CONST SCORPI_X64_HWINFO_ENTRY *
EFIAPI
ScorpiHwInfoNext (
  IN CONST SCORPI_HWINFO            *HwInfo,
  IN CONST SCORPI_X64_HWINFO_ENTRY  *Entry
  )
{
  CONST SCORPI_X64_HWINFO_HEADER  *Header;
  CONST UINT8                     *Base;
  CONST UINT8                     *Next;

  Header = ScorpiHwInfoHeader (HwInfo);
  if ((Header == NULL) || (Entry == NULL)) {
    return NULL;
  }

  Base = (CONST UINT8 *)HwInfo->Blob;
  Next = (CONST UINT8 *)Entry + Entry->Size;
  if ((Next <= (CONST UINT8 *)Entry) || (Next >= Base + Header->TotalSize)) {
    return NULL;
  }

  return (CONST SCORPI_X64_HWINFO_ENTRY *)Next;
}

CONST SCORPI_X64_HWINFO_ENTRY *
EFIAPI
ScorpiHwInfoFind (
  IN CONST SCORPI_HWINFO            *HwInfo,
  IN UINT16                         Type,
  IN CONST SCORPI_X64_HWINFO_ENTRY  *Previous  OPTIONAL
  )
{
  CONST SCORPI_X64_HWINFO_ENTRY  *Entry;

  if (Previous == NULL) {
    Entry = ScorpiHwInfoFirst (HwInfo);
  } else {
    Entry = ScorpiHwInfoNext (HwInfo, Previous);
  }

  while (Entry != NULL) {
    if (Entry->Type == Type) {
      return Entry;
    }

    Entry = ScorpiHwInfoNext (HwInfo, Entry);
  }

  return NULL;
}
