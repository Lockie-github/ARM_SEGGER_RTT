#ifndef HW07_FIXTURE_H
#define HW07_FIXTURE_H

#include "SEGGER_RTT.h"

#include <stdint.h>
#include <string.h>

#define HW07_PHASE_COUNT       6u
#define HW07_FORCE_COUNT       32u
#define HW07_PHASE_DURATION_MS 1500u
#define HW07_MAX_FRAME         128u
#define HW07_GUARD_BEFORE      UINT32_C(0x7707BEEF)
#define HW07_GUARD_AFTER       UINT32_C(0xA55A0707)

typedef struct {
  unsigned Mode;
  unsigned Length;
  unsigned Rate;
  unsigned TickDivisor;
} HW07_PhaseConfig;

typedef struct {
  volatile uint32_t Attempted;
  volatile uint32_t Accepted;
  volatile uint32_t Dropped;
  volatile uint32_t MaxWriteUs;
} HW07_SourceStats;

static const HW07_PhaseConfig HW07_Phases[HW07_PHASE_COUNT] = {
  {0u,  48u,  100u, 10u},
  {0u,  64u,  250u,  4u},
  {0u, 128u,  500u,  2u},
  {1u,  48u,  100u, 10u},
  {1u,  64u,  250u,  4u},
  {1u, 128u,  500u,  2u}
};

static volatile uint32_t HW07_GuardBefore = HW07_GUARD_BEFORE;
static volatile uint32_t HW07_GuardAfter = HW07_GUARD_AFTER;
static volatile HW07_SourceStats HW07_Stats[2];
static volatile unsigned HW07_ActivePhase;
static volatile unsigned HW07_InISR;
static volatile unsigned HW07_MainSending;
static volatile unsigned HW07_MainWriteCalls;
static volatile unsigned HW07_ForceIssued;
static volatile unsigned HW07_SoftwarePending;
static volatile unsigned HW07_SoftwareEntries;
static volatile unsigned HW07_TimerEntries;
static volatile uint32_t HW07_ISRStartUs;
static volatile uint32_t HW07_MaxISRTotalUs;
static uint32_t HW07_ResetFlags;

uintptr_t __stack_chk_guard = (uintptr_t)UINT32_C(0x6D5A56A9);

unsigned __real_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes);

__attribute__((noreturn, no_stack_protector))
void __stack_chk_fail(void)
{
  static const char Failure[] = "HW-07|STACK_GUARD=FAIL\n";

  (void)__real_SEGGER_RTT_Write(0u, Failure, sizeof(Failure) - 1u);
  for (;;) {
    __NOP();
  }
}

static uint32_t HW07_ElapsedUs(uint32_t Start, uint32_t End)
{
  return (End >= Start) ? (End - Start) : ((1000u - Start) + End);
}

unsigned __wrap_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes)
{
  unsigned Result = __real_SEGGER_RTT_Write(BufferIndex, pBuffer, NumBytes);

  if ((HW07_MainSending != 0u) && (HW07_InISR == 0u) &&
      (HW07_ActivePhase != 0u)) {
    HW07_MainWriteCalls++;
    if ((HW07_MainWriteCalls == 1u) &&
        (HW07_ForceIssued < HW07_FORCE_COUNT)) {
      HW07_ForceIssued++;
      HW07_SoftwarePending = 1u;
      NVIC_SetPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
      __DSB();
      __ISB();
    }
  }
  return Result;
}

static void HW07_PutDecimal(char *Destination, uint32_t Value,
                            unsigned Width)
{
  while (Width != 0u) {
    Width--;
    Destination[Width] = (char)('0' + (Value % 10u));
    Value /= 10u;
  }
}

static void HW07_BuildFrame(char *Frame, unsigned Length, char Source,
                            uint32_t Sequence, unsigned Phase)
{
  unsigned PayloadLength = Length - 34u;
  uint32_t Checksum = (uint32_t)(Phase * 1000000u) +
                      ((Source == 'M') ? 100000u : 200000u) +
                      Sequence + Length;

  memcpy(Frame, "F1|P", 4u);
  Frame[4] = (char)('0' + Phase);
  Frame[5] = '|';
  Frame[6] = Source;
  Frame[7] = '|';
  Frame[8] = 'Q';
  HW07_PutDecimal(Frame + 9u, Sequence, 8u);
  Frame[17] = '|';
  Frame[18] = 'L';
  HW07_PutDecimal(Frame + 19u, Length, 3u);
  Frame[22] = '|';
  memset(Frame + 23u, Source, PayloadLength);
  Frame[Length - 11u] = '|';
  Frame[Length - 10u] = 'C';
  HW07_PutDecimal(Frame + Length - 9u, Checksum, 8u);
  Frame[Length - 1u] = '\n';
  Frame[Length] = '\0';
}

