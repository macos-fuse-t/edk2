/** @file
  Scorpi x64 TSC timer library.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/TimerLib.h>
#include <Register/Intel/Cpuid.h>

#define SCORPI_FALLBACK_TSC_HZ  1000000000ULL
#define SCORPI_DEFAULT_XTAL_HZ  24000000ULL

STATIC UINT64  mTscHz;

STATIC
UINT64
ScorpiTscHz (
  VOID
  )
{
  UINT32  MaxLeaf;
  UINT32  Eax;
  UINT32  Ebx;
  UINT32  Ecx;
  UINT64  Hz;

  if (mTscHz != 0) {
    return mTscHz;
  }

  AsmCpuid (0, &MaxLeaf, NULL, NULL, NULL);

  if (MaxLeaf >= CPUID_TIME_STAMP_COUNTER) {
    AsmCpuid (CPUID_TIME_STAMP_COUNTER, &Eax, &Ebx, &Ecx, NULL);
    if ((Eax != 0) && (Ebx != 0)) {
      Hz = MultU64x32 ((Ecx == 0) ? SCORPI_DEFAULT_XTAL_HZ : Ecx, Ebx);
      Hz = DivU64x32 (Hz + (Eax >> 1), Eax);
      if (Hz != 0) {
        mTscHz = Hz;
        return mTscHz;
      }
    }
  }

  if (MaxLeaf >= CPUID_PROCESSOR_FREQUENCY) {
    AsmCpuid (CPUID_PROCESSOR_FREQUENCY, &Eax, NULL, NULL, NULL);
    if (Eax != 0) {
      mTscHz = MultU64x32 (Eax, 1000000);
      return mTscHz;
    }
  }

  mTscHz = SCORPI_FALLBACK_TSC_HZ;
  return mTscHz;
}

STATIC
VOID
ScorpiDelay (
  IN UINT64  Ticks
  )
{
  UINT64  End;

  End = AsmReadTsc () + Ticks;
  while (AsmReadTsc () < End) {
    CpuPause ();
  }
}

UINTN
EFIAPI
MicroSecondDelay (
  IN UINTN  MicroSeconds
  )
{
  ScorpiDelay (
    DivU64x32 (MultU64x64 (MicroSeconds, ScorpiTscHz ()), 1000000)
    );

  return MicroSeconds;
}

UINTN
EFIAPI
NanoSecondDelay (
  IN UINTN  NanoSeconds
  )
{
  ScorpiDelay (
    DivU64x32 (MultU64x64 (NanoSeconds, ScorpiTscHz ()), 1000000000)
    );

  return NanoSeconds;
}

UINT64
EFIAPI
GetPerformanceCounter (
  VOID
  )
{
  return AsmReadTsc ();
}

UINT64
EFIAPI
GetPerformanceCounterProperties (
  OUT UINT64  *StartValue  OPTIONAL,
  OUT UINT64  *EndValue    OPTIONAL
  )
{
  if (StartValue != NULL) {
    *StartValue = 0;
  }

  if (EndValue != NULL) {
    *EndValue = MAX_UINT64;
  }

  return ScorpiTscHz ();
}

UINT64
EFIAPI
GetTimeInNanoSecond (
  IN UINT64  Ticks
  )
{
  UINT64  Frequency;
  UINT64  NanoSeconds;
  UINT64  Remainder;
  INTN    Shift;

  Frequency = ScorpiTscHz ();
  NanoSeconds = MultU64x32 (
                  DivU64x64Remainder (Ticks, Frequency, &Remainder),
                  1000000000
                  );

  Shift = MAX (0, HighBitSet64 (Remainder) - 33);
  Remainder = RShiftU64 (Remainder, (UINTN)Shift);
  Frequency = RShiftU64 (Frequency, (UINTN)Shift);

  return NanoSeconds +
         DivU64x64Remainder (
           MultU64x32 (Remainder, 1000000000),
           Frequency,
           NULL
           );
}
