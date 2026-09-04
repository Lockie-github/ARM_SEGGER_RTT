#include "SEGGER_RTT.h"
#include "rtt_log.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define HW08_SAMPLE_COUNT       1024u
#define HW08_WARMUP_COUNT       256u
#define HW08_TEST_BUFFER_SIZE   256u
#define HW08_THROUGHPUT_COUNT   2048u
#define HW08_THROUGHPUT_SIZE    64u
#define HW08_STACK_PATTERN      UINT32_C(0xA55A5AA5)
#define HW08_STACK_MARGIN       128u
#define HW08_GUARD_BEFORE       UINT32_C(0x4808BEEF)
#define HW08_GUARD_AFTER        UINT32_C(0xA55A0808)

#if RTT_LOG_FLOAT_FAST_PATH
#define HW08_FLOAT_NAME "FAST"
#else
#define HW08_FLOAT_NAME "COMPAT"
#endif

#if RTT_WRITE_SKIP_USE_ASM
#define HW08_SKIP_NAME "ASM"
#else
#define HW08_SKIP_NAME "C"
#endif

typedef void (*HW08_Call)(void);

static char HW08_TestBuffer[HW08_TEST_BUFFER_SIZE];
static uint32_t HW08_Samples[HW08_SAMPLE_COUNT];
static volatile uint32_t HW08_GuardBefore = HW08_GUARD_BEFORE;
static volatile uint32_t HW08_GuardAfter = HW08_GUARD_AFTER;
static volatile unsigned HW08_Measuring;
static volatile unsigned HW08_WriteCalls;
static volatile unsigned HW08_WriteBytes;
static uintptr_t HW08_StackPaintEnd;
static uint32_t HW08_ResetFlags;

extern uint32_t _sstack;
extern uint32_t _estack;

uintptr_t __stack_chk_guard = (uintptr_t)UINT32_C(0x6D5A56A9);

unsigned __real_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes);

__attribute__((noreturn, no_stack_protector))
void __stack_chk_fail(void)
{
  static const char Failure[] = "HW-08|STACK_GUARD=FAIL\n";

  (void)__real_SEGGER_RTT_Write(0u, Failure, sizeof(Failure) - 1u);
  for (;;) {
    __NOP();
  }
}

unsigned __wrap_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes)
{
  if ((HW08_Measuring != 0u) && (BufferIndex == RTT_LOG_BUFFER_INDEX)) {
    HW08_WriteCalls++;
    HW08_WriteBytes += NumBytes;
  }
  return __real_SEGGER_RTT_Write(BufferIndex, pBuffer, NumBytes);
}

static float HW08_FromBits(uint32_t Bits)
{
  union {
    uint32_t Bits;
    float Value;
  } Number;

  Number.Bits = Bits;
  return Number.Value;
}

static void HW08_CallPrint(void)
{
  (void)SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "%s: %d\n",
                          "i32", (int)INT32_MIN);
}

static void HW08_CallTyped(void)
{
  (void)RTT_LogI32("i32", INT32_MIN);
}

static void HW08_CallFloat(void)
{
  RTT_LogFloat3(HW08_FromBits(UINT32_C(0xCF7FFFFF)), NULL);
}

static void HW08_CallLegacy46(void)
{
  static const char Label46[] =
    "1234567890123456789012345678901234567890123456";

  RTT_LogFloat3(HW08_FromBits(UINT32_C(0xCF7FFFFF)), Label46);
}

static void HW08_CallLegacy47(void)
{
  static const char Label47[] =
    "12345678901234567890123456789012345678901234567";

  RTT_LogFloat3(HW08_FromBits(UINT32_C(0xCF7FFFFF)), Label47);
}

static void HW08_CallSkip(void)
{
  static const char Payload[48] =
    "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";

  (void)SEGGER_RTT_WriteNoLock(RTT_LOG_BUFFER_INDEX,
                               Payload, sizeof(Payload));
}

static void HW08_CallEmpty(void)
{
  __asm volatile ("" ::: "memory");
}

static void HW08_EmptyTestBuffer(void)
{
  SEGGER_RTT_BUFFER_UP *Up = &_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX];

  Up->RdOff = 0u;
  Up->WrOff = 0u;
}

static uint32_t HW08_TimeCall(HW08_Call Call)
{
  uint32_t Start;
  uint32_t Result;
  uint32_t Primask = __get_PRIMASK();

  __disable_irq();
  HW08_EmptyTestBuffer();
  __DSB();
  __ISB();
  HW08_WriteCalls = 0u;
  HW08_WriteBytes = 0u;
  HW08_Measuring = 1u;
  Start = DWT->CYCCNT;
  Call();
  Result = DWT->CYCCNT - Start;
  HW08_Measuring = 0u;
  __set_PRIMASK(Primask);
  return Result;
}

