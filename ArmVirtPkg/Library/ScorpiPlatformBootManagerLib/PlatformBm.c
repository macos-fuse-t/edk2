/** @file
  Implementation for PlatformBootManagerLib library class interfaces.

  Copyright (C) 2015-2016, Red Hat, Inc.
  Copyright (c) 2014 - 2023, Arm Ltd. All rights reserved.<BR>
  Copyright (c) 2004 - 2018, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2016, Linaro Ltd. All rights reserved.<BR>
  Copyright (c) 2021, Semihalf All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci22.h>
#include <Library/BootLogoLib.h>
#include <Library/CapsuleLib.h>
#include <Library/DevicePathLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/Tcg2PhysicalPresenceLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Protocol/BootManagerPolicy.h>
#include <Protocol/DevicePath.h>
#include <Protocol/EsrtManagement.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/NonDiscoverableDevice.h>
#include <Protocol/PciIo.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/PlatformBootManager.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/BootDiscoveryPolicy.h>
#include <Guid/EventGroup.h>
#include <Guid/AuthenticatedVariableFormat.h>
#include <Guid/GlobalVariable.h>
#include <Guid/NonDiscoverableDevice.h>
#include <Guid/TtyTerm.h>
#include <Guid/SerialPortLibVendor.h>
#include <Protocol/FdtClient.h>

#include "PlatformBm.h"

#define DP_NODE_LEN(Type)  { (UINT8)sizeof (Type), (UINT8)(sizeof (Type) >> 8) }

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH            SerialDxe;
  UART_DEVICE_PATH              Uart;
  VENDOR_DEFINED_DEVICE_PATH    TermType;
  EFI_DEVICE_PATH_PROTOCOL      End;
} PLATFORM_SERIAL_CONSOLE;
#pragma pack ()

STATIC PLATFORM_SERIAL_CONSOLE  mSerialConsole = {
  //
  // VENDOR_DEVICE_PATH SerialDxe
  //
  {
    { HARDWARE_DEVICE_PATH,  HW_VENDOR_DP, DP_NODE_LEN (VENDOR_DEVICE_PATH) },
    EDKII_SERIAL_PORT_LIB_VENDOR_GUID
  },

  //
  // UART_DEVICE_PATH Uart
  //
  {
    { MESSAGING_DEVICE_PATH, MSG_UART_DP,  DP_NODE_LEN (UART_DEVICE_PATH)   },
    0,                                      // Reserved
    FixedPcdGet64 (PcdUartDefaultBaudRate), // BaudRate
    FixedPcdGet8 (PcdUartDefaultDataBits),  // DataBits
    FixedPcdGet8 (PcdUartDefaultParity),    // Parity
    FixedPcdGet8 (PcdUartDefaultStopBits)   // StopBits
  },

  //
  // VENDOR_DEFINED_DEVICE_PATH TermType
  //
  {
    {
      MESSAGING_DEVICE_PATH, MSG_VENDOR_DP,
      DP_NODE_LEN (VENDOR_DEFINED_DEVICE_PATH)
    }
    //
    // Guid to be filled in dynamically
    //
  },

  //
  // EFI_DEVICE_PATH_PROTOCOL End
  //
  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    DP_NODE_LEN (EFI_DEVICE_PATH_PROTOCOL)
  }
};

#pragma pack (1)
typedef struct {
  USB_CLASS_DEVICE_PATH       Keyboard;
  EFI_DEVICE_PATH_PROTOCOL    End;
} PLATFORM_USB_KEYBOARD;
#pragma pack ()

STATIC PLATFORM_USB_KEYBOARD  mUsbKeyboard = {
  //
  // USB_CLASS_DEVICE_PATH Keyboard
  //
  {
    {
      MESSAGING_DEVICE_PATH, MSG_USB_CLASS_DP,
      DP_NODE_LEN (USB_CLASS_DEVICE_PATH)
    },
    0xFFFF, // VendorId: any
    0xFFFF, // ProductId: any
    3,      // DeviceClass: HID
    1,      // DeviceSubClass: boot
    1       // DeviceProtocol: keyboard
  },

  //
  // EFI_DEVICE_PATH_PROTOCOL End
  //
  {
    END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE,
    DP_NODE_LEN (EFI_DEVICE_PATH_PROTOCOL)
  }
};

STATIC
BOOLEAN
AsciiEqualsCi (
  IN CONST CHAR8  *Lhs,
  IN CONST CHAR8  *Rhs
  )
{
  CHAR8  L;
  CHAR8  R;

  while ((*Lhs != '\0') && (*Rhs != '\0')) {
    L = *Lhs++;
    R = *Rhs++;
    if ((L >= 'A') && (L <= 'Z')) {
      L = (CHAR8)(L - 'A' + 'a');
    }

    if ((R >= 'A') && (R <= 'Z')) {
      R = (CHAR8)(R - 'A' + 'a');
    }

    if (L != R) {
      return FALSE;
    }
  }

  return (*Lhs == '\0') && (*Rhs == '\0');
}

STATIC
EFI_STATUS
ScorpiGetChosenString (
  IN  CONST CHAR8  *PropertyName,
  OUT CONST CHAR8  **Value
  )
{
  FDT_CLIENT_PROTOCOL  *FdtClient;
  CONST VOID           *Prop;
  UINT32               PropSize;
  INT32                ChosenNode;
  EFI_STATUS           Status;

  *Value = NULL;

  Status = gBS->LocateProtocol (
                  &gFdtClientProtocolGuid,
                  NULL,
                  (VOID **)&FdtClient
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FdtClient->GetOrInsertChosenNode (FdtClient, &ChosenNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FdtClient->GetNodeProperty (
                        FdtClient,
                        ChosenNode,
                        PropertyName,
                        &Prop,
                        &PropSize
                        );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if ((PropSize == 0) || (((CONST CHAR8 *)Prop)[PropSize - 1] != '\0')) {
    return EFI_COMPROMISED_DATA;
  }

  *Value = (CONST CHAR8 *)Prop;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ScorpiGetFwCfgString (
  IN  CONST CHAR8  *FileName,
  OUT CHAR8        **Value
  )
{
  FIRMWARE_CONFIG_ITEM  FwCfgItem;
  RETURN_STATUS         ReturnStatus;
  UINTN                 FwCfgSize;
  CHAR8                 *Buffer;

  *Value = NULL;

  if (!QemuFwCfgIsAvailable ()) {
    return EFI_NOT_FOUND;
  }

  ReturnStatus = QemuFwCfgFindFile (FileName, &FwCfgItem, &FwCfgSize);
  if (RETURN_ERROR (ReturnStatus) || (FwCfgSize == 0)) {
    return EFI_NOT_FOUND;
  }

  Buffer = AllocateZeroPool (FwCfgSize + 1);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  QemuFwCfgSelectItem (FwCfgItem);
  QemuFwCfgReadBytes (FwCfgSize, Buffer);
  Buffer[FwCfgSize] = '\0';

  *Value = Buffer;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ScorpiGetFirmwareString (
  IN  CONST CHAR8  *FwCfgName,
  IN  CONST CHAR8  *FdtName,
  OUT CHAR8        **Value
  )
{
  CONST CHAR8  *ChosenValue;
  EFI_STATUS   Status;
  UINTN        Size;

  Status = ScorpiGetFwCfgString (FwCfgName, Value);
  if (!EFI_ERROR (Status)) {
    return Status;
  }

  Status = ScorpiGetChosenString (FdtName, &ChosenValue);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Size   = AsciiStrSize (ChosenValue);
  *Value = AllocateCopyPool (Size, ChosenValue);
  if (*Value == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  return EFI_SUCCESS;
}

STATIC
BOOLEAN
ScorpiParseUint16 (
  IN  CONST CHAR8  *Value,
  OUT UINT16       *Result
  )
{
  UINTN  Parsed;

  if ((Value == NULL) || (*Value == '\0')) {
    return FALSE;
  }

  Parsed = 0;
  while (*Value != '\0') {
    if ((*Value < '0') || (*Value > '9')) {
      return FALSE;
    }

    Parsed = Parsed * 10 + (UINTN)(*Value - '0');
    if (Parsed > MAX_UINT16) {
      return FALSE;
    }

    Value++;
  }

  *Result = (UINT16)Parsed;
  return TRUE;
}

STATIC
BOOLEAN
ScorpiParseSecureBootValue (
  IN  CONST CHAR8  *Value,
  OUT UINT8        *Result
  )
{
  if (AsciiEqualsCi (Value, "on") ||
      AsciiEqualsCi (Value, "off"))
  {
    *Result = AsciiEqualsCi (Value, "on") ? SECURE_BOOT_ENABLE :
                                            SECURE_BOOT_DISABLE;
    return TRUE;
  }

  return FALSE;
}

STATIC
VOID
ScorpiApplyTimeoutParam (
  VOID
  )
{
  CHAR8       *Value;
  EFI_STATUS   Status;
  UINT16       Timeout;

  Status = ScorpiGetFirmwareString (
             "opt/scorpi/boot-timeout",
             "scorpi,timeout",
             &Value
             );
  if (EFI_ERROR (Status)) {
    return;
  }

  if (!ScorpiParseUint16 (Value, &Timeout)) {
    DEBUG ((DEBUG_WARN, "%a: invalid scorpi,timeout='%a'\n", __func__, Value));
    FreePool (Value);
    return;
  }

  Status = PcdSet16S (PcdPlatformBootTimeOut, Timeout);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: PcdSet16S failed: %r\n", __func__, Status));
  }

  Status = gRT->SetVariable (
                  EFI_TIME_OUT_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof (Timeout),
                  &Timeout
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: Timeout variable update failed: %r\n", __func__, Status));
  }

  FreePool (Value);
}

STATIC
VOID
ScorpiApplySecureBootParam (
  VOID
  )
{
  CHAR8       *Value;
  EFI_STATUS   Status;
  UINT8        SecureBootEnable;

  Status = ScorpiGetFirmwareString (
             "opt/scorpi/secure-boot",
             "scorpi,secure-boot",
             &Value
             );
  if (EFI_ERROR (Status)) {
    Value = AllocateCopyPool (sizeof ("off"), "off");
    if (Value == NULL) {
      return;
    }
  }

  if (!ScorpiParseSecureBootValue (Value, &SecureBootEnable)) {
    DEBUG ((DEBUG_WARN, "%a: invalid scorpi,secure-boot='%a'\n", __func__, Value));
    FreePool (Value);
    return;
  }

  Status = gRT->SetVariable (
                  EFI_SECURE_BOOT_ENABLE_NAME,
                  &gEfiSecureBootEnableDisableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                  sizeof (SecureBootEnable),
                  &SecureBootEnable
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: SecureBootEnable update failed: %r\n", __func__, Status));
  }

  FreePool (Value);
}

#define SCORPI_BOOT_ID_MAX  64
#define SCORPI_BOOT_TYPE_MAX  32

STATIC CHAR16  mScorpiBootFileDesc[] = L"Scorpi boot file";

typedef struct {
  CHAR8      Id[SCORPI_BOOT_ID_MAX];
  CHAR8      Type[SCORPI_BOOT_TYPE_MAX];
  UINT8      Bus;
  UINT8      Slot;
  UINT8      Func;
  UINT16     Port;
  BOOLEAN    HasPort;
} SCORPI_BOOT_DEVICE;

STATIC
BOOLEAN
ScorpiAsciiTokenEquals (
  IN CONST CHAR8  *Token,
  IN UINTN        TokenLen,
  IN CONST CHAR8  *Value
  )
{
  UINTN  Index;

  for (Index = 0; Index < TokenLen; Index++) {
    if (Value[Index] == '\0' || Token[Index] != Value[Index]) {
      return FALSE;
    }
  }

  return Value[TokenLen] == '\0';
}

STATIC
BOOLEAN
ScorpiParseDecimalToken (
  IN  CONST CHAR8  *Token,
  IN  UINTN        TokenLen,
  OUT UINTN        *Result
  )
{
  UINTN  Index;
  UINTN  Parsed;

  if (TokenLen == 0) {
    return FALSE;
  }

  Parsed = 0;
  for (Index = 0; Index < TokenLen; Index++) {
    if ((Token[Index] < '0') || (Token[Index] > '9')) {
      return FALSE;
    }

    Parsed = Parsed * 10 + (UINTN)(Token[Index] - '0');
  }

  *Result = Parsed;
  return TRUE;
}

STATIC
BOOLEAN
ScorpiCopyToken (
  OUT CHAR8        *Dest,
  IN  UINTN        DestSize,
  IN  CONST CHAR8  *Token,
  IN  UINTN        TokenLen
  )
{
  if ((DestSize == 0) || (TokenLen >= DestSize)) {
    return FALSE;
  }

  CopyMem (Dest, Token, TokenLen);
  Dest[TokenLen] = '\0';
  return TRUE;
}

STATIC
BOOLEAN
ScorpiParseBootMapField (
  IN OUT SCORPI_BOOT_DEVICE  *Device,
  IN     CONST CHAR8         *Key,
  IN     UINTN               KeyLen,
  IN     CONST CHAR8         *Value,
  IN     UINTN               ValueLen
  )
{
  UINTN  Parsed;

  if (ScorpiAsciiTokenEquals (Key, KeyLen, "id")) {
    return ScorpiCopyToken (Device->Id, sizeof (Device->Id), Value, ValueLen);
  }

  if (ScorpiAsciiTokenEquals (Key, KeyLen, "type")) {
    return ScorpiCopyToken (Device->Type, sizeof (Device->Type), Value, ValueLen);
  }

  if (!ScorpiParseDecimalToken (Value, ValueLen, &Parsed)) {
    return FALSE;
  }

  if (ScorpiAsciiTokenEquals (Key, KeyLen, "bus")) {
    if (Parsed > MAX_UINT8) {
      return FALSE;
    }

    Device->Bus = (UINT8)Parsed;
    return TRUE;
  }

  if (ScorpiAsciiTokenEquals (Key, KeyLen, "slot")) {
    if (Parsed > MAX_UINT8) {
      return FALSE;
    }

    Device->Slot = (UINT8)Parsed;
    return TRUE;
  }

  if (ScorpiAsciiTokenEquals (Key, KeyLen, "func")) {
    if (Parsed > MAX_UINT8) {
      return FALSE;
    }

    Device->Func = (UINT8)Parsed;
    return TRUE;
  }

  if (ScorpiAsciiTokenEquals (Key, KeyLen, "port")) {
    if (Parsed <= MAX_UINT16) {
      Device->Port    = (UINT16)Parsed;
      Device->HasPort = TRUE;
    }

    return TRUE;
  }

  return TRUE;
}

STATIC
BOOLEAN
ScorpiParseBootMapLine (
  IN  CONST CHAR8          *Line,
  IN  UINTN                LineLen,
  OUT SCORPI_BOOT_DEVICE   *Device
  )
{
  CONST CHAR8  *Field;
  CONST CHAR8  *FieldEnd;
  CONST CHAR8  *LineEnd;
  CONST CHAR8  *Equals;

  ZeroMem (Device, sizeof (*Device));
  Device->Port    = MAX_UINT16;
  Device->HasPort = FALSE;

  Field   = Line;
  LineEnd = Line + LineLen;
  while (Field < LineEnd) {
    FieldEnd = Field;
    while ((FieldEnd < LineEnd) && (*FieldEnd != ',')) {
      FieldEnd++;
    }

    Equals = Field;
    while ((Equals < FieldEnd) && (*Equals != '=')) {
      Equals++;
    }

    if ((Equals == Field) || (Equals == FieldEnd)) {
      return FALSE;
    }

    if (!ScorpiParseBootMapField (
           Device,
           Field,
           (UINTN)(Equals - Field),
           Equals + 1,
           (UINTN)(FieldEnd - Equals - 1)
           ))
    {
      return FALSE;
    }

    Field = FieldEnd + 1;
  }

  return Device->Id[0] != '\0';
}

STATIC
BOOLEAN
ScorpiFindBootDevice (
  IN  CONST CHAR8          *Id,
  OUT SCORPI_BOOT_DEVICE   *Device
  )
{
  CHAR8       *Map;
  CONST CHAR8 *Line;
  CONST CHAR8 *LineEnd;
  EFI_STATUS  Status;

  if (Id[0] == '@') {
    Id++;
  }

  Status = ScorpiGetFirmwareString (
             "opt/scorpi/boot-map",
             "scorpi,boot-map",
             &Map
             );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Line = Map;
  while (*Line != '\0') {
    LineEnd = Line;
    while ((*LineEnd != '\0') && (*LineEnd != '\n')) {
      LineEnd++;
    }

    if (ScorpiParseBootMapLine (Line, (UINTN)(LineEnd - Line), Device) &&
        AsciiEqualsCi (Device->Id, Id))
    {
      FreePool (Map);
      return TRUE;
    }

    Line = (*LineEnd == '\n') ? LineEnd + 1 : LineEnd;
  }

  FreePool (Map);
  return FALSE;
}

STATIC
BOOLEAN
ScorpiBootOptionMatchesDevice (
  IN CONST EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption,
  IN CONST SCORPI_BOOT_DEVICE            *Device
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *Node;
  PCI_DEVICE_PATH           *Pci;
  SATA_DEVICE_PATH          *Sata;
  BOOLEAN                   PciMatched;
  BOOLEAN                   PortMatched;

  PciMatched  = FALSE;
  PortMatched = !Device->HasPort;

  for (Node = BootOption->FilePath; !IsDevicePathEnd (Node);
       Node = NextDevicePathNode (Node))
  {
    if ((DevicePathType (Node) == HARDWARE_DEVICE_PATH) &&
        (DevicePathSubType (Node) == HW_PCI_DP))
    {
      Pci = (PCI_DEVICE_PATH *)Node;
      if ((Pci->Device == Device->Slot) && (Pci->Function == Device->Func)) {
        PciMatched = TRUE;
      }
    }

    if ((DevicePathType (Node) == MESSAGING_DEVICE_PATH) &&
        (DevicePathSubType (Node) == MSG_SATA_DP))
    {
      Sata = (SATA_DEVICE_PATH *)Node;
      if (Device->HasPort && (Sata->HBAPortNumber == Device->Port)) {
        PortMatched = TRUE;
      }
    }
  }

  return PciMatched && PortMatched;
}

STATIC
BOOLEAN
ScorpiFileExistsOnFsHandle (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *Path
  )
{
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL  *SimpleFs;
  EFI_FILE_PROTOCOL                *Root;
  EFI_FILE_PROTOCOL                *File;
  EFI_STATUS                       Status;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&SimpleFs
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = SimpleFs->OpenVolume (SimpleFs, &Root);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  Status = Root->Open (
                   Root,
                   &File,
                   (CHAR16 *)Path,
                   EFI_FILE_MODE_READ,
                   0
                   );
  Root->Close (Root);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  File->Close (File);
  return TRUE;
}

STATIC
EFI_STATUS
ScorpiInitializeBootFileOption (
  IN  EFI_HANDLE                    Handle,
  IN  CONST CHAR16                  *Path,
  OUT EFI_BOOT_MANAGER_LOAD_OPTION  *NewOption
  )
{
  EFI_DEVICE_PATH_PROTOCOL      *DevicePath;
  EFI_STATUS                    Status;

  DevicePath = FileDevicePath (Handle, (CHAR16 *)Path);
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = EfiBootManagerInitializeLoadOption (
             NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             LOAD_OPTION_ACTIVE,
             mScorpiBootFileDesc,
             DevicePath,
             NULL,
             0
             );
  FreePool (DevicePath);

  return Status;
}

STATIC
CHAR16 *
ScorpiAsciiPathToChar16 (
  IN CONST CHAR8  *Path
  )
{
  CHAR16  *Result;
  UINTN   Index;
  UINTN   Size;

  Size   = AsciiStrLen (Path) + 1;
  Result = AllocateZeroPool (Size * sizeof (*Result));
  if (Result == NULL) {
    return NULL;
  }

  for (Index = 0; Index < Size; Index++) {
    if (Path[Index] == '/') {
      Result[Index] = L'\\';
    } else {
      Result[Index] = (CHAR16)Path[Index];
    }
  }

  return Result;
}

STATIC
BOOLEAN
ScorpiResolveBootFileSpec (
  IN  CONST SCORPI_BOOT_DEVICE  *Device,
  IN  CONST CHAR8               *BootFile,
  OUT UINT16                    *OptionNumber
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  EFI_BOOT_MANAGER_LOAD_OPTION  NewOption;
  EFI_HANDLE                    *Handles;
  EFI_STATUS                    Status;
  CHAR16                        *BootFilePath;
  INTN                          OptionIndex;
  UINTN                         BootOptionCount;
  UINTN                         HandleCount;
  UINTN                         Index;

  BootFilePath = ScorpiAsciiPathToChar16 (BootFile);
  if (BootFilePath == NULL) {
    return FALSE;
  }

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    FreePool (BootFilePath);
    return FALSE;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    if (!ScorpiFileExistsOnFsHandle (Handles[Index], BootFilePath)) {
      continue;
    }

    Status = ScorpiInitializeBootFileOption (
               Handles[Index],
               BootFilePath,
               &NewOption
               );
    if (EFI_ERROR (Status)) {
      continue;
    }

    if (!ScorpiBootOptionMatchesDevice (&NewOption, Device)) {
      EfiBootManagerFreeLoadOption (&NewOption);
      continue;
    }

    BootOptions = EfiBootManagerGetLoadOptions (
                    &BootOptionCount,
                    LoadOptionTypeBoot
                    );
    OptionIndex = EfiBootManagerFindLoadOption (
                    &NewOption,
                    BootOptions,
                    BootOptionCount
                    );
    if (OptionIndex >= 0) {
      *OptionNumber = (UINT16)BootOptions[OptionIndex].OptionNumber;
    } else {
      Status = EfiBootManagerAddLoadOptionVariable (&NewOption, MAX_UINTN);
      if (EFI_ERROR (Status)) {
        EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
        EfiBootManagerFreeLoadOption (&NewOption);
        continue;
      }

      *OptionNumber = (UINT16)NewOption.OptionNumber;
    }

    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    EfiBootManagerFreeLoadOption (&NewOption);
    FreePool (Handles);
    FreePool (BootFilePath);
    return TRUE;
  }

  FreePool (Handles);
  FreePool (BootFilePath);
  return FALSE;
}

STATIC
BOOLEAN
ScorpiResolveBootSpec (
  IN  CONST CHAR8                    *Spec,
  IN  CONST CHAR8                    *BootFile OPTIONAL,
  IN  EFI_BOOT_MANAGER_LOAD_OPTION   *BootOptions,
  IN  UINTN                          BootOptionCount,
  OUT UINT16                         *OptionNumber
  )
{
  SCORPI_BOOT_DEVICE  Device;
  UINTN               Index;

  if (!ScorpiFindBootDevice (Spec, &Device)) {
    return FALSE;
  }

  if ((BootFile != NULL) && (BootFile[0] != '\0')) {
    return ScorpiResolveBootFileSpec (&Device, BootFile, OptionNumber);
  }

  for (Index = 0; Index < BootOptionCount; Index++) {
    if (ScorpiBootOptionMatchesDevice (&BootOptions[Index], &Device)) {
      *OptionNumber = (UINT16)BootOptions[Index].OptionNumber;
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
BOOLEAN
ScorpiBootOrderContains (
  IN CONST UINT16  *BootOrder,
  IN UINTN         BootOrderCount,
  IN UINT16        OptionNumber
  )
{
  UINTN  Index;

  for (Index = 0; Index < BootOrderCount; Index++) {
    if (BootOrder[Index] == OptionNumber) {
      return TRUE;
    }
  }

  return FALSE;
}

STATIC
VOID
ScorpiApplyBootOrderParam (
  VOID
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  CONST CHAR8                   *Cursor;
  CONST CHAR8                   *SpecEnd;
  CHAR8                         *BootFile;
  CHAR8                         *Value;
  EFI_STATUS                    Status;
  UINT16                        *BootOrder;
  UINT16                        OptionNumber;
  UINTN                         BootOptionCount;
  UINTN                         BootOrderCount;
  UINTN                         Index;
  CHAR8                         Spec[SCORPI_BOOT_ID_MAX];

  Status = ScorpiGetFirmwareString (
             "opt/scorpi/boot-order",
             "scorpi,boot-order",
             &Value
             );
  if (EFI_ERROR (Status)) {
    return;
  }

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  BootOrder   = AllocateZeroPool ((BootOptionCount + 1) * sizeof (*BootOrder));
  if (BootOrder == NULL) {
    FreePool (Value);
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    return;
  }

  Status = ScorpiGetFirmwareString (
             "opt/scorpi/boot-file",
             "scorpi,boot-file",
             &BootFile
             );
  if (EFI_ERROR (Status)) {
    BootFile = NULL;
  }

  BootOrderCount = 0;
  Cursor         = Value;
  while (*Cursor != '\0') {
    SpecEnd = Cursor;
    while ((*SpecEnd != '\0') && (*SpecEnd != ':')) {
      SpecEnd++;
    }

    if (!ScorpiCopyToken (Spec, sizeof (Spec), Cursor, (UINTN)(SpecEnd - Cursor)) ||
        !ScorpiResolveBootSpec (Spec, BootFile, BootOptions, BootOptionCount, &OptionNumber))
    {
      DEBUG ((DEBUG_WARN, "%a: no boot option matched boot order entry '%a'\n", __func__, Spec));
    } else if (!ScorpiBootOrderContains (BootOrder, BootOrderCount, OptionNumber)) {
      BootOrder[BootOrderCount++] = OptionNumber;
    }

    Cursor = (*SpecEnd == ':') ? SpecEnd + 1 : SpecEnd;
  }

  for (Index = 0; Index < BootOptionCount; Index++) {
    OptionNumber = (UINT16)BootOptions[Index].OptionNumber;
    if (!ScorpiBootOrderContains (BootOrder, BootOrderCount, OptionNumber)) {
      BootOrder[BootOrderCount++] = OptionNumber;
    }
  }

  if (BootOrderCount == 0) {
    if (BootFile != NULL) {
      FreePool (BootFile);
    }
    FreePool (Value);
    FreePool (BootOrder);
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    return;
  }

  Status = gRT->SetVariable (
                  EFI_BOOT_ORDER_VARIABLE_NAME,
                  &gEfiGlobalVariableGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS |
                  EFI_VARIABLE_RUNTIME_ACCESS,
                  BootOrderCount * sizeof (*BootOrder),
                  BootOrder
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_WARN, "%a: BootOrder update failed: %r\n", __func__, Status));
  }

  if (BootFile != NULL) {
    FreePool (BootFile);
  }
  FreePool (Value);
  FreePool (BootOrder);
  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

/**
  Check if the handle satisfies a particular condition.

  @param[in] Handle      The handle to check.
  @param[in] ReportText  A caller-allocated string passed in for reporting
                         purposes. It must never be NULL.

  @retval TRUE   The condition is satisfied.
  @retval FALSE  Otherwise. This includes the case when the condition could not
                 be fully evaluated due to an error.
**/
typedef
BOOLEAN
(EFIAPI *FILTER_FUNCTION)(
  IN EFI_HANDLE   Handle,
  IN CONST CHAR16 *ReportText
  );

