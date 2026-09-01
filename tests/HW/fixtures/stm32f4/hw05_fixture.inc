#include "SEGGER_RTT.h"
#include "rtt_log.h"

#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#define HW05_CAPTURE_SIZE 128u
#define HW05_FIXED_CASES 20u
#define HW05_RANDOM_CASES 256u
#define HW05_CORPUS_CASES (HW05_FIXED_CASES + HW05_RANDOM_CASES)
#define HW05_BOUNDARY_ITERATIONS 10000u
#define HW05_GUARD_BEFORE UINT32_C(0x51A7C0DE)
#define HW05_GUARD_AFTER  UINT32_C(0xA55A3CC3)

typedef struct {
  uint32_t GuardBefore;
  unsigned char Data[HW05_CAPTURE_SIZE];
  uint32_t GuardAfter;
  unsigned Length;
  unsigned WriteCalls;
  unsigned DirectWrites;
  unsigned FormatterCalls;
  unsigned FormatterWrites;
  unsigned MaxRequest;
  unsigned LastReturn;
  unsigned Overflow;
} HW05_Capture;

typedef struct {
  uint32_t Bits;
  unsigned Length;
  unsigned char Text[17];
  unsigned Passed;
} HW05_CorpusResult;

typedef struct {
  const char *Name;
  unsigned Length;
  unsigned WriteCalls;
  unsigned DirectWrites;
  unsigned FormatterCalls;
  unsigned FormatterWrites;
  unsigned MaxRequest;
  unsigned WriteReturn;
  uint32_t Hash;
  uint32_t Cycles;
  unsigned Passed;
  unsigned char Frame[65];
} HW05_BoundaryResult;

static HW05_Capture HW05_CurrentCapture;
static HW05_CorpusResult HW05_Corpus[HW05_CORPUS_CASES];
static HW05_BoundaryResult HW05_Boundaries[4];
static unsigned HW05_MeasurementActive;
static unsigned HW05_FormatterDepth;

uintptr_t __stack_chk_guard = (uintptr_t)UINT32_C(0x6D5A56A9);

unsigned __real_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes);

__attribute__((noreturn, no_stack_protector))
void __stack_chk_fail(void)
{
  static const char Failure[] = "HW-05|STACK_GUARD=FAIL\n";

  (void)__real_SEGGER_RTT_Write(0u, Failure, sizeof(Failure) - 1u);
  for (;;) {
    __NOP();
  }
}

unsigned __wrap_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes)
{
  if (HW05_MeasurementActive != 0u) {
    HW05_CurrentCapture.WriteCalls++;
    if (HW05_FormatterDepth != 0u) {
      HW05_CurrentCapture.FormatterWrites++;
    } else {
      HW05_CurrentCapture.DirectWrites++;
    }
    if (NumBytes > HW05_CurrentCapture.MaxRequest) {
      HW05_CurrentCapture.MaxRequest = NumBytes;
    }
    if ((BufferIndex != RTT_LOG_BUFFER_INDEX) ||
        (NumBytes > (HW05_CAPTURE_SIZE - HW05_CurrentCapture.Length))) {
      HW05_CurrentCapture.Overflow = 1u;
      return 0u;
    }
    memcpy(HW05_CurrentCapture.Data + HW05_CurrentCapture.Length,
           pBuffer, NumBytes);
    HW05_CurrentCapture.Length += NumBytes;
    HW05_CurrentCapture.LastReturn = NumBytes;
    return NumBytes;
  }
  return __real_SEGGER_RTT_Write(BufferIndex, pBuffer, NumBytes);
}

int __wrap_SEGGER_RTT_printf(unsigned BufferIndex, const char *sFormat, ...)
{
  va_list Parameters;
  int Result;

  if (HW05_MeasurementActive != 0u) {
    HW05_CurrentCapture.FormatterCalls++;
    HW05_FormatterDepth++;
  }
  va_start(Parameters, sFormat);
  Result = SEGGER_RTT_vprintf(BufferIndex, sFormat, &Parameters);
  va_end(Parameters);
  if (HW05_MeasurementActive != 0u) {
    HW05_FormatterDepth--;
  }
  return Result;
}

static float HW05_FromBits(uint32_t Bits)
{
  union {
    uint32_t Bits;
    float Value;
  } Number;

  Number.Bits = Bits;
  return Number.Value;
}

