#include "SEGGER_RTT.h"
#include "rtt_log.h"

#include <stdint.h>
#include <string.h>

#define HW06_CASE_COUNT 11u
#define HW06_PRINTF_SIZE 64u
#define HW06_GUARD_BEFORE UINT32_C(0x6155A11C)
#define HW06_GUARD_AFTER  UINT32_C(0xC06E5AFE)

typedef struct {
  uint32_t GuardBefore;
  unsigned char Data[HW06_UP_SIZE];
  uint32_t GuardAfter;
} HW06_TestBuffer;

typedef struct {
  unsigned Calls;
  unsigned Requested;
  unsigned Returned;
  unsigned Failed;
  unsigned MaxRequest;
  unsigned LastRequest;
  unsigned LastReturn;
} HW06_WriteStats;

typedef struct {
  const char *Name;
  int ApiReturn;
  unsigned Calls;
  unsigned Requested;
  unsigned Returned;
  unsigned WrBefore;
  unsigned WrAfter;
  unsigned Accepted;
  unsigned Rejected;
  uint32_t Hash;
  unsigned Passed;
} HW06_Result;

static HW06_TestBuffer HW06_Buffer;
static HW06_WriteStats HW06_Stats;
static HW06_Result HW06_Results[HW06_CASE_COUNT];
static unsigned HW06_MeasurementActive;
static unsigned HW06_Passed;
static uint32_t HW06_ResetFlags;
static uint32_t HW06_Business;
static uint32_t HW06_Attempted;
static uint32_t HW06_Accepted;
static uint32_t HW06_Dropped;
static uint32_t HW06_LastLiveTick;
static uint32_t HW06_LastReportTick;

uintptr_t __stack_chk_guard = (uintptr_t)UINT32_C(0x6D5A56A9);

unsigned __real_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes);

__attribute__((noreturn, no_stack_protector))
void __stack_chk_fail(void)
{
  static const char Failure[] = "HW-06|STACK_GUARD=FAIL\n";

  (void)__real_SEGGER_RTT_Write(0u, Failure, sizeof(Failure) - 1u);
  for (;;) {
    __NOP();
  }
}

unsigned __wrap_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes)
{
  unsigned ReturnValue;

  if ((HW06_MeasurementActive != 0u) &&
      (BufferIndex == RTT_LOG_BUFFER_INDEX)) {
    HW06_Stats.Calls++;
    HW06_Stats.Requested += NumBytes;
    HW06_Stats.LastRequest = NumBytes;
    if (NumBytes > HW06_Stats.MaxRequest) {
      HW06_Stats.MaxRequest = NumBytes;
    }
  }
  ReturnValue = __real_SEGGER_RTT_Write(BufferIndex, pBuffer, NumBytes);
  if ((HW06_MeasurementActive != 0u) &&
      (BufferIndex == RTT_LOG_BUFFER_INDEX)) {
    HW06_Stats.Returned += ReturnValue;
    HW06_Stats.LastReturn = ReturnValue;
    if (ReturnValue != NumBytes) {
      HW06_Stats.Failed++;
    }
  }
  return ReturnValue;
}

static uint32_t HW06_Hash(const unsigned char *Data, unsigned Length)
{
  uint32_t Hash = UINT32_C(2166136261);
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    Hash ^= Data[Index];
    Hash *= UINT32_C(16777619);
  }
  return Hash;
}

static void HW06_Fill(char *Data, unsigned Length)
{
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    Data[Index] = (char)('A' + (Index % 26u));
  }
  Data[Length] = '\0';
}

static unsigned HW06_GuardsValid(void)
{
  return (HW06_Buffer.GuardBefore == HW06_GUARD_BEFORE) &&
         (HW06_Buffer.GuardAfter == HW06_GUARD_AFTER);
}

static void HW06_ResetRing(void)
{
  HW06_Buffer.GuardBefore = HW06_GUARD_BEFORE;
  memset(HW06_Buffer.Data, 0xA5, sizeof(HW06_Buffer.Data));
  HW06_Buffer.GuardAfter = HW06_GUARD_AFTER;
  _SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].RdOff = 0u;
  _SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff = 0u;
  _SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].Flags =
    SEGGER_RTT_MODE_NO_BLOCK_SKIP;
  memset(&HW06_Stats, 0, sizeof(HW06_Stats));
}

static void HW06_SetAvailable(unsigned Available)
{
  HW06_ResetRing();
  _SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff =
    HW06_UP_SIZE - 1u - Available;
}