/**
  Process a handle.

  @param[in] Handle      The handle to process.
  @param[in] ReportText  A caller-allocated string passed in for reporting
                         purposes. It must never be NULL.
**/
typedef
VOID
(EFIAPI *CALLBACK_FUNCTION)(
  IN EFI_HANDLE   Handle,
  IN CONST CHAR16 *ReportText
  );

/**
  Locate all handles that carry the specified protocol, filter them with a
  callback function, and pass each handle that passes the filter to another
  callback.

  @param[in] ProtocolGuid  The protocol to look for.

  @param[in] Filter        The filter function to pass each handle to. If this
                           parameter is NULL, then all handles are processed.

  @param[in] Process       The callback function to pass each handle to that
                           clears the filter.
**/
STATIC
VOID
FilterAndProcess (
  IN EFI_GUID           *ProtocolGuid,
  IN FILTER_FUNCTION    Filter         OPTIONAL,
  IN CALLBACK_FUNCTION  Process
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  *Handles;
  UINTN       NoHandles;
  UINTN       Idx;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  ProtocolGuid,
                  NULL /* SearchKey */,
                  &NoHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    //
    // This is not an error, just an informative condition.
    //
    DEBUG ((
      DEBUG_VERBOSE,
      "%a: %g: %r\n",
      __func__,
      ProtocolGuid,
      Status
      ));
    return;
  }

  ASSERT (NoHandles > 0);
  for (Idx = 0; Idx < NoHandles; ++Idx) {
    CHAR16         *DevicePathText;
    STATIC CHAR16  Fallback[] = L"<device path unavailable>";

    //
    // The ConvertDevicePathToText() function handles NULL input transparently.
    //
    DevicePathText = ConvertDevicePathToText (
                       DevicePathFromHandle (Handles[Idx]),
                       FALSE, // DisplayOnly
                       FALSE  // AllowShortcuts
                       );
    if (DevicePathText == NULL) {
      DevicePathText = Fallback;
    }

    if ((Filter == NULL) || Filter (Handles[Idx], DevicePathText)) {
      Process (Handles[Idx], DevicePathText);
    }

    if (DevicePathText != Fallback) {
      FreePool (DevicePathText);
    }
  }

  gBS->FreePool (Handles);
}