static unsigned HW08_EmitSamples(const char *Path, HW08_Call Call,
                                 unsigned ExpectedMinWrites,
                                 unsigned ExpectedMaxWrites)
{
  unsigned Index;
  unsigned Invalid = 0u;
  unsigned MinWrites = UINT_MAX;
  unsigned MaxWrites = 0u;

  for (Index = 0u; Index < HW08_WARMUP_COUNT; ++Index) {
    (void)HW08_TimeCall(Call);
  }
  for (Index = 0u; Index < HW08_SAMPLE_COUNT; ++Index) {
    unsigned Calls;

    HW08_Samples[Index] = HW08_TimeCall(Call);
    Calls = HW08_WriteCalls;
    if (Calls < MinWrites) {
      MinWrites = Calls;
    }
    if (Calls > MaxWrites) {
      MaxWrites = Calls;
    }
    if ((HW08_Samples[Index] == 0u) ||
        (Calls < ExpectedMinWrites) || (Calls > ExpectedMaxWrites)) {
      Invalid++;
    }
  }
  (void)SEGGER_RTT_printf(0u,
    "HW-08|PATH|BEGIN|NAME=%s|N=%u|WARMUP=%u|WRITES=%u..%u|INVALID=%u\n",
    Path, HW08_SAMPLE_COUNT, HW08_WARMUP_COUNT,
    MinWrites, MaxWrites, Invalid);
  for (Index = 0u; Index < HW08_SAMPLE_COUNT; ++Index) {
    (void)SEGGER_RTT_printf(0u, "HW-08|SAMPLE|%s|%u|%u\n",
                            Path, Index, (unsigned)HW08_Samples[Index]);
    HAL_Delay(1u);
  }
  (void)SEGGER_RTT_printf(0u,
    "HW-08|PATH|END|NAME=%s|INVALID=%u|RESULT=%s\n",
    Path, Invalid, (Invalid == 0u) ? "PASS" : "FAIL");
  HAL_Delay(20u);
  return Invalid == 0u;
}

static void HW08_EmitOverhead(void)
{
  unsigned Index;

  for (Index = 0u; Index < HW08_WARMUP_COUNT; ++Index) {
    (void)HW08_TimeCall(HW08_CallEmpty);
  }
  (void)SEGGER_RTT_printf(0u,
    "HW-08|PATH|BEGIN|NAME=OVERHEAD|N=%u|WARMUP=%u|WRITES=0..0|INVALID=0\n",
    HW08_SAMPLE_COUNT, HW08_WARMUP_COUNT);
  for (Index = 0u; Index < HW08_SAMPLE_COUNT; ++Index) {
    HW08_Samples[Index] = HW08_TimeCall(HW08_CallEmpty);
    (void)SEGGER_RTT_printf(0u, "HW-08|SAMPLE|OVERHEAD|%u|%u\n",
                            Index, (unsigned)HW08_Samples[Index]);
    HAL_Delay(1u);
  }
  (void)SEGGER_RTT_printf(0u,
    "HW-08|PATH|END|NAME=OVERHEAD|INVALID=0|RESULT=PASS\n");
  HAL_Delay(20u);
}

static void HW08_RunThroughput(void)
{
  static const char Frame[HW08_THROUGHPUT_SIZE] =
    "HW08-THROUGHPUT-0123456789ABCDEF0123456789ABCDEF0123456789ABC\n";
  uint32_t Start;
  uint32_t Cycles;
  unsigned Accepted = 0u;
  unsigned Index;

  HAL_Delay(500u);
  Start = DWT->CYCCNT;
  for (Index = 0u; Index < HW08_THROUGHPUT_COUNT; ++Index) {
    if (__real_SEGGER_RTT_Write(0u, Frame, sizeof(Frame)) == sizeof(Frame)) {
      Accepted++;
    }
    if ((Index & 31u) == 31u) {
      HAL_Delay(1u);
    }
  }
  Cycles = DWT->CYCCNT - Start;
  HAL_Delay(500u);
  (void)SEGGER_RTT_printf(0u,
    "HW-08|THROUGHPUT|COUNT=%u|ACCEPT=%u|BYTES=%u|CYCLES=%u|RESULT=CHARACTERIZATION\n",
    HW08_THROUGHPUT_COUNT, Accepted,
    Accepted * HW08_THROUGHPUT_SIZE, (unsigned)Cycles);
}

static void HW08_PaintStack(void)
{
  uintptr_t Current = (uintptr_t)__get_MSP();
  uint32_t *CurrentWord;
  uint32_t *Word = &_sstack;

  HW08_StackPaintEnd = Current - HW08_STACK_MARGIN;
  CurrentWord = (uint32_t *)(HW08_StackPaintEnd & ~(uintptr_t)3u);
  while (Word < CurrentWord) {
    *Word++ = HW08_STACK_PATTERN;
  }
}

