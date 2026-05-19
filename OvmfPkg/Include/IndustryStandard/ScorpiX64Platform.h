/** @file
  Scorpi X64 fixed platform addresses.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SCORPI_X64_PLATFORM_H_
#define SCORPI_X64_PLATFORM_H_

#define SCORPI_X64_RESET_BASE            0xF0000000ULL
#define SCORPI_X64_RESET_SIZE            0x1000
#define SCORPI_X64_RESET_OFFSET          0
#define SCORPI_X64_SHUTDOWN_OFFSET       4
#define SCORPI_X64_RESET_VALUE           1
#define SCORPI_X64_SHUTDOWN_VALUE        1
#define SCORPI_X64_ACPI_GED_BASE         0xF0001000ULL
#define SCORPI_X64_ACPI_GED_SIZE         0x1000
#define SCORPI_X64_ACPI_GED_IRQ          10
#define SCORPI_X64_ACPI_GED_PWR_DOWN_EVT 0x2

#endif