/**
  This FILTER_FUNCTION checks if a handle corresponds to a PCI display device.
**/
STATIC
BOOLEAN
EFIAPI
IsPciDisplay (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS           Status;
  EFI_PCI_IO_PROTOCOL  *PciIo;
  PCI_TYPE00           Pci;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPciIoProtocolGuid,
                  (VOID **)&PciIo
                  );
  if (EFI_ERROR (Status)) {
    //
    // This is not an error worth reporting.
    //
    return FALSE;
  }

  Status = PciIo->Pci.Read (
                        PciIo,
                        EfiPciIoWidthUint32,
                        0 /* Offset */,
                        sizeof Pci / sizeof (UINT32),
                        &Pci
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: %s: %r\n", __func__, ReportText, Status));
    return FALSE;
  }

  return IS_PCI_DISPLAY (&Pci);
}

/**
  This FILTER_FUNCTION checks if a handle corresponds to a non-discoverable
  USB host controller.
**/
STATIC
BOOLEAN
EFIAPI
IsUsbHost (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  NON_DISCOVERABLE_DEVICE  *Device;
  EFI_STATUS               Status;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid,
                  (VOID **)&Device
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (CompareGuid (Device->Type, &gEdkiiNonDiscoverableUhciDeviceGuid) ||
      CompareGuid (Device->Type, &gEdkiiNonDiscoverableEhciDeviceGuid) ||
      CompareGuid (Device->Type, &gEdkiiNonDiscoverableXhciDeviceGuid))
  {
    return TRUE;
  }

  return FALSE;
}

