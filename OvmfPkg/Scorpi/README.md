# Scorpi X64 Firmware

Build the x64 firmware with stable debug and release artifact paths:

```sh
OvmfPkg/Scorpi/build-x64.sh debug
OvmfPkg/Scorpi/build-x64.sh release
OvmfPkg/Scorpi/build-x64.sh all
```

The script copies the images to stable paths:

```text
Build/ScorpiX64/Artifacts/debug/SCORPI_EFI.fd
Build/ScorpiX64/Artifacts/debug/SCORPI_VARS.fd
Build/ScorpiX64/Artifacts/release/SCORPI_EFI.fd
Build/ScorpiX64/Artifacts/release/SCORPI_VARS.fd
```

The debug flavor is the normal EDK2 `DEBUG` target. It emits OVMF debug prints
through the standard OVMF debug I/O port. The release flavor is the normal EDK2
`RELEASE` target.

Point VM YAML files at one of the artifact paths above. Switching between debug
and release should only require changing the `bootrom` path, not rebuilding
firmware.
