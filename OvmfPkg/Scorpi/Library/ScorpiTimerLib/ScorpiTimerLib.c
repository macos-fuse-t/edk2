/** @file
  Scorpi x64 local APIC timer library.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Register/Intel/ArchitecturalMsr.h>

#define SCORPI_APIC_BASE_DEFAULT  0xFEE00000ULL
#define SCORPI_APIC_BUS_HZ        1000000000ULL
#define SCORPI_APIC_DIVISOR       16ULL
#define SCORPI_APIC_TIMER_HZ      (SCORPI_APIC_BUS_HZ / SCORPI_APIC_DIVISOR)
#define SCORPI_APIC_TIMER_INIT    MAX_UINT32
#define SCORPI_APIC_DELAY_MAX     (SCORPI_APIC_TIMER_INIT / 2)

#define APIC_SVR        0x0F0
#define APIC_LVT_TIMER  0x320
#define APIC_TMICT      0x380
#define APIC_TMCCT      0x390
#define APIC_TDCR       0x3E0

#define APIC_SVR_ENABLE       BIT8
#define APIC_LVT_MASKED       BIT16
#define APIC_LVT_PERIODIC     BIT17
#define APIC_TDCR_DIVIDE_16   0x3
#define APIC_SPURIOUS_VECTOR  0xFF
#define APIC_TIMER_VECTOR     0xEF

STATIC UINTN
ScorpiApicBase (
  VOID
  )
{
  MSR_IA32_APIC_BASE_REGISTER  ApicBase;
  UINTN                        Base;

  ApicBase.Uint64  = AsmReadMsr64 (MSR_IA32_APIC_BASE);
  ApicBase.Bits.EN = 1;
  ApicBase.Bits.EXTD = 0;

  Base = (UINTN)(ApicBase.Uint64 & 0xFFFFFF000ULL);
  if (Base == 0) {
    Base = SCORPI_APIC_BASE_DEFAULT;
    ApicBase.Uint64 = (ApicBase.Uint64 & ~0xFFFFFF000ULL) | Base;
  }

  AsmWriteMsr64 (MSR_IA32_APIC_BASE, ApicBase.Uint64);
  return Base;
}

STATIC
UINTN
ScorpiApicInit (
  VOID
  )
{
  UINTN   Base;
  UINT32  Svr;

  Base = ScorpiApicBase ();

  Svr = MmioRead32 (Base + APIC_SVR);
  MmioWrite32 (Base + APIC_SVR, (Svr | APIC_SVR_ENABLE) | APIC_SPURIOUS_VECTOR);

  MmioWrite32 (Base + APIC_TDCR, APIC_TDCR_DIVIDE_16);
  MmioWrite32 (
    Base + APIC_LVT_TIMER,
    APIC_LVT_MASKED | APIC_LVT_PERIODIC | APIC_TIMER_VECTOR
    );

  if (MmioRead32 (Base + APIC_TMICT) != SCORPI_APIC_TIMER_INIT) {
    MmioWrite32 (Base + APIC_TMICT, SCORPI_APIC_TIMER_INIT);
  }

  return Base;
}

STATIC
UINT32
ScorpiApicCounter (
  IN UINTN  Base
  )
{
  return MmioRead32 (Base + APIC_TMCCT);
}

STATIC
UINT32
ScorpiApicElapsed (
  IN UINT32  Start,
  IN UINT32  Current
  )
{
  return Start - Current;
}

STATIC
VOID
ScorpiApicDelay (
  IN UINT64  Ticks
  )
{
  UINTN   Base;
  UINT32  Chunk;
  UINT32  Start;

  Base = ScorpiApicInit ();
  while (Ticks > 0) {
    Chunk = (Ticks > SCORPI_APIC_DELAY_MAX) ?
            SCORPI_APIC_DELAY_MAX :
            (UINT32)Ticks;
    Start = ScorpiApicCounter (Base);

    while (ScorpiApicElapsed (Start, ScorpiApicCounter (Base)) < Chunk) {
      CpuPause ();
    }

    Ticks -= Chunk;
  }
}

UINTN
EFIAPI
MicroSecondDelay (
  IN UINTN  MicroSeconds
  )
{
  ScorpiApicDelay (
    DivU64x32 (MultU64x64 (MicroSeconds, SCORPI_APIC_TIMER_HZ), 1000000)
    );

  return MicroSeconds;
}

UINTN
EFIAPI
NanoSecondDelay (
  IN UINTN  NanoSeconds
  )
{
  ScorpiApicDelay (
    DivU64x32 (MultU64x64 (NanoSeconds, SCORPI_APIC_TIMER_HZ), 1000000000)
    );

  return NanoSeconds;
}

UINT64
EFIAPI
GetPerformanceCounter (
  VOID
  )
{
  return ScorpiApicCounter (ScorpiApicInit ());
}

UINT64
EFIAPI
GetPerformanceCounterProperties (
  OUT UINT64  *StartValue  OPTIONAL,
  OUT UINT64  *EndValue    OPTIONAL
  )
{
  ScorpiApicInit ();

  if (StartValue != NULL) {
    *StartValue = SCORPI_APIC_TIMER_INIT;
  }

  if (EndValue != NULL) {
    *EndValue = 0;
  }

  return SCORPI_APIC_TIMER_HZ;
}

UINT64
EFIAPI
GetTimeInNanoSecond (
  IN UINT64  Ticks
  )
{
  UINT64  NanoSeconds;
  UINT64  Remainder;
  INTN    Shift;

  NanoSeconds = MultU64x32 (
                  DivU64x64Remainder (
                    Ticks,
                    SCORPI_APIC_TIMER_HZ,
                    &Remainder
                    ),
                  1000000000
                  );

  Shift = MAX (0, HighBitSet64 (Remainder) - 33);
  Remainder = RShiftU64 (Remainder, (UINTN)Shift);

  return NanoSeconds +
         DivU64x64Remainder (
           MultU64x32 (Remainder, 1000000000),
           RShiftU64 (SCORPI_APIC_TIMER_HZ, (UINTN)Shift),
           NULL
           );
}