static void HW06_Save(unsigned Index, const char *Name, int ApiReturn,
                      unsigned WrBefore, unsigned Accepted,
                      unsigned Rejected, unsigned Passed)
{
  HW06_Result *Result = &HW06_Results[Index];
  unsigned WrAfter = _SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff;

  Result->Name = Name;
  Result->ApiReturn = ApiReturn;
  Result->Calls = HW06_Stats.Calls;
  Result->Requested = HW06_Stats.Requested;
  Result->Returned = HW06_Stats.Returned;
  Result->WrBefore = WrBefore;
  Result->WrAfter = WrAfter;
  Result->Accepted = Accepted;
  Result->Rejected = Rejected;
  Result->Hash = HW06_Hash(HW06_Buffer.Data, HW06_UP_SIZE);
  Result->Passed = Passed && HW06_GuardsValid();
  HW06_Passed += Result->Passed;
}

static void HW06_RunFormatterCase(unsigned Index, const char *Name,
                                  unsigned TotalLength)
{
  char Payload[HW06_UP_SIZE + 1u];
  unsigned PayloadLength = TotalLength - 1u;
  unsigned ExpectedCalls = (TotalLength + HW06_PRINTF_SIZE - 1u) /
                           HW06_PRINTF_SIZE;
  int ApiReturn;
  unsigned Passed;

  HW06_ResetRing();
  HW06_Fill(Payload, PayloadLength);
  HW06_MeasurementActive = 1u;
  ApiReturn = RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, "%s", Payload);
  HW06_MeasurementActive = 0u;
  Passed = (ApiReturn == (int)TotalLength) &&
           (HW06_Stats.Calls == ExpectedCalls) &&
           (HW06_Stats.Requested == TotalLength) &&
           (HW06_Stats.Returned == TotalLength) &&
           (HW06_Stats.Failed == 0u) &&
           (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff == TotalLength) &&
           (memcmp(HW06_Buffer.Data, Payload, PayloadLength) == 0) &&
           (HW06_Buffer.Data[PayloadLength] == '\n');
  HW06_Save(Index, Name, ApiReturn, 0u, 1u, 0u, Passed);
}

static void HW06_RunFormatterCapacity(unsigned Index, unsigned Overflow)
{
  char Payload[HW06_UP_SIZE + 1u];
  unsigned TotalLength = HW06_UP_SIZE - 1u + Overflow;
  unsigned PayloadLength = TotalLength - 1u;
  unsigned ExpectedReturned = Overflow ?
    (HW06_UP_SIZE - HW06_PRINTF_SIZE) : (HW06_UP_SIZE - 1u);
  unsigned ExpectedCalls = Overflow ?
    (HW06_UP_SIZE / HW06_PRINTF_SIZE) :
    ((TotalLength + HW06_PRINTF_SIZE - 1u) / HW06_PRINTF_SIZE);
  int ApiReturn;
  unsigned Passed;

  HW06_ResetRing();
  HW06_Fill(Payload, PayloadLength);
  HW06_MeasurementActive = 1u;
  ApiReturn = RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, "%s", Payload);
  HW06_MeasurementActive = 0u;
  Passed = (HW06_Stats.Calls == ExpectedCalls) &&
           (HW06_Stats.Returned == ExpectedReturned) &&
           (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff == ExpectedReturned);
  if (Overflow != 0u) {
    Passed = Passed && (ApiReturn == -1) && (HW06_Stats.Failed == 1u) &&
             (HW06_Stats.LastRequest == HW06_PRINTF_SIZE) &&
             (HW06_Stats.LastReturn == 0u) &&
             (memcmp(HW06_Buffer.Data, Payload, ExpectedReturned) == 0);
  } else {
    Passed = Passed && (ApiReturn == (int)TotalLength) &&
             (HW06_Stats.Failed == 0u) &&
             (memcmp(HW06_Buffer.Data, Payload, PayloadLength) == 0) &&
             (HW06_Buffer.Data[PayloadLength] == '\n');
  }
  HW06_Save(Index, Overflow ? "FMT_OVER" : "FMT_CAPACITY", ApiReturn,
            0u, Overflow ? 0u : 1u, Overflow ? 1u : 0u, Passed);
}

static void HW06_RunStringCase(unsigned Index, unsigned Overflow)
{
  char Payload[HW06_UP_SIZE + 1u];
  unsigned Length = HW06_UP_SIZE - 1u + Overflow;
  int ApiReturn;
  unsigned Passed;

  HW06_ResetRing();
  HW06_Fill(Payload, Length);
  HW06_MeasurementActive = 1u;
  ApiReturn = RTT_LogString(Payload);
  HW06_MeasurementActive = 0u;
  if (Overflow != 0u) {
    Passed = (ApiReturn == 0) && (HW06_Stats.Calls == 0u) &&
             (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff == 0u);
  } else {
    Passed = (ApiReturn == (int)Length) && (HW06_Stats.Calls == 0u) &&
             (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff == Length) &&
             (memcmp(HW06_Buffer.Data, Payload, Length) == 0);
  }
  HW06_Save(Index, Overflow ? "STRING_OVER" : "STRING_EXACT",
            ApiReturn, 0u, Overflow ? 0u : 1u, Overflow ? 1u : 0u,
            Passed);
}