/**
  This CALLBACK_FUNCTION attempts to connect a handle non-recursively, asking
  the matching driver to produce all first-level child handles.
**/
STATIC
VOID
EFIAPI
Connect (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS  Status;

  Status = gBS->ConnectController (
                  Handle, // ControllerHandle
                  NULL,   // DriverImageHandle
                  NULL,   // RemainingDevicePath -- produce all children
                  FALSE   // Recursive
                  );
  DEBUG ((
    EFI_ERROR (Status) ? DEBUG_ERROR : DEBUG_VERBOSE,
    "%a: %s: %r\n",
    __func__,
    ReportText,
    Status
    ));
}

/**
  This CALLBACK_FUNCTION retrieves the EFI_DEVICE_PATH_PROTOCOL from the
  handle, and adds it to ConOut and ErrOut.
**/
STATIC
VOID
EFIAPI
AddOutput (
  IN EFI_HANDLE    Handle,
  IN CONST CHAR16  *ReportText
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  DevicePath = DevicePathFromHandle (Handle);
  if (DevicePath == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s: handle %p: device path not found\n",
      __func__,
      ReportText,
      Handle
      ));
    return;
  }

  Status = EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s: adding to ConOut: %r\n",
      __func__,
      ReportText,
      Status
      ));
    return;
  }

  Status = EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: %s: adding to ErrOut: %r\n",
      __func__,
      ReportText,
      Status
      ));
    return;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: %s: added to ConOut and ErrOut\n",
    __func__,
    ReportText
    ));
}

