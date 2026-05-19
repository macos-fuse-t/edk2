/** @file
  Scorpi X64 hardware-info ABI.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SCORPI_X64_HWINFO_H__
#define __SCORPI_X64_HWINFO_H__

#include <Base.h>

#define SCORPI_X64_HWINFO_FWCFG_FILE    "opt/scorpi/x64-hardware-info"
#define SCORPI_X64_HWINFO_MAGIC         "SXHI"
#define SCORPI_X64_HWINFO_MAJOR         1
#define SCORPI_X64_HWINFO_MINOR         0

#define SCORPI_X64_HWINFO_HEADER_SIZE   40
#define SCORPI_X64_HWINFO_ENTRY_SIZE    8

#define SCORPI_X64_PROFILE_LINUX_VIRTIO   1
#define SCORPI_X64_PROFILE_WINDOWS_INBOX  2

#define SCORPI_X64_APIC_FLAG_X2APIC       BIT0

#define SCORPI_X64_ENTRY_RAM_RANGE       1
#define SCORPI_X64_ENTRY_RESERVED_RANGE  2
#define SCORPI_X64_ENTRY_CPU             3
#define SCORPI_X64_ENTRY_APIC            4
#define SCORPI_X64_ENTRY_PCIE_ECAM       5
#define SCORPI_X64_ENTRY_PCI_WINDOW      6
#define SCORPI_X64_ENTRY_DEVICE          7
#define SCORPI_X64_ENTRY_FRAMEBUFFER     8
#define SCORPI_X64_ENTRY_TPM             9
#define SCORPI_X64_ENTRY_RESET           10
#define SCORPI_X64_ENTRY_FLASH           11

#define SCORPI_X64_RANGE_USABLE          1
#define SCORPI_X64_RANGE_RESERVED        2
#define SCORPI_X64_RANGE_ACPI_RECLAIM    3
#define SCORPI_X64_RANGE_ACPI_NVS        4

#define SCORPI_X64_PCI_WINDOW_MMIO32     1
#define SCORPI_X64_PCI_WINDOW_MMIO64     2

#define SCORPI_X64_DEVICE_VIRTIO_BLK     1
#define SCORPI_X64_DEVICE_VIRTIO_NET     2
#define SCORPI_X64_DEVICE_VIRTIO_GPU     3
#define SCORPI_X64_DEVICE_AHCI           4
#define SCORPI_X64_DEVICE_XHCI           5
#define SCORPI_X64_DEVICE_USB_HID        6
#define SCORPI_X64_DEVICE_USB_NET        7

#define SCORPI_X64_TPM_INTERFACE_CRB     1
#define SCORPI_X64_TPM_INTERFACE_TIS     2

#pragma pack(1)

typedef struct {
  CHAR8     Magic[4];
  UINT16    Major;
  UINT16    Minor;
  UINT32    HeaderSize;
  UINT32    TotalSize;
  UINT32    Checksum32;
  UINT32    Flags;
  UINT32    Profile;
  UINT32    EntryCount;
  UINT64    EntriesOffset;
} SCORPI_X64_HWINFO_HEADER;

typedef struct {
  UINT16    Type;
  UINT16    Flags;
  UINT32    Size;
} SCORPI_X64_HWINFO_ENTRY;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT64                     Base;
  UINT64                     Size;
  UINT32                     RangeType;
  UINT32                     Flags;
} SCORPI_X64_HWINFO_RANGE;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT32                     AcpiProcessorUid;
  UINT32                     ApicId;
  UINT32                     Flags;
  UINT32                     Reserved;
} SCORPI_X64_HWINFO_CPU;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT64                     LocalApicBase;
  UINT64                     IoApicBase;
  UINT32                     IoApicId;
  UINT32                     GsiBase;
  UINT32                     GsiCount;
  UINT32                     Flags;
} SCORPI_X64_HWINFO_APIC;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT16                     Segment;
  UINT8                      StartBus;
  UINT8                      EndBus;
  UINT32                     Reserved;
  UINT64                     Base;
  UINT64                     Size;
} SCORPI_X64_HWINFO_PCIE_ECAM;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT32                     WindowType;
  UINT32                     Flags;
  UINT64                     CpuBase;
  UINT64                     PciBase;
  UINT64                     Size;
} SCORPI_X64_HWINFO_PCI_WINDOW;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT16                     DeviceType;
  UINT16                     Bus;
  UINT16                     Slot;
  UINT16                     Function;
  UINT32                     Flags;
  UINT32                     BootIndex;
} SCORPI_X64_HWINFO_DEVICE;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT64                     Base;
  UINT64                     Size;
  UINT32                     Width;
  UINT32                     Height;
  UINT32                     Stride;
  UINT32                     Format;
} SCORPI_X64_HWINFO_FRAMEBUFFER;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT64                     Base;
  UINT32                     Size;
  UINT32                     InterfaceType;
  UINT32                     Flags;
  UINT32                     Reserved;
} SCORPI_X64_HWINFO_TPM;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT64                     Base;
  UINT32                     Size;
  UINT32                     ResetOffset;
  UINT32                     ShutdownOffset;
  UINT32                     ResetValue;
  UINT32                     ShutdownValue;
  UINT32                     Flags;
} SCORPI_X64_HWINFO_RESET;

typedef struct {
  SCORPI_X64_HWINFO_ENTRY    Entry;
  UINT64                     Base;
  UINT64                     Size;
  UINT32                     BlockSize;
  UINT32                     Flags;
} SCORPI_X64_HWINFO_FLASH;

#pragma pack()

STATIC_ASSERT (
  sizeof (SCORPI_X64_HWINFO_HEADER) == SCORPI_X64_HWINFO_HEADER_SIZE,
  "unexpected Scorpi X64 hwinfo header size"
  );
STATIC_ASSERT (
  sizeof (SCORPI_X64_HWINFO_ENTRY) == SCORPI_X64_HWINFO_ENTRY_SIZE,
  "unexpected Scorpi X64 hwinfo entry size"
  );

#endif // __SCORPI_X64_HWINFO_H__
