/** @file
  Scorpi boot parameter handling for the x64 platform boot manager.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "BdsPlatform.h"

#define SCORPI_BOOT_ID_MAX    64
#define SCORPI_BOOT_TYPE_MAX  32

STATIC CHAR16  mScorpiBootFileDesc[] = L"Scorpi boot file";

STATIC CONST CHAR8  *mScorpiDefaultBootFiles[] = {
  "/EFI/BOOT/BOOTX64.EFI",
  "/EFI/BOOT/bootx64.efi",
  "/boot/loader.efi",
  NULL
};

typedef struct {
  CHAR8      Id[SCORPI_BOOT_ID_MAX];
  CHAR8      Type[SCORPI_BOOT_TYPE_MAX];
  UINT8      Bus;
  UINT8      Slot;
  UINT8      Func;
  UINT16     Port;
  BOOLEAN    HasPort;
} SCORPI_BOOT_DEVICE;

RETURN_STATUS
EFIAPI
QemuFwCfgInitialize (
  VOID
  );

STATIC
BOOLEAN
AsciiEqualsCi (
  IN CONST CHAR8  *Left,
  IN CONST CHAR8  *Right
  )
{
  while ((*Left != '\0') && (*Right != '\0')) {
    if (AsciiCharToUpper (*Left) != AsciiCharToUpper (*Right)) {
      return FALSE;
    }

    Left++;
    Right++;
  }

  return *Left == *Right;
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
  EFI_STATUS            Status;
  UINTN                 FwCfgSize;
  CHAR8                 *Buffer;

  if (!QemuFwCfgIsAvailable ()) {
    QemuFwCfgInitialize ();
    if (!QemuFwCfgIsAvailable ()) {
      return EFI_NOT_FOUND;
    }
  }

  ReturnStatus = QemuFwCfgFindFile (FileName, &FwCfgItem, &FwCfgSize);
  if (RETURN_ERROR (ReturnStatus)) {
    return EFI_NOT_FOUND;
  }

  Buffer = AllocateZeroPool (FwCfgSize + 1);
  if (Buffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  QemuFwCfgSelectItem (FwCfgItem);
  QemuFwCfgReadBytes (FwCfgSize, Buffer);
  Buffer[FwCfgSize] = '\0';

  Status = EFI_SUCCESS;
  if ((FwCfgSize > 0) && (Buffer[FwCfgSize - 1] != '\0')) {
    Status = EFI_PROTOCOL_ERROR;
  }

  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  *Value = Buffer;
  return EFI_SUCCESS;
}

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
    if ((Value[Index] == '\0') || (Token[Index] != Value[Index])) {
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
  IN  CONST CHAR8         *Line,
  IN  UINTN               LineLen,
  OUT SCORPI_BOOT_DEVICE  *Device
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
  IN  CONST CHAR8         *Id,
  OUT SCORPI_BOOT_DEVICE  *Device
  )
{
  CHAR8        *Map;
  CONST CHAR8  *Line;
  CONST CHAR8  *LineEnd;
  EFI_STATUS   Status;

  if (Id[0] == '@') {
    Id++;
  }

  Status = ScorpiGetFwCfgString ("opt/scorpi/boot-map", &Map);
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
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_STATUS                Status;

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
    Result[Index] = (Path[Index] == '/') ? L'\\' : (CHAR16)Path[Index];
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
  IN  CONST CHAR8                   *Spec,
  IN  CONST CHAR8                   *BootFile OPTIONAL,
  IN  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOptions,
  IN  UINTN                         BootOptionCount,
  OUT UINT16                        *OptionNumber
  )
{
  SCORPI_BOOT_DEVICE  Device;
  UINTN               Index;

  if (!ScorpiFindBootDevice (Spec, &Device)) {
    return FALSE;
  }

  if ((BootFile != NULL) && (BootFile[0] != '\0')) {
    if (ScorpiResolveBootFileSpec (&Device, BootFile, OptionNumber)) {
      return TRUE;
    }
  } else {
    for (Index = 0; mScorpiDefaultBootFiles[Index] != NULL; Index++) {
      if (ScorpiResolveBootFileSpec (
            &Device,
            mScorpiDefaultBootFiles[Index],
            OptionNumber
            ))
      {
        return TRUE;
      }
    }
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
BOOLEAN
ScorpiAppendBootOrder (
  IN OUT UINT16  **BootOrder,
  IN OUT UINTN   *BootOrderCapacity,
  IN OUT UINTN   *BootOrderCount,
  IN     UINT16  OptionNumber
  )
{
  UINT16  *NewBootOrder;
  UINTN   NewCapacity;

  if (ScorpiBootOrderContains (*BootOrder, *BootOrderCount, OptionNumber)) {
    return TRUE;
  }

  if (*BootOrderCount == *BootOrderCapacity) {
    NewCapacity  = (*BootOrderCapacity == 0) ? 8 : *BootOrderCapacity * 2;
    NewBootOrder = ReallocatePool (
                     *BootOrderCapacity * sizeof (**BootOrder),
                     NewCapacity * sizeof (**BootOrder),
                     *BootOrder
                     );
    if (NewBootOrder == NULL) {
      return FALSE;
    }

    *BootOrder         = NewBootOrder;
    *BootOrderCapacity = NewCapacity;
  }

  (*BootOrder)[(*BootOrderCount)++] = OptionNumber;
  return TRUE;
}

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
  UINTN                         BootOrderCapacity;
  UINTN                         BootOrderCount;
  UINTN                         Index;
  CHAR8                         Spec[SCORPI_BOOT_ID_MAX];

  Status = ScorpiGetFwCfgString ("opt/scorpi/boot-order", &Value);
  if (EFI_ERROR (Status)) {
    return;
  }

  Status = ScorpiGetFwCfgString ("opt/scorpi/boot-file", &BootFile);
  if (EFI_ERROR (Status)) {
    BootFile = NULL;
  }

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);
  BootOrderCapacity = BootOptionCount + 1;
  BootOrder         = AllocateZeroPool (BootOrderCapacity * sizeof (*BootOrder));
  if (BootOrder == NULL) {
    if (BootFile != NULL) {
      FreePool (BootFile);
    }

    FreePool (Value);
    EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
    return;
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
      DEBUG ((DEBUG_WARN, "%a: no boot option matched '%a'\n", __func__, Spec));
    } else {
      ScorpiAppendBootOrder (
        &BootOrder,
        &BootOrderCapacity,
        &BootOrderCount,
        OptionNumber
        );
    }

    Cursor = (*SpecEnd == ':') ? SpecEnd + 1 : SpecEnd;
  }

  for (Index = 0; Index < BootOptionCount; Index++) {
    OptionNumber = (UINT16)BootOptions[Index].OptionNumber;
    ScorpiAppendBootOrder (
      &BootOrder,
      &BootOrderCapacity,
      &BootOrderCount,
      OptionNumber
      );
  }

  if (BootOrderCount != 0) {
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
  }

  if (BootFile != NULL) {
    FreePool (BootFile);
  }

  FreePool (Value);
  FreePool (BootOrder);
  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}