STATIC
VOID
PlatformRegisterFvBootOption (
  CONST EFI_GUID  *FileGuid,
  CHAR16          *Description,
  UINT32          Attributes,
  EFI_INPUT_KEY   *Key
  )
{
  EFI_STATUS                         Status;
  INTN                               OptionIndex;
  EFI_BOOT_MANAGER_LOAD_OPTION       NewOption;
  EFI_BOOT_MANAGER_LOAD_OPTION       *BootOptions;
  UINTN                              BootOptionCount;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  ASSERT_EFI_ERROR (Status);

  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  ASSERT (DevicePath != NULL);
  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  ASSERT (DevicePath != NULL);

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             Attributes,
             Description,
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);
  FreePool (DevicePath);

  BootOptions = EfiBootManagerGetLoadOptions (
                  &BootOptionCount,
                  LoadOptionTypeBoot
                  );

  OptionIndex = EfiBootManagerFindLoadOption (
                  &NewOption,
                  BootOptions,
                  BootOptionCount
                  );

  if (OptionIndex == -1) {
    Status = EfiBootManagerAddLoadOptionVariable (&NewOption, MAX_UINTN);
    ASSERT_EFI_ERROR (Status);
    Status = EfiBootManagerAddKeyOptionVariable (
               NULL,
               (UINT16)NewOption.OptionNumber,
               0,
               Key,
               NULL
               );
    ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
  }

  EfiBootManagerFreeLoadOption (&NewOption);
  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

