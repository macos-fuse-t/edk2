/** @file
  Scorpi hardware-info access helpers.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SCORPI_HWINFO_LIB_H__
#define __SCORPI_HWINFO_LIB_H__

#include <Base.h>
#include <IndustryStandard/ScorpiX64HwInfo.h>

typedef struct {
  CONST VOID    *Blob;
  UINTN         Size;
} SCORPI_HWINFO;

/**
  Read and validate the Scorpi hardware-info fw_cfg file.

  The caller owns the returned buffer and must release it with
  ScorpiHwInfoRelease().

  @param[out] HwInfo  Hardware-info blob descriptor.

  @retval RETURN_SUCCESS            The blob was read and validated.
  @retval RETURN_INVALID_PARAMETER  HwInfo is NULL.
  @retval RETURN_NOT_FOUND          The fw_cfg file is not present.
  @retval RETURN_UNSUPPORTED        fw_cfg is not available, or the ABI is unknown.
  @retval RETURN_COMPROMISED_DATA   The blob is malformed.
  @retval RETURN_OUT_OF_RESOURCES   Memory allocation failed.
**/
RETURN_STATUS
EFIAPI
ScorpiHwInfoRead (
  OUT SCORPI_HWINFO  *HwInfo
  );

/**
  Release a hardware-info blob returned by ScorpiHwInfoRead().

  @param[in,out] HwInfo  Hardware-info blob descriptor.
**/
VOID
EFIAPI
ScorpiHwInfoRelease (
  IN OUT SCORPI_HWINFO  *HwInfo
  );

/**
  Validate a Scorpi hardware-info blob.

  @param[in] Blob  Blob base address.
  @param[in] Size  Blob size in bytes.

  @retval RETURN_SUCCESS            The blob is valid.
  @retval RETURN_INVALID_PARAMETER  Blob is NULL or Size is zero.
  @retval RETURN_UNSUPPORTED        The ABI is unknown.
  @retval RETURN_COMPROMISED_DATA   The blob is malformed.
**/
RETURN_STATUS
EFIAPI
ScorpiHwInfoValidate (
  IN CONST VOID  *Blob,
  IN UINTN       Size
  );

/**
  Return the validated blob header.

  @param[in] HwInfo  Hardware-info blob descriptor.

  @return Header pointer, or NULL if HwInfo is invalid.
**/
CONST SCORPI_X64_HWINFO_HEADER *
EFIAPI
ScorpiHwInfoHeader (
  IN CONST SCORPI_HWINFO  *HwInfo
  );

/**
  Return the first entry in a validated blob.

  @param[in] HwInfo  Hardware-info blob descriptor.

  @return Entry pointer, or NULL if the blob has no entries.
**/
CONST SCORPI_X64_HWINFO_ENTRY *
EFIAPI
ScorpiHwInfoFirst (
  IN CONST SCORPI_HWINFO  *HwInfo
  );

/**
  Return the entry that follows Entry in a validated blob.

  @param[in] HwInfo  Hardware-info blob descriptor.
  @param[in] Entry   Current entry.

  @return Next entry pointer, or NULL at the end of the blob.
**/
CONST SCORPI_X64_HWINFO_ENTRY *
EFIAPI
ScorpiHwInfoNext (
  IN CONST SCORPI_HWINFO            *HwInfo,
  IN CONST SCORPI_X64_HWINFO_ENTRY  *Entry
  );

/**
  Find the next entry with the requested type.

  Pass NULL for Previous to start at the first entry.

  @param[in] HwInfo    Hardware-info blob descriptor.
  @param[in] Type      Scorpi hardware-info entry type.
  @param[in] Previous  Previous matching entry, or NULL.

  @return Matching entry pointer, or NULL if no more entries match.
**/
CONST SCORPI_X64_HWINFO_ENTRY *
EFIAPI
ScorpiHwInfoFind (
  IN CONST SCORPI_HWINFO            *HwInfo,
  IN UINT16                         Type,
  IN CONST SCORPI_X64_HWINFO_ENTRY  *Previous  OPTIONAL
  );

#endif // __SCORPI_HWINFO_LIB_H__