static void HW06_RunTypedCapacity(unsigned Index, unsigned Short)
{
  static const char Expected[] = "SEQ: 4294967295\n";
  const unsigned Length = sizeof(Expected) - 1u;
  unsigned Available = Length - Short;
  unsigned WrBefore;
  int ApiReturn;
  unsigned Passed;

  HW06_SetAvailable(Available);
  WrBefore = _SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff;
  HW06_MeasurementActive = 1u;
  ApiReturn = RTT_LogU32("SEQ", UINT32_MAX);
  HW06_MeasurementActive = 0u;
  if (Short != 0u) {
    Passed = (ApiReturn == -1) && (HW06_Stats.Calls == 1u) &&
             (HW06_Stats.Returned == 0u) &&
             (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff == WrBefore) &&
             (HW06_Buffer.Data[WrBefore] == 0xA5u);
  } else {
    Passed = (ApiReturn == (int)Length) && (HW06_Stats.Calls == 1u) &&
             (HW06_Stats.Returned == Length) &&
             (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff == WrBefore + Length) &&
             (memcmp(HW06_Buffer.Data + WrBefore, Expected, Length) == 0);
  }
  HW06_Save(Index, Short ? "TYPED_SHORT" : "TYPED_EXACT", ApiReturn,
            WrBefore, Short ? 0u : 1u, Short ? 1u : 0u, Passed);
}

static void HW06_RunTypedBurst(unsigned Index)
{
  static const char Expected[] = "B: 0xA5A5A5A5\n";
  const unsigned FrameLength = sizeof(Expected) - 1u;
  unsigned ExpectedAccepted = (HW06_UP_SIZE - 1u) / FrameLength;
  unsigned Attempts = ExpectedAccepted + 8u;
  unsigned Accepted = 0u;
  unsigned Rejected = 0u;
  unsigned Attempt;
  unsigned Passed = 1u;

  HW06_ResetRing();
  HW06_MeasurementActive = 1u;
  for (Attempt = 0u; Attempt < Attempts; ++Attempt) {
    int ReturnValue = RTT_LogHex32("B", UINT32_C(0xA5A5A5A5));
    if (ReturnValue == (int)FrameLength) {
      Accepted++;
    } else if (ReturnValue == -1) {
      Rejected++;
    } else {
      Passed = 0u;
    }
  }
  HW06_MeasurementActive = 0u;
  for (Attempt = 0u; Attempt < Accepted; ++Attempt) {
    if (memcmp(HW06_Buffer.Data + (Attempt * FrameLength),
               Expected, FrameLength) != 0) {
      Passed = 0u;
    }
  }
  Passed = Passed && (Accepted == ExpectedAccepted) &&
           (Rejected == (Attempts - ExpectedAccepted)) &&
           (HW06_Stats.Returned == Accepted * FrameLength) &&
           (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff ==
            Accepted * FrameLength);
  HW06_Save(Index, "TYPED_BURST", -1, 0u, Accepted, Rejected, Passed);
}

static void HW06_RunFormatterBurst(unsigned Index)
{
  char Payload[32];
  const unsigned FrameLength = 32u;
  unsigned ExpectedAccepted = (HW06_UP_SIZE - 1u) / FrameLength;
  unsigned Attempts = ExpectedAccepted + 8u;
  unsigned Accepted = 0u;
  unsigned Rejected = 0u;
  unsigned Attempt;
  unsigned Passed = 1u;

  HW06_Fill(Payload, FrameLength - 1u);
  HW06_ResetRing();
  HW06_MeasurementActive = 1u;
  for (Attempt = 0u; Attempt < Attempts; ++Attempt) {
    int ReturnValue = RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, "%s", Payload);
    if (ReturnValue == (int)FrameLength) {
      Accepted++;
    } else if (ReturnValue == -1) {
      Rejected++;
    } else {
      Passed = 0u;
    }
  }
  HW06_MeasurementActive = 0u;
  for (Attempt = 0u; Attempt < Accepted; ++Attempt) {
    unsigned Offset = Attempt * FrameLength;
    if ((memcmp(HW06_Buffer.Data + Offset, Payload,
                FrameLength - 1u) != 0) ||
        (HW06_Buffer.Data[Offset + FrameLength - 1u] != '\n')) {
      Passed = 0u;
    }
  }
  Passed = Passed && (Accepted == ExpectedAccepted) &&
           (Rejected == (Attempts - ExpectedAccepted)) &&
           (HW06_Stats.Returned == Accepted * FrameLength) &&
           (_SEGGER_RTT.aUp[RTT_LOG_BUFFER_INDEX].WrOff ==
            Accepted * FrameLength);
  HW06_Save(Index, "FMT_BURST", -1, 0u, Accepted, Rejected, Passed);
}