/** Boot a Fv Boot Option.

  This function is useful for booting the UEFI Shell as it is loaded
  as a non active boot option.

  @param[in] FileGuid      The File GUID.
  @param[in] Description   String describing the Boot Option.

**/
STATIC
VOID
PlatformBootFvBootOption (
  IN  CONST EFI_GUID  *FileGuid,
  IN  CHAR16          *Description
  )
{
  EFI_STATUS                         Status;
  EFI_BOOT_MANAGER_LOAD_OPTION       NewOption;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH  FileNode;
  EFI_LOADED_IMAGE_PROTOCOL          *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL           *DevicePath;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // The UEFI Shell was registered in PlatformRegisterFvBootOption ()
  // previously, thus it must still be available in this FV.
  //
  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  ASSERT (DevicePath != NULL);
  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *)&FileNode
                 );
  ASSERT (DevicePath != NULL);

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             LOAD_OPTION_ACTIVE,
             Description,
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);
  FreePool (DevicePath);

  EfiBootManagerBoot (&NewOption);
}

STATIC
VOID
GetPlatformOptions (
  VOID
  )
{
  EFI_STATUS                      Status;
  EFI_BOOT_MANAGER_LOAD_OPTION    *CurrentBootOptions;
  EFI_BOOT_MANAGER_LOAD_OPTION    *BootOptions;
  EFI_INPUT_KEY                   *BootKeys;
  PLATFORM_BOOT_MANAGER_PROTOCOL  *PlatformBootManager;
  UINTN                           CurrentBootOptionCount;
  UINTN                           Index;
  UINTN                           BootCount;

  Status = gBS->LocateProtocol (
                  &gPlatformBootManagerProtocolGuid,
                  NULL,
                  (VOID **)&PlatformBootManager
                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = PlatformBootManager->GetPlatformBootOptionsAndKeys (
                                  &BootCount,
                                  &BootOptions,
                                  &BootKeys
                                  );
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Fetch the existent boot options. If there are none, CurrentBootCount
  // will be zeroed.
  //
  CurrentBootOptions = EfiBootManagerGetLoadOptions (
                         &CurrentBootOptionCount,
                         LoadOptionTypeBoot
                         );
  //
  // Process the platform boot options.
  //
  for (Index = 0; Index < BootCount; Index++) {
    INTN   Match;
    UINTN  BootOptionNumber;

    //
    // If there are any preexistent boot options, and the subject platform boot
    // option is already among them, then don't try to add it. Just get its
    // assigned boot option number so we can associate a hotkey with it. Note
    // that EfiBootManagerFindLoadOption() deals fine with (CurrentBootOptions
    // == NULL) if (CurrentBootCount == 0).
    //
    Match = EfiBootManagerFindLoadOption (
              &BootOptions[Index],
              CurrentBootOptions,
              CurrentBootOptionCount
              );
    if (Match >= 0) {
      BootOptionNumber = CurrentBootOptions[Match].OptionNumber;
    } else {
      //
      // Add the platform boot options as a new one, at the end of the boot
      // order. Note that if the platform provided this boot option with an
      // unassigned option number, then the below function call will assign a
      // number.
      //
      Status = EfiBootManagerAddLoadOptionVariable (
                 &BootOptions[Index],
                 MAX_UINTN
                 );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to register \"%s\": %r\n",
          __func__,
          BootOptions[Index].Description,
          Status
          ));
        continue;
      }

      BootOptionNumber = BootOptions[Index].OptionNumber;
    }

    //
    // Register a hotkey with the boot option, if requested.
    //
    if (BootKeys[Index].UnicodeChar == L'\0') {
      continue;
    }

    Status = EfiBootManagerAddKeyOptionVariable (
               NULL,
               BootOptionNumber,
               0,
               &BootKeys[Index],
               NULL
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to register hotkey for \"%s\": %r\n",
        __func__,
        BootOptions[Index].Description,
        Status
        ));
    }
  }

  EfiBootManagerFreeLoadOptions (CurrentBootOptions, CurrentBootOptionCount);
  EfiBootManagerFreeLoadOptions (BootOptions, BootCount);
  FreePool (BootKeys);
}