static void HW05_ResetCapture(void)
{
  memset(&HW05_CurrentCapture, 0, sizeof(HW05_CurrentCapture));
  HW05_CurrentCapture.GuardBefore = HW05_GUARD_BEFORE;
  HW05_CurrentCapture.GuardAfter = HW05_GUARD_AFTER;
  HW05_FormatterDepth = 0u;
}

static unsigned HW05_CaptureValid(void)
{
  return (HW05_CurrentCapture.GuardBefore == HW05_GUARD_BEFORE) &&
         (HW05_CurrentCapture.GuardAfter == HW05_GUARD_AFTER) &&
         (HW05_CurrentCapture.Overflow == 0u) &&
         (HW05_FormatterDepth == 0u);
}

static void HW05_CaptureLegacy(uint32_t Bits, const char *Label)
{
  HW05_ResetCapture();
  HW05_MeasurementActive = 1u;
  log_float_label(Label, HW05_FromBits(Bits));
  HW05_MeasurementActive = 0u;
}

static void HW05_CaptureTyped(uint32_t Bits, const char *Label)
{
  HW05_ResetCapture();
  HW05_MeasurementActive = 1u;
  log_f32(Label, HW05_FromBits(Bits));
  HW05_MeasurementActive = 0u;
}

static uint32_t HW05_Hash(const unsigned char *Data, unsigned Length)
{
  uint32_t Hash = UINT32_C(2166136261);
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    Hash ^= Data[Index];
    Hash *= UINT32_C(16777619);
  }
  return Hash;
}

static void HW05_ToHex(const unsigned char *Data, unsigned Length, char *Text)
{
  static const char Hex[] = "0123456789ABCDEF";
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    Text[Index * 2u] = Hex[Data[Index] >> 4];
    Text[Index * 2u + 1u] = Hex[Data[Index] & 0x0Fu];
  }
  Text[Length * 2u] = '\0';
}

static unsigned HW05_RunCorpusCase(unsigned Index, uint32_t Bits)
{
  unsigned char Legacy[HW05_CAPTURE_SIZE];
  unsigned LegacyLength;
  unsigned LegacyValid;
  unsigned Passed;

  HW05_CaptureLegacy(Bits, NULL);
  LegacyLength = HW05_CurrentCapture.Length;
  LegacyValid = HW05_CaptureValid();
  memcpy(Legacy, HW05_CurrentCapture.Data, LegacyLength);

  HW05_CaptureTyped(Bits, NULL);
  Passed = LegacyValid && HW05_CaptureValid() &&
           (LegacyLength == HW05_CurrentCapture.Length) &&
           (LegacyLength <= sizeof(HW05_Corpus[Index].Text)) &&
           (HW05_CurrentCapture.WriteCalls == 1u) &&
           (HW05_CurrentCapture.DirectWrites == 1u) &&
           (HW05_CurrentCapture.FormatterCalls == 0u) &&
           (HW05_CurrentCapture.LastReturn == LegacyLength) &&
           (memcmp(Legacy, HW05_CurrentCapture.Data, LegacyLength) == 0);

  HW05_Corpus[Index].Bits = Bits;
  HW05_Corpus[Index].Length = LegacyLength;
  HW05_Corpus[Index].Passed = Passed;
  if (LegacyLength <= sizeof(HW05_Corpus[Index].Text)) {
    memcpy(HW05_Corpus[Index].Text, Legacy, LegacyLength);
  }
  return Passed;
}

static unsigned HW05_RunCorpus(void)
{
  static const uint32_t Fixed[HW05_FIXED_CASES] = {
    UINT32_C(0x00000000), UINT32_C(0x80000000), UINT32_C(0x00000001),
    UINT32_C(0x80000001), UINT32_C(0x3A83126F), UINT32_C(0xBA83126F),
    UINT32_C(0x3F800000), UINT32_C(0xBF800000), UINT32_C(0x3F9E0419),
    UINT32_C(0xBF9E0419), UINT32_C(0x3FFFFFFF), UINT32_C(0x40000000),
    UINT32_C(0x4F7FFFFF), UINT32_C(0xCF7FFFFF), UINT32_C(0x4F800000),
    UINT32_C(0xCF800000), UINT32_C(0x7F800000), UINT32_C(0xFF800000),
    UINT32_C(0x7FC00000), UINT32_C(0xFFC00000)
  };
  uint32_t State = UINT32_C(0x0E5413B1);
  unsigned Index;
  unsigned Passed = 0u;

  for (Index = 0u; Index < HW05_FIXED_CASES; ++Index) {
    Passed += HW05_RunCorpusCase(Index, Fixed[Index]);
  }
  for (Index = HW05_FIXED_CASES; Index < HW05_CORPUS_CASES; ++Index) {
    State ^= State << 13;
    State ^= State >> 17;
    State ^= State << 5;
    Passed += HW05_RunCorpusCase(Index, State);
  }
  return Passed;
}

