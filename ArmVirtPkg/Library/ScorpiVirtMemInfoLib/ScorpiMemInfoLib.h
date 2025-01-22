/** @file

  Copyright (c) 2022, Arm Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SCORPI_VIRT_MEM_INFO_LIB_H_
#define SCORPI_VIRT_MEM_INFO_LIB_H_

#define SCORPI_MAX_MEM_NODE_NUM  10

// Record memory node info (base address and size)
typedef struct {
  UINT64    Base;
  UINT64    Size;
} SCORPI_MEM_NODE_INFO;

// Number of Virtual Memory Map Descriptors
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS  (4 + SCORPI_MAX_MEM_NODE_NUM)

//
// Core peripherals such as the UART, the GIC and the RTC are
// all mapped in the 'miscellaneous device I/O' region, which we just map
// in its entirety rather than device by device. Note that it does not
// cover any of the NOR flash banks or PCI resource windows.
//
#define MACH_VIRT_PERIPH_BASE  0x10000
#define MACH_VIRT_PERIPH_SIZE  0x4000

#define SCORPI_GIC_BASE_ADDR  0x2F000000UL
#define SCORPI_GIC_SIZE       0xF10000UL

#endif // SCORPI_VIRT_MEM_INFO_LIB_H_