STATIC
VOID
PlatformRegisterOptionsAndKeys (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_INPUT_KEY                 Enter;
  EFI_INPUT_KEY                 F2;
  EFI_INPUT_KEY                 Esc;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootOption;

  GetPlatformOptions ();

  //
  // Register ENTER as CONTINUE key
  //
  Enter.ScanCode    = SCAN_NULL;
  Enter.UnicodeChar = CHAR_CARRIAGE_RETURN;
  Status            = EfiBootManagerRegisterContinueKeyOption (0, &Enter, NULL);
  ASSERT_EFI_ERROR (Status);

  //
  // Map F2 and ESC to Boot Manager Menu
  //
  F2.ScanCode     = SCAN_F2;
  F2.UnicodeChar  = CHAR_NULL;
  Esc.ScanCode    = SCAN_ESC;
  Esc.UnicodeChar = CHAR_NULL;
  Status          = EfiBootManagerGetBootManagerMenu (&BootOption);
  ASSERT_EFI_ERROR (Status);
  Status = EfiBootManagerAddKeyOptionVariable (
             NULL,
             (UINT16)BootOption.OptionNumber,
             0,
             &F2,
             NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
  Status = EfiBootManagerAddKeyOptionVariable (
             NULL,
             (UINT16)BootOption.OptionNumber,
             0,
             &Esc,
             NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
}

//
// BDS Platform Functions
//

/**
  Do the platform init, can be customized by OEM/IBV
  Possible things that can be done in PlatformBootManagerBeforeConsole:
  > Update console variable: 1. include hot-plug devices;
  >                          2. Clear ConIn and add SOL for AMT
  > Register new Driver#### or Boot####
  > Register new Key####: e.g.: F12
  > Signal ReadyToLock event
  > Authentication action: 1. connect Auth devices;
  >                        2. Identify auto logon user.
**/
VOID
EFIAPI
PlatformBootManagerBeforeConsole (
  VOID
  )
{
  ScorpiApplyTimeoutParam ();
  ScorpiApplySecureBootParam ();

  //
  // Signal EndOfDxe PI Event
  //
  EfiEventGroupSignal (&gEfiEndOfDxeEventGroupGuid);

  //
  // Dispatch deferred images after EndOfDxe event.
  //
  EfiBootManagerDispatchDeferredImages ();

  //
  // Locate the PCI root bridges and make the PCI bus driver connect each,
  // non-recursively. This will produce a number of child handles with PciIo on
  // them.
  //
  FilterAndProcess (&gEfiPciRootBridgeIoProtocolGuid, NULL, Connect);

  //
  // Find all display class PCI devices (using the handles from the previous
  // step), and connect them non-recursively. This should produce a number of
  // child handles with GOPs on them.
  //
  FilterAndProcess (&gEfiPciIoProtocolGuid, IsPciDisplay, Connect);

  //
  // Now add the device path of all handles with GOP on them to ConOut and
  // ErrOut.
  //
  FilterAndProcess (&gEfiGraphicsOutputProtocolGuid, NULL, AddOutput);

  //
  // The core BDS code connects short-form USB device paths by explicitly
  // looking for handles with PCI I/O installed, and checking the PCI class
  // code whether it matches the one for a USB host controller. This means
  // non-discoverable USB host controllers need to have the non-discoverable
  // PCI driver attached first.
  //
  FilterAndProcess (&gEdkiiNonDiscoverableDeviceProtocolGuid, IsUsbHost, Connect);

  //
  // Add the hardcoded short-form USB keyboard device path to ConIn.
  //
  EfiBootManagerUpdateConsoleVariable (
    ConIn,
    (EFI_DEVICE_PATH_PROTOCOL *)&mUsbKeyboard,
    NULL
    );

  //
  // Add the hardcoded serial console device path to ConIn, ConOut, ErrOut.
  //
  STATIC_ASSERT (
    FixedPcdGet8 (PcdDefaultTerminalType) == 4,
    "PcdDefaultTerminalType must be TTYTERM"
    );
  STATIC_ASSERT (
    FixedPcdGet8 (PcdUartDefaultParity) != 0,
    "PcdUartDefaultParity must be set to an actual value, not 'default'"
    );
  STATIC_ASSERT (
    FixedPcdGet8 (PcdUartDefaultStopBits) != 0,
    "PcdUartDefaultStopBits must be set to an actual value, not 'default'"
    );

  CopyGuid (&mSerialConsole.TermType.Guid, &gEfiTtyTermGuid);

  EfiBootManagerUpdateConsoleVariable (
    ConIn,
    (EFI_DEVICE_PATH_PROTOCOL *)&mSerialConsole,
    NULL
    );
  EfiBootManagerUpdateConsoleVariable (
    ConOut,
    (EFI_DEVICE_PATH_PROTOCOL *)&mSerialConsole,
    NULL
    );
  EfiBootManagerUpdateConsoleVariable (
    ErrOut,
    (EFI_DEVICE_PATH_PROTOCOL *)&mSerialConsole,
    NULL
    );

  //
  // Register platform-specific boot options and keyboard shortcuts.
  //
  PlatformRegisterOptionsAndKeys ();
}

STATIC
VOID
HandleCapsules (
  VOID
  )
{
  ESRT_MANAGEMENT_PROTOCOL  *EsrtManagement;
  EFI_PEI_HOB_POINTERS      HobPointer;
  EFI_CAPSULE_HEADER        *CapsuleHeader;
  BOOLEAN                   NeedReset;
  EFI_STATUS                Status;

  DEBUG ((DEBUG_INFO, "%a: processing capsules ...\n", __func__));

  Status = gBS->LocateProtocol (
                  &gEsrtManagementProtocolGuid,
                  NULL,
                  (VOID **)&EsrtManagement
                  );
  if (!EFI_ERROR (Status)) {
    EsrtManagement->SyncEsrtFmp ();
  }

  //
  // Find all capsule images from hob
  //
  HobPointer.Raw = GetHobList ();
  NeedReset      = FALSE;
  while ((HobPointer.Raw = GetNextHob (
                             EFI_HOB_TYPE_UEFI_CAPSULE,
                             HobPointer.Raw
                             )) != NULL)
  {
    CapsuleHeader = (VOID *)(UINTN)HobPointer.Capsule->BaseAddress;

    Status = ProcessCapsuleImage (CapsuleHeader);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: failed to process capsule %p - %r\n",
        __func__,
        CapsuleHeader,
        Status
        ));
      return;
    }

    NeedReset      = TRUE;
    HobPointer.Raw = GET_NEXT_HOB (HobPointer);
  }

  if (NeedReset) {
    DEBUG ((
      DEBUG_WARN,
      "%a: capsule update successful, resetting ...\n",
      __func__
      ));

    gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
    CpuDeadLoop ();
  }
}

#define VERSION_STRING_PREFIX  L"Tianocore/EDK2 firmware version "

/**
  This functions checks the value of BootDiscoverPolicy variable and
  connect devices of class specified by that variable. Then it refreshes
  Boot order for newly discovered boot device.

  @retval  EFI_SUCCESS  Devices connected successfully or connection
                        not required.
  @retval  others       Return values from GetVariable(), LocateProtocol()
                        and ConnectDeviceClass().
**/
STATIC
EFI_STATUS
BootDiscoveryPolicyHandler (
  VOID
  )
{
  EFI_STATUS                        Status;
  UINT32                            DiscoveryPolicy;
  UINT32                            DiscoveryPolicyOld;
  UINTN                             Size;
  EFI_BOOT_MANAGER_POLICY_PROTOCOL  *BMPolicy;
  EFI_GUID                          *Class;

  Size   = sizeof (DiscoveryPolicy);
  Status = gRT->GetVariable (
                  BOOT_DISCOVERY_POLICY_VAR,
                  &gBootDiscoveryPolicyMgrFormsetGuid,
                  NULL,
                  &Size,
                  &DiscoveryPolicy
                  );
  if (Status == EFI_NOT_FOUND) {
    DiscoveryPolicy = PcdGet32 (PcdBootDiscoveryPolicy);
    Status          = PcdSet32S (PcdBootDiscoveryPolicy, DiscoveryPolicy);
    if (Status == EFI_NOT_FOUND) {
      return EFI_SUCCESS;
    } else if (EFI_ERROR (Status)) {
      return Status;
    }
  } else if (EFI_ERROR (Status)) {
    return Status;
  }

  if (DiscoveryPolicy == BDP_CONNECT_MINIMAL) {
    return EFI_SUCCESS;
  }

  switch (DiscoveryPolicy) {
    case BDP_CONNECT_NET:
      Class = &gEfiBootManagerPolicyNetworkGuid;
      break;
    case BDP_CONNECT_ALL:
      Class = &gEfiBootManagerPolicyConnectAllGuid;
      break;
    default:
      DEBUG ((
        DEBUG_INFO,
        "%a - Unexpected DiscoveryPolicy (0x%x). Run Minimal Discovery Policy\n",
        __func__,
        DiscoveryPolicy
        ));
      return EFI_SUCCESS;
  }

  Status = gBS->LocateProtocol (
                  &gEfiBootManagerPolicyProtocolGuid,
                  NULL,
                  (VOID **)&BMPolicy
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "%a - Failed to locate gEfiBootManagerPolicyProtocolGuid."
      "Driver connect will be skipped.\n",
      __func__
      ));
    return Status;
  }

  Status = BMPolicy->ConnectDeviceClass (BMPolicy, Class);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a - ConnectDeviceClass returns - %r\n", __func__, Status));
    return Status;
  }

  //
  // Refresh Boot Options if Boot Discovery Policy has been changed
  //
  Size   = sizeof (DiscoveryPolicyOld);
  Status = gRT->GetVariable (
                  BOOT_DISCOVERY_POLICY_OLD_VAR,
                  &gBootDiscoveryPolicyMgrFormsetGuid,
                  NULL,
                  &Size,
                  &DiscoveryPolicyOld
                  );
  if ((Status == EFI_NOT_FOUND) || (DiscoveryPolicyOld != DiscoveryPolicy)) {
    EfiBootManagerRefreshAllBootOption ();

    Status = gRT->SetVariable (
                    BOOT_DISCOVERY_POLICY_OLD_VAR,
                    &gBootDiscoveryPolicyMgrFormsetGuid,
                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
                    sizeof (DiscoveryPolicyOld),
                    &DiscoveryPolicy
                    );
  }

  return EFI_SUCCESS;
}