static void HW05_MakeLabels(char *Label46, char *Label47)
{
  memset(Label46, 'L', 46u);
  Label46[46] = '\0';
  memset(Label47, 'L', 47u);
  Label47[47] = '\0';
}

static unsigned HW05_BoundaryExpected(unsigned Kind,
                                      unsigned ExpectedLength,
                                      const unsigned char *Expected)
{
  unsigned PathValid;

  if ((Kind >= 2u) || ((HW05_FAST != 0) && (Kind == 0u))) {
    PathValid = (HW05_CurrentCapture.DirectWrites == 1u) &&
                (HW05_CurrentCapture.FormatterCalls == 0u) &&
                (HW05_CurrentCapture.FormatterWrites == 0u);
  } else {
    /*
     * SEGGER_RTT_printf wrapping is optional.  Without it, its final Write is
     * indistinguishable from a direct Write; accept either observation.
     */
    PathValid = ((HW05_CurrentCapture.DirectWrites == 1u) &&
                 (HW05_CurrentCapture.FormatterCalls == 0u) &&
                 (HW05_CurrentCapture.FormatterWrites == 0u)) ||
                ((HW05_CurrentCapture.DirectWrites == 0u) &&
                 (HW05_CurrentCapture.FormatterCalls == 1u) &&
                 (HW05_CurrentCapture.FormatterWrites == 1u));
  }
  return HW05_CaptureValid() &&
         (HW05_CurrentCapture.Length == ExpectedLength) &&
         (HW05_CurrentCapture.WriteCalls == 1u) &&
         PathValid &&
         (HW05_CurrentCapture.MaxRequest == ExpectedLength) &&
         (HW05_CurrentCapture.LastReturn == ExpectedLength) &&
         (memcmp(HW05_CurrentCapture.Data, Expected, ExpectedLength) == 0);
}

static unsigned HW05_RunBoundary(unsigned Kind,
                                 const char *Label,
                                 const unsigned char *Expected,
                                 unsigned ExpectedLength)
{
  HW05_BoundaryResult *Result = &HW05_Boundaries[Kind];
  uint32_t Start;
  unsigned Iteration;
  unsigned Passed = 1u;

  for (Iteration = 1u; Iteration <= HW05_BOUNDARY_ITERATIONS; ++Iteration) {
    HW05_ResetCapture();
    HW05_MeasurementActive = 1u;
    Start = DWT->CYCCNT;
    if (Kind < 2u) {
      log_float_label(Label, HW05_FromBits(UINT32_C(0xCF7FFFFF)));
    } else {
      log_f32(Label, HW05_FromBits(UINT32_C(0xCF7FFFFF)));
    }
    Result->Cycles += DWT->CYCCNT - Start;
    HW05_MeasurementActive = 0u;
    if (!HW05_BoundaryExpected(Kind, ExpectedLength, Expected)) {
      Passed = 0u;
    }
    if ((Iteration % 2500u) == 0u) {
      (void)SEGGER_RTT_printf(0u, "HW-05|HEARTBEAT|%s|ITER=%u|%s\n",
                              Result->Name, Iteration,
                              Passed ? "PASS" : "FAIL");
    }
  }

  Result->Length = HW05_CurrentCapture.Length;
  Result->WriteCalls = HW05_CurrentCapture.WriteCalls;
  Result->DirectWrites = HW05_CurrentCapture.DirectWrites;
  Result->FormatterCalls = HW05_CurrentCapture.FormatterCalls;
  Result->FormatterWrites = HW05_CurrentCapture.FormatterWrites;
  Result->MaxRequest = HW05_CurrentCapture.MaxRequest;
  Result->WriteReturn = HW05_CurrentCapture.LastReturn;
  Result->Hash = HW05_Hash(HW05_CurrentCapture.Data,
                           HW05_CurrentCapture.Length);
  Result->Passed = Passed;
  memcpy(Result->Frame, HW05_CurrentCapture.Data,
         HW05_CurrentCapture.Length);
  return Passed;
}