static unsigned HW08_StackUsed(void)
{
  uint32_t *Word = &_sstack;
  uint32_t *End = (uint32_t *)HW08_StackPaintEnd;

  while ((Word < End) && (*Word == HW08_STACK_PATTERN)) {
    ++Word;
  }
  return (unsigned)((uintptr_t)&_estack - (uintptr_t)Word);
}

static unsigned HW08_ControlBlockValid(void)
{
  const SEGGER_RTT_BUFFER_UP *Test =
    &_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX];

  return (memcmp(_SEGGER_RTT.acID, "SEGGER RTT", 10u) == 0) &&
         (_SEGGER_RTT.MaxNumUpBuffers == 1) &&
         (Test->pBuffer == HW08_TestBuffer) &&
         (Test->SizeOfBuffer == sizeof(HW08_TestBuffer)) &&
         ((Test->Flags & SEGGER_RTT_MODE_MASK) ==
          SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}

static void HW08_Init(void)
{
  HW08_ResetFlags = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  (void)SEGGER_RTT_ConfigUpBuffer(RTT_LOG_BUFFER_INDEX, "HW08_TEST",
                                  HW08_TestBuffer, sizeof(HW08_TestBuffer),
                                  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
  /* Keep the timing ring target-local so a probe cannot perturb its offsets. */
  _SEGGER_RTT.MaxNumUpBuffers = 1;
  HW08_PaintStack();
}

static void HW08_Run(void)
{
  unsigned StackUsed;
  unsigned Passed;
  unsigned PathsPassed = 0u;

  (void)SEGGER_RTT_printf(0u,
    "HW-08|BEGIN|STM32F103C8|%s|%s|FLOAT=%s|SKIP=%s|SAMPLES=%u|WARMUP=%u|HCLK=16000000|ICACHE=NA|DCACHE=NA|BUFFER0=8192|BUFFER1=256|SHA=" HW_TEST_LIBRARY_SHA "\n",
    HW08_PROJECT_NAME, HW08_BUILD_TYPE, HW08_FLOAT_NAME, HW08_SKIP_NAME,
    HW08_SAMPLE_COUNT, HW08_WARMUP_COUNT);
  (void)SEGGER_RTT_printf(0u,
    "HW-08|ENV|RESET=0x%08X|DWT=%s|CLIENT=CONTINUOUS_FOR_EVIDENCE|TIMING_BUFFER=HOST_INDEPENDENT\n",
    (unsigned)HW08_ResetFlags,
    ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u) ? "PASS" : "FAIL");

  HW08_EmitOverhead();
  PathsPassed += HW08_EmitSamples("LOG_PRINT_I32", HW08_CallPrint, 1u, 1u);
  PathsPassed += HW08_EmitSamples("TYPED_I32", HW08_CallTyped, 1u, 1u);
  PathsPassed += HW08_EmitSamples("FLOAT", HW08_CallFloat, 1u, 1u);
  PathsPassed += HW08_EmitSamples("LEGACY46", HW08_CallLegacy46, 1u, 1u);
  PathsPassed += HW08_EmitSamples("LEGACY47", HW08_CallLegacy47, 2u, 2u);
  PathsPassed += HW08_EmitSamples("SKIP48", HW08_CallSkip, 0u, 0u);
  HW08_RunThroughput();

  StackUsed = HW08_StackUsed();
  Passed = HW08_ControlBlockValid() &&
           (HW08_GuardBefore == HW08_GUARD_BEFORE) &&
           (HW08_GuardAfter == HW08_GUARD_AFTER) &&
           (StackUsed < (1024u - 128u)) &&
           (PathsPassed == 6u);
  (void)SEGGER_RTT_printf(0u,
    "HW-08|STACK|USED=%u|RESERVED=1024|HEADROOM=%u|GUARD=%s|RESULT=%s\n",
    StackUsed, 1024u - StackUsed,
    ((HW08_GuardBefore == HW08_GUARD_BEFORE) &&
     (HW08_GuardAfter == HW08_GUARD_AFTER)) ? "PASS" : "FAIL",
    (StackUsed < (1024u - 128u)) ? "PASS" : "FAIL");
  (void)SEGGER_RTT_printf(0u,
    "HW-08|END|PATHS=%u/6|THROUGHPUT=%s|CB=%s|FAULT=NONE|STACK=%s|RESULT=%s\n",
    PathsPassed, "CHARACTERIZATION",
    HW08_ControlBlockValid() ? "PASS" : "FAIL",
    (StackUsed < (1024u - 128u)) ? "PASS" : "FAIL",
    Passed ? "PASS" : "FAIL");
}