static void HW06_RunCases(void)
{
  HW06_Passed = 0u;
  HW06_RunFormatterCase(0u, "FMT63", 63u);
  HW06_RunFormatterCase(1u, "FMT64", 64u);
  HW06_RunFormatterCase(2u, "FMT65", 65u);
  HW06_RunFormatterCapacity(3u, 0u);
  HW06_RunFormatterCapacity(4u, 1u);
  HW06_RunStringCase(5u, 0u);
  HW06_RunStringCase(6u, 1u);
  HW06_RunTypedCapacity(7u, 0u);
  HW06_RunTypedCapacity(8u, 1u);
  HW06_RunTypedBurst(9u);
  HW06_RunFormatterBurst(10u);
}

static void HW06_Report(void)
{
  unsigned Index;

  (void)SEGGER_RTT_printf(0u,
    "HW-06|BEGIN|STM32F411CE|%s|%s|UP=%u|PRINTF=64|SHA=" HW_TEST_LIBRARY_SHA "\n",
    HW06_PROJECT_NAME, HW06_BUILD_TYPE, (unsigned)HW06_UP_SIZE);
  (void)SEGGER_RTT_printf(0u, "HW-06|RESET_FLAGS=0x%08X\n",
                          (unsigned)HW06_ResetFlags);
  for (Index = 0u; Index < HW06_CASE_COUNT; ++Index) {
    const HW06_Result *Result = &HW06_Results[Index];
    (void)SEGGER_RTT_printf(0u,
      "HW-06|CASE|%02u|%s|API=%d|CALLS=%u|REQ=%u|RET=%u|WR=%u>%u|ACC=%u|DROP=%u|HASH=%08X|%s\n",
      Index + 1u, Result->Name, Result->ApiReturn, Result->Calls,
      Result->Requested, Result->Returned, Result->WrBefore,
      Result->WrAfter, Result->Accepted, Result->Rejected,
      (unsigned)Result->Hash, Result->Passed ? "PASS" : "FAIL");
  }
  (void)SEGGER_RTT_printf(0u,
    "HW-06|END|CASES=%u/%u|BUFFER=%u|STACK=PASS|RESULT=%s\n",
    HW06_Passed, (unsigned)HW06_CASE_COUNT, (unsigned)HW06_UP_SIZE,
    (HW06_Passed == HW06_CASE_COUNT) ? "PASS" : "FAIL");
}

static void HW06_Init(void)
{
  HW06_ResetFlags = RCC->CSR;
  (void)SEGGER_RTT_ConfigUpBuffer(RTT_LOG_BUFFER_INDEX, "HW06-PAYLOAD",
                                  HW06_Buffer.Data, HW06_UP_SIZE,
                                  SEGGER_RTT_MODE_NO_BLOCK_SKIP);
  HW06_RunCases();
  HW06_LastLiveTick = HAL_GetTick();
  HW06_LastReportTick = HAL_GetTick() - 1000u;
}

static void HW06_Tick(void)
{
  uint32_t Now = HAL_GetTick();

  if ((uint32_t)(Now - HW06_LastLiveTick) >= 10u) {
    int ReturnValue;

    HW06_LastLiveTick = Now;
    HW06_Business++;
    HW06_Attempted++;
    HW06_Accepted++;
    ReturnValue = SEGGER_RTT_printf(0u,
      "HW-06|LIVE|SEQ=%u|BUS=%u|ATT=%u|ACC=%u|DROP=%u\n",
      (unsigned)HW06_Attempted, (unsigned)HW06_Business,
      (unsigned)HW06_Attempted, (unsigned)HW06_Accepted,
      (unsigned)HW06_Dropped);
    if (ReturnValue < 0) {
      HW06_Accepted--;
      HW06_Dropped++;
    }
  }
  if ((uint32_t)(Now - HW06_LastReportTick) >= 1000u) {
    HW06_LastReportTick = Now;
    HW06_Report();
  }
  HAL_Delay(1u);
}
