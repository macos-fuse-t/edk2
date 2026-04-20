/** @file
  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2021 - 2022, ARM Ltd. All rights reserved.<BR>
    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

DefinitionBlock ("DsdtTable.aml", "DSDT", 2, "ARMLTD", "ARM-KVMT", 1) {
  Scope (_SB) {
    Device (TPM0) {
      Name (_HID, "MSFT0101")
      Name (TSTA, Buffer (One) { 0x00 })
      Method (_STA, 0, Serialized) {
        Return (DerefOf (Index (TSTA, Zero)))
      }

      Name (_UID, Zero)
      Name (_STR, Unicode ("TPM 2.0 Device"))
      Name (_CRS, ResourceTemplate () {
        Memory32Fixed (ReadWrite, 0x54504D30, 0x54504D31)
      })

      Name (TPM2, Package (2) { Zero, Zero })
      Name (TPM3, Package (3) { Zero, Zero, Zero })

      Method (TPFN, 1, Serialized) {
        If (LGreaterEqual (Arg0, 0x100)) {
          Return (Zero)
        }

        OperationRegion (TPP1, SystemMemory, Add (0x00025000, Arg0), One)
        Field (TPP1, ByteAcc, NoLock, Preserve) {
          TPPF, 8
        }

        Return (TPPF)
      }

      OperationRegion (TPP2, SystemMemory, 0x00025100, 0x5A)
      Field (TPP2, ByteAcc, NoLock, Preserve) {
        PPIN, 8,
        PPIP, 32,
        PPRP, 32,
        PPRQ, 32,
        PPRM, 32,
        LPPR, 32
      }

      OperationRegion (TPP3, SystemMemory, 0x0002515A, One)
      Field (TPP3, ByteAcc, NoLock, Preserve) {
        MOVV, 8
      }

      Method (_DSM, 4, Serialized) {
        If (LEqual (Arg0, ToUUID ("3DDDFAA6-361B-4EB4-A424-8D10089D1653"))) {
          If (LEqual (Arg2, Zero)) {
            Return (Buffer (0x02) {
              0xFF, 0x01
            })
          }

          If (LEqual (Arg2, One)) {
            Return ("1.3")
          }

          If (LEqual (Arg2, 0x02)) {
            Store (DerefOf (Index (Arg3, Zero)), Local0)
            Store (TPFN (Local0), Local1)
            If (LEqual (And (Local1, 0x07), Zero)) {
              Return (One)
            }

            Store (Local0, PPRQ)
            Store (Zero, PPRM)
            Return (Zero)
          }

          If (LEqual (Arg2, 0x03)) {
            If (LEqual (Arg1, One)) {
              Store (PPRQ, Index (TPM2, One))
              Return (TPM2)
            }

            If (LEqual (Arg1, 0x02)) {
              Store (PPRQ, Index (TPM3, One))
              Store (PPRM, Index (TPM3, 0x02))
              Return (TPM3)
            }
          }

          If (LEqual (Arg2, 0x04)) {
            Return (0x02)
          }

          If (LEqual (Arg2, 0x05)) {
            Store (LPPR, Index (TPM3, One))
            Store (PPRP, Index (TPM3, 0x02))
            Return (TPM3)
          }

          If (LEqual (Arg2, 0x06)) {
            Return (0x03)
          }

          If (LEqual (Arg2, 0x07)) {
            Store (DerefOf (Index (Arg3, Zero)), Local0)
            Store (TPFN (Local0), Local1)
            If (LEqual (And (Local1, 0x07), Zero)) {
              Return (One)
            }

            If (LEqual (And (Local1, 0x07), 0x02)) {
              Return (0x03)
            }

            If (LEqual (Arg1, One)) {
              Store (Local0, PPRQ)
              Store (Zero, PPRM)
            }

            If (LEqual (Arg1, 0x02)) {
              Store (Local0, PPRQ)
              Store (DerefOf (Index (Arg3, One)), PPRM)
            }

            Return (Zero)
          }

          If (LEqual (Arg2, 0x08)) {
            Store (DerefOf (Index (Arg3, Zero)), Local0)
            Store (TPFN (Local0), Local1)
            Return (And (Local1, 0x07))
          }

          Return (Buffer (One) {
            0x00
          })
        }

        If (LEqual (Arg0, ToUUID ("376054ED-CC13-4675-901C-4756D7F2D45D"))) {
          If (LEqual (Arg2, Zero)) {
            Return (Buffer (One) {
              0x03
            })
          }

          If (LEqual (Arg2, One)) {
            Store (DerefOf (Index (Arg3, Zero)), Local0)
            Store (Local0, MOVV)
            Return (Zero)
          }
        }

        Return (Buffer (One) {
          0x00
        })
      }
    }
  } // Scope (_SB)
}