static unsigned HW05_RunBoundaries(void)
{
  static const unsigned char Number[] = "-4294967040.000\n";
  unsigned char Expected46[64];
  unsigned char Expected47[65];
  char Label46[47];
  char Label47[48];
  unsigned Passed = 0u;

  HW05_MakeLabels(Label46, Label47);
  memset(Expected46, 'L', 46u);
  Expected46[46] = ':';
  Expected46[47] = ' ';
  memcpy(Expected46 + 48u, Number, sizeof(Number) - 1u);
  memset(Expected47, 'L', 47u);
  Expected47[47] = ':';
  Expected47[48] = ' ';
  memcpy(Expected47 + 49u, Number, sizeof(Number) - 1u);

  HW05_Boundaries[0].Name = "LEGACY46";
  HW05_Boundaries[1].Name = "LEGACY47";
  HW05_Boundaries[2].Name = "TYPED46";
  HW05_Boundaries[3].Name = "TYPED47";
  Passed += HW05_RunBoundary(0u, Label46, Expected46, sizeof(Expected46));
  Passed += HW05_RunBoundary(1u, Label47, Expected47, sizeof(Expected47));
  Passed += HW05_RunBoundary(2u, Label46, Expected46, sizeof(Expected46));
  Passed += HW05_RunBoundary(3u, Label47, Expected46, sizeof(Expected46));
  return Passed;
}

static void HW05_EnableCycleCounter(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void HW05_Report(unsigned CorpusPassed, unsigned BoundaryPassed,
                        uint32_t ResetFlags)
{
  char Hex[HW05_CAPTURE_SIZE * 2u + 1u];
  unsigned Index;

  (void)ResetFlags;
  for (Index = 0u; Index < HW05_CORPUS_CASES; ++Index) {
    HW05_ToHex(HW05_Corpus[Index].Text, HW05_Corpus[Index].Length, Hex);
    (void)SEGGER_RTT_printf(0u,
      "HW-05|CORPUS|%03u|BITS=%08X|LEN=%u|HEX=%s|%s\n",
      Index, (unsigned)HW05_Corpus[Index].Bits,
      HW05_Corpus[Index].Length, Hex,
      HW05_Corpus[Index].Passed ? "PASS" : "FAIL");
  }
  for (Index = 0u; Index < 4u; ++Index) {
    const HW05_BoundaryResult *Result = &HW05_Boundaries[Index];
    HW05_ToHex(Result->Frame, Result->Length, Hex);
    (void)SEGGER_RTT_printf(0u,
      "HW-05|BOUNDARY|%s|ITER=10000|LEN=%u|WRITE=%u|DIRECT=%u|FMT_CALL=%u|FMT_WRITE=%u|MAX_REQ=%u|WRITE_RET=%u|HASH=%08X|CYCLES=%u|FRAME=%s|%s\n",
      Result->Name, Result->Length, Result->WriteCalls,
      Result->DirectWrites, Result->FormatterCalls,
      Result->FormatterWrites, Result->MaxRequest,
      Result->WriteReturn, (unsigned)Result->Hash,
      (unsigned)Result->Cycles, Hex,
      Result->Passed ? "PASS" : "FAIL");
  }
  (void)SEGGER_RTT_printf(0u,
    "HW-05|END|CORPUS=%u/%u|BOUNDARY=%u/4|ITERATIONS=40000|STACK=PASS|RESULT=%s\n",
    CorpusPassed, (unsigned)HW05_CORPUS_CASES, BoundaryPassed,
    ((CorpusPassed == HW05_CORPUS_CASES) && (BoundaryPassed == 4u)) ?
      "PASS" : "FAIL");
}

static void HW05_Run(void)
{
  uint32_t ResetFlags;
  unsigned CorpusPassed;
  unsigned BoundaryPassed;

  ResetFlags = RCC->CSR;
  (void)SEGGER_RTT_printf(0u,
    "HW-05|BEGIN|STM32F411CE|%s|%s|MODFF=%u|FAST=%u|SHA=" HW_TEST_LIBRARY_SHA "\n",
    HW05_PROJECT_NAME, HW05_BUILD_TYPE,
    (unsigned)HW05_MODFF, (unsigned)HW05_FAST);
  (void)SEGGER_RTT_printf(0u, "HW-05|RESET_FLAGS=0x%08X\n",
                          (unsigned)ResetFlags);
  HW05_EnableCycleCounter();
  CorpusPassed = HW05_RunCorpus();
  BoundaryPassed = HW05_RunBoundaries();
  HW05_Report(CorpusPassed, BoundaryPassed, ResetFlags);
}
