/** @file
  Scorpi X64 DSDT.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

DefinitionBlock ("Dsdt.aml", "DSDT", 2, "SCORPI", "SCORPIX", 0x00000001)
{
    Name (_S5, Package (0x02)
    {
        0x05,
        Zero
    })

    Scope (_SB)
    {
    }
}