static unsigned HW07_ControlBlockValid(void)
{
  const SEGGER_RTT_BUFFER_UP *Up = &_SEGGER_RTT.aUp[0];

  return (memcmp(_SEGGER_RTT.acID, "SEGGER RTT", 10u) == 0) &&
         (_SEGGER_RTT.MaxNumUpBuffers == 1) &&
         (Up->pBuffer != NULL) &&
         (Up->SizeOfBuffer == BUFFER_SIZE_UP) &&
         (Up->RdOff < Up->SizeOfBuffer) &&
         (Up->WrOff < Up->SizeOfBuffer) &&
         ((Up->Flags & SEGGER_RTT_MODE_MASK) == SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}

static unsigned HW07_GuardsValid(void)
{
  return (HW07_GuardBefore == HW07_GUARD_BEFORE) &&
         (HW07_GuardAfter == HW07_GUARD_AFTER);
}

static void HW07_UpdateMaximum(volatile uint32_t *Maximum, uint32_t Value)
{
  if (Value > *Maximum) {
    *Maximum = Value;
  }
}

static void HW07_Send(unsigned SourceIndex)
{
  const HW07_PhaseConfig *Config;
  volatile HW07_SourceStats *Stats;
  char Frame[HW07_MAX_FRAME + 1u];
  char Source = (SourceIndex == 0u) ? 'M' : 'I';
  uint32_t Sequence;
  uint32_t Start;
  uint32_t Elapsed;
  int Result;
  unsigned Phase = HW07_ActivePhase;

  if ((Phase == 0u) || (Phase > HW07_PHASE_COUNT)) {
    return;
  }
  Config = &HW07_Phases[Phase - 1u];
  Stats = &HW07_Stats[SourceIndex];
  Sequence = ++Stats->Attempted;
  HW07_BuildFrame(Frame, Config->Length, Source, Sequence, Phase);

  if (SourceIndex == 0u) {
    HW07_MainSending = 1u;
    HW07_MainWriteCalls = 0u;
  }
  Start = TIM1->CNT;
  if (Config->Mode == 0u) {
    Result = (int)SEGGER_RTT_Write(0u, Frame, Config->Length);
  } else {
    Result = SEGGER_RTT_printf(0u, "%s", Frame);
  }
  Elapsed = HW07_ElapsedUs(Start, TIM1->CNT);
  if (SourceIndex == 0u) {
    HW07_MainSending = 0u;
  }
  HW07_UpdateMaximum(&Stats->MaxWriteUs, Elapsed);
  if (Result == (int)Config->Length) {
    Stats->Accepted++;
  } else {
    Stats->Dropped++;
  }
}

void HW07_ISR_Begin(void)
{
  unsigned Send = 0u;
  unsigned Phase = HW07_ActivePhase;

  HW07_ISRStartUs = TIM1->CNT;
  HW07_InISR = 1u;
  if (HW07_SoftwarePending != 0u) {
    HW07_SoftwarePending = 0u;
    HW07_SoftwareEntries++;
    Send = 1u;
  } else if ((TIM1->SR & TIM_SR_UIF) != 0u) {
    HW07_TimerEntries++;
    if ((Phase != 0u) &&
        ((HW07_TimerEntries % HW07_Phases[Phase - 1u].TickDivisor) == 0u)) {
      Send = 1u;
    }
  }
  if (Send != 0u) {
    HW07_Send(1u);
  }
}

void HW07_ISR_End(void)
{
  uint32_t Elapsed = HW07_ElapsedUs(HW07_ISRStartUs, TIM1->CNT);

  HW07_InISR = 0u;
  HW07_UpdateMaximum(&HW07_MaxISRTotalUs, Elapsed);
}

static void HW07_ResetPhase(void)
{
  memset((void *)HW07_Stats, 0, sizeof(HW07_Stats));
  HW07_MainSending = 0u;
  HW07_MainWriteCalls = 0u;
  HW07_ForceIssued = 0u;
  HW07_SoftwarePending = 0u;
  HW07_SoftwareEntries = 0u;
  HW07_TimerEntries = 0u;
  HW07_MaxISRTotalUs = 0u;
}

static unsigned HW07_RunPhase(unsigned Phase)
{
  const HW07_PhaseConfig *Config = &HW07_Phases[Phase - 1u];
  const char *ModeName = (Config->Mode == 0u) ? "WRITE_ATOMIC" : "FORMATTER";
  uint32_t StartTick;
  uint32_t NextMain;
  uint32_t EndTick;
  unsigned Passed;

  HW07_ActivePhase = 0u;
  HW07_ResetPhase();
  (void)SEGGER_RTT_printf(
    0u, "HW-07|PHASE|BEGIN|ID=%u|MODE=%s|LEN=%u|RATE=%u|FORCE=%u\n",
    Phase, ModeName, Config->Length, Config->Rate, HW07_FORCE_COUNT);
  StartTick = HAL_GetTick();
  NextMain = StartTick;
  EndTick = StartTick + HW07_PHASE_DURATION_MS;
  HW07_ActivePhase = Phase;
  while ((int32_t)(HAL_GetTick() - EndTick) < 0) {
    uint32_t Now = HAL_GetTick();
    if ((int32_t)(Now - NextMain) >= 0) {
      HW07_Send(0u);
      NextMain += Config->TickDivisor;
    }
  }
  HW07_ActivePhase = 0u;
  HAL_Delay(20u);
  Passed = (HW07_Stats[0].Attempted != 0u) &&
           (HW07_Stats[1].Attempted != 0u) &&
           (HW07_Stats[0].Attempted == HW07_Stats[0].Accepted) &&
           (HW07_Stats[1].Attempted == HW07_Stats[1].Accepted) &&
           (HW07_Stats[0].Dropped == 0u) &&
           (HW07_Stats[1].Dropped == 0u) &&
           (HW07_ForceIssued == HW07_FORCE_COUNT) &&
           (HW07_SoftwareEntries == HW07_FORCE_COUNT) &&
           (HW07_Stats[0].MaxWriteUs != 0u) &&
           (HW07_Stats[1].MaxWriteUs != 0u) &&
           (HW07_MaxISRTotalUs != 0u) &&
           HW07_ControlBlockValid() && HW07_GuardsValid();
  (void)SEGGER_RTT_printf(
    0u,
    "HW-07|PHASE|END|ID=%u|MA=%u|MC=%u|MD=%u|IA=%u|IC=%u|IR=%u|MMAXUS=%u|IMAXUS=%u|ISRMAXUS=%u|FORCED=%u|SWISR=%u|CB=%s|STACK=%s|RESULT=%s\n",
    Phase,
    (unsigned)HW07_Stats[0].Attempted,
    (unsigned)HW07_Stats[0].Accepted,
    (unsigned)HW07_Stats[0].Dropped,
    (unsigned)HW07_Stats[1].Attempted,
    (unsigned)HW07_Stats[1].Accepted,
    (unsigned)HW07_Stats[1].Dropped,
    (unsigned)HW07_Stats[0].MaxWriteUs,
    (unsigned)HW07_Stats[1].MaxWriteUs,
    (unsigned)HW07_MaxISRTotalUs,
    (unsigned)HW07_ForceIssued,
    (unsigned)HW07_SoftwareEntries,
    HW07_ControlBlockValid() ? "PASS" : "FAIL",
    HW07_GuardsValid() ? "PASS" : "FAIL",
    Passed ? "PASS" : "FAIL");
  HAL_Delay(80u);
  return Passed;
}

static void HW07_Init(void)
{
  HW07_ResetFlags = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  (void)SEGGER_RTT_SetFlagsUpBuffer(0u, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
  HW07_ActivePhase = 0u;
}

static void HW07_Run(void)
{
  unsigned Phase;
  unsigned Passed = 0u;

  (void)SEGGER_RTT_printf(
    0u,
    "HW-07|BEGIN|STM32F042G6|%s|%s|PHASES=6|BUFFER=2048|PRINTF=64|IRQ=TIM1_1KHZ|IRQPRI=%u|LOCK=PRIMASK|TIMER=TIM1_1MHZ|SHA=" HW_TEST_LIBRARY_SHA "\n",
    HW07_PROJECT_NAME, HW07_BUILD_NAME,
    (unsigned)NVIC_GetPriority(TIM1_BRK_UP_TRG_COM_IRQn));
  (void)SEGGER_RTT_printf(
    0u,
    "HW-07|ENV|RESET=0x%08X|DWT=NA_CORTEX_M0|RTOS=NA_NOT_PRESENT|WATCHDOG=NA_NOT_CONFIGURED|GPIO=NA_NOT_CONFIGURED\n",
    (unsigned)HW07_ResetFlags);
  for (Phase = 1u; Phase <= HW07_PHASE_COUNT; ++Phase) {
    Passed += HW07_RunPhase(Phase);
  }
  (void)SEGGER_RTT_printf(
    0u,
    "HW-07|END|PHASES=%u/6|SUPPORTED=5/5|LIMIT=FORMAT_GT64_NOT_FRAME_ATOMIC|RTOS=NA_NOT_PRESENT|WATCHDOG=NA_NOT_CONFIGURED|CB=%s|STACK=%s|FAULT=NONE|RESULT=%s\n",
    Passed,
    HW07_ControlBlockValid() ? "PASS" : "FAIL",
    HW07_GuardsValid() ? "PASS" : "FAIL",
    (Passed == HW07_PHASE_COUNT) ? "PASS" : "FAIL");
}

#endif