/**
  Do the platform specific action after the console is ready
  Possible things that can be done in PlatformBootManagerAfterConsole:
  > Console post action:
    > Dynamically switch output mode from 100x31 to 80x25 for certain scenario
    > Signal console ready platform customized event
  > Run diagnostics like memory testing
  > Connect certain devices
  > Dispatch additional option roms
  > Special boot: e.g.: USB boot, enter UI
**/
VOID
EFIAPI
PlatformBootManagerAfterConsole (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL  *GraphicsOutput;
  UINTN                         FirmwareVerLength;
  UINTN                         PosX;
  UINTN                         PosY;
  EFI_INPUT_KEY                 Key;

  //
  // Initialize and process the QEMU TPM physical presence interface before
  // booting an OS. This fills the PPI function table consumed by the ACPI
  // MSFT0101 _DSM methods and executes any pending TPM requests.
  //
  Tcg2PhysicalPresenceLibProcessRequest (NULL);

  FirmwareVerLength = StrLen (PcdGetPtr (PcdFirmwareVersionString));

  //
  // Show the splash screen.
  //
  Status = BootLogoEnableLogo ();
  if (EFI_ERROR (Status)) {
    if (FirmwareVerLength > 0) {
      Print (
        VERSION_STRING_PREFIX L"%s\n",
        PcdGetPtr (PcdFirmwareVersionString)
        );
    }

    Print (L"Press ESCAPE for boot options ");
  } else if (FirmwareVerLength > 0) {
    Status = gBS->HandleProtocol (
                    gST->ConsoleOutHandle,
                    &gEfiGraphicsOutputProtocolGuid,
                    (VOID **)&GraphicsOutput
                    );
    if (!EFI_ERROR (Status)) {
      PosX = (GraphicsOutput->Mode->Info->HorizontalResolution -
              (StrLen (VERSION_STRING_PREFIX) + FirmwareVerLength) *
              EFI_GLYPH_WIDTH) / 2;
      PosY = 0;

      PrintXY (
        PosX,
        PosY,
        NULL,
        NULL,
        VERSION_STRING_PREFIX L"%s",
        PcdGetPtr (PcdFirmwareVersionString)
        );
    }
  }

  //
  // Connect device specified by BootDiscoverPolicy variable and
  // refresh Boot order for newly discovered boot devices
  //
  BootDiscoveryPolicyHandler ();

  //
  // On ARM, there is currently no reason to use the phased capsule
  // update approach where some capsules are dispatched before EndOfDxe
  // and some are dispatched after. So just handle all capsules here,
  // when the console is up and we can actually give the user some
  // feedback about what is going on.
  //
  HandleCapsules ();

  //
  // Register UEFI Shell
  //
  Key.ScanCode    = SCAN_NULL;
  Key.UnicodeChar = L's';
  PlatformRegisterFvBootOption (&gUefiShellFileGuid, L"UEFI Shell", 0, &Key);

  ScorpiApplyBootOrderParam ();
}

/**
  This function is called each second during the boot manager waits the
  timeout.

  @param TimeoutRemain  The remaining timeout.
**/
VOID
EFIAPI
PlatformBootManagerWaitCallback (
  UINT16  TimeoutRemain
  )
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  Black;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL_UNION  White;
  UINT16                               Timeout;
  EFI_STATUS                           Status;

  Timeout = PcdGet16 (PcdPlatformBootTimeOut);

  Black.Raw = 0x00000000;
  White.Raw = 0x00FFFFFF;

  Status = BootLogoUpdateProgress (
             White.Pixel,
             Black.Pixel,
             L"Press ESCAPE for boot options",
             White.Pixel,
             (Timeout - TimeoutRemain) * 100 / Timeout,
             0
             );
  if (EFI_ERROR (Status)) {
    Print (L".");
  }
}

/**
  The function is called when no boot option could be launched,
  including platform recovery options and options pointing to applications
  built into firmware volumes.

  If this function returns, BDS attempts to enter an infinite loop.
**/
VOID
EFIAPI
PlatformBootManagerUnableToBoot (
  VOID
  )
{
  EFI_STATUS                    Status;
  EFI_BOOT_MANAGER_LOAD_OPTION  BootManagerMenu;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions;
  UINTN                         OldBootOptionCount;
  UINTN                         NewBootOptionCount;

  //
  // Record the total number of boot configured boot options
  //
  BootOptions = EfiBootManagerGetLoadOptions (
                  &OldBootOptionCount,
                  LoadOptionTypeBoot
                  );
  EfiBootManagerFreeLoadOptions (BootOptions, OldBootOptionCount);

  //
  // Connect all devices, and regenerate all boot options
  //
  EfiBootManagerConnectAll ();
  EfiBootManagerRefreshAllBootOption ();

  //
  // Boot the 'UEFI Shell'. If the Pcd is not set, the UEFI Shell is not
  // an active boot option and must be manually selected through UiApp
  // (at least during the fist boot).
  //
  if (FixedPcdGetBool (PcdUefiShellDefaultBootEnable)) {
    PlatformBootFvBootOption (
      &gUefiShellFileGuid,
      L"UEFI Shell (default)"
      );
  }

  //
  // Record the updated number of boot configured boot options
  //
  BootOptions = EfiBootManagerGetLoadOptions (
                  &NewBootOptionCount,
                  LoadOptionTypeBoot
                  );
  EfiBootManagerFreeLoadOptions (BootOptions, NewBootOptionCount);

  //
  // If the number of configured boot options has changed, reboot
  // the system so the new boot options will be taken into account
  // while executing the ordinary BDS bootflow sequence.
  // *Unless* persistent varstore is being emulated, since we would
  // then end up in an endless reboot loop.
  //
  if (!PcdGetBool (PcdEmuVariableNvModeEnable)) {
    if (NewBootOptionCount != OldBootOptionCount) {
      DEBUG ((
        DEBUG_WARN,
        "%a: rebooting after refreshing all boot options\n",
        __func__
        ));
      gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
    }
  }

  Status = EfiBootManagerGetBootManagerMenu (&BootManagerMenu);
  if (EFI_ERROR (Status)) {
    return;
  }

  for ( ; ;) {
    EfiBootManagerBoot (&BootManagerMenu);
  }
}
