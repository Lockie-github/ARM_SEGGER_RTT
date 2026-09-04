#include "SEGGER_RTT.h"

#include <stdint.h>
#include <string.h>

#if HW04_SKIP_ASM
#define HW04_PATH_NAME "ASM"
#else
#define HW04_PATH_NAME "C"
#endif

#define HW04_RING_SIZE 8u
#define HW04_CASE_COUNT 10u

typedef struct {
  const char *Name;
  unsigned Flags;
  unsigned InitialWr;
  unsigned InitialRd;
  const char *Data;
  unsigned Length;
  unsigned ExpectedReturn;
  unsigned ExpectedWr;
  const char *ExpectedBuffer;
} HW04_Case;

typedef struct {
  unsigned ReturnValue;
  unsigned RdOff;
  unsigned WrOff;
  uint32_t Cycles;
  char Buffer[HW04_RING_SIZE];
  unsigned Passed;
} HW04_Result;

static char HW04_Ring[HW04_RING_SIZE];
static HW04_Result HW04_Results[HW04_CASE_COUNT];

static const HW04_Case HW04_Cases[HW04_CASE_COUNT] = {
  {"SKIP_ZERO",   SEGGER_RTT_MODE_NO_BLOCK_SKIP,      0u, 0u, "Z",     0u, 0u, 0u, "........"},
  {"SKIP_CONTIG", SEGGER_RTT_MODE_NO_BLOCK_SKIP,      1u, 0u, "ABC",   3u, 3u, 4u, ".ABC...."},
  {"SKIP_GAP",    SEGGER_RTT_MODE_NO_BLOCK_SKIP,      1u, 6u, "XYZ",   3u, 3u, 4u, ".XYZ...."},
  {"SKIP_WRAP",   SEGGER_RTT_MODE_NO_BLOCK_SKIP,      6u, 4u, "WXYZ",  4u, 4u, 2u, "YZ....WX"},
  {"SKIP_EXACT",  SEGGER_RTT_MODE_NO_BLOCK_SKIP,      6u, 3u, "ABCD",  4u, 4u, 2u, "CD....AB"},
  {"SKIP_SHORT",  SEGGER_RTT_MODE_NO_BLOCK_SKIP,      6u, 3u, "ABCDE", 5u, 0u, 6u, "........"},
  {"SKIP_FULL",   SEGGER_RTT_MODE_NO_BLOCK_SKIP,      7u, 0u, "Z",     1u, 0u, 7u, "........"},
  {"TRIM_SHORT",  SEGGER_RTT_MODE_NO_BLOCK_TRIM,      6u, 2u, "ABCDE", 5u, 3u, 1u, "C.....AB"},
  {"BLOCK_FIT",   SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL, 1u, 0u, "ABC",   3u, 3u, 4u, ".ABC...."},
  {"INVALID",     3u,                                 2u, 0u, "AB",    2u, 0u, 2u, "........"}
};

static void HW04_ResetRing(const HW04_Case *Test)
{
  memset(HW04_Ring, '.', sizeof(HW04_Ring));
  _SEGGER_RTT.aUp[1].sName = "HW04_TEST";
  _SEGGER_RTT.aUp[1].pBuffer = HW04_Ring;
  _SEGGER_RTT.aUp[1].SizeOfBuffer = sizeof(HW04_Ring);
  _SEGGER_RTT.aUp[1].WrOff = Test->InitialWr;
  _SEGGER_RTT.aUp[1].RdOff = Test->InitialRd;
  _SEGGER_RTT.aUp[1].Flags = Test->Flags;
}

static void HW04_RunCase(unsigned Index)
{
  const HW04_Case *Test = &HW04_Cases[Index];
  HW04_Result *Result = &HW04_Results[Index];
  uint32_t Start;

  HW04_ResetRing(Test);
  __DSB();
  __ISB();
  Start = TIM1->CNT;
  Result->ReturnValue = SEGGER_RTT_WriteNoLock(1u, Test->Data, Test->Length);
  Result->Cycles = (TIM1->CNT + 1000u - Start) % 1000u;
  __DSB();
  memcpy(Result->Buffer, HW04_Ring, sizeof(Result->Buffer));
  Result->RdOff = _SEGGER_RTT.aUp[1].RdOff;
  Result->WrOff = _SEGGER_RTT.aUp[1].WrOff;
  Result->Passed = (Result->ReturnValue == Test->ExpectedReturn) &&
                   (Result->RdOff == Test->InitialRd) &&
                   (Result->WrOff == Test->ExpectedWr) &&
                   (memcmp(Result->Buffer, Test->ExpectedBuffer,
                           sizeof(Result->Buffer)) == 0);
}

static void HW04_BufferHex(const char *Buffer, char *Text)
{
  static const char Hex[] = "0123456789ABCDEF";
  unsigned Index;

  for (Index = 0u; Index < HW04_RING_SIZE; ++Index) {
    unsigned Value = (unsigned)(unsigned char)Buffer[Index];
    Text[Index * 2u] = Hex[Value >> 4];
    Text[Index * 2u + 1u] = Hex[Value & 0x0Fu];
  }
  Text[HW04_RING_SIZE * 2u] = '\0';
}

static void HW04_Run(void)
{
  unsigned Index;
  unsigned Passed = 0u;
  char BufferText[HW04_RING_SIZE * 2u + 1u];

  SEGGER_RTT_Init();
  for (Index = 0u; Index < HW04_CASE_COUNT; ++Index) {
    HW04_RunCase(Index);
  }

  (void)SEGGER_RTT_printf(0u,
    "HW-04|BEGIN|STM32F042G6|%s|%s|PATH=%s|SKIP_ASM=%u|SHA=" HW_TEST_LIBRARY_SHA "\n",
    HW04_PROJECT_NAME, HW04_BUILD_NAME, HW04_PATH_NAME,
    (unsigned)HW04_SKIP_ASM);
  for (Index = 0u; Index < HW04_CASE_COUNT; ++Index) {
    const HW04_Case *Test = &HW04_Cases[Index];
    const HW04_Result *Result = &HW04_Results[Index];
    HW04_BufferHex(Result->Buffer, BufferText);
    Passed += Result->Passed;
    (void)SEGGER_RTT_printf(0u,
      "HW-04|CASE|%02u|%s|FLAGS=%u|LEN=%u|RET=%u|RD=%u|WR=%u|BUF=%s|CYCLES=%u|%s\n",
      Index + 1u, Test->Name, Test->Flags, Test->Length,
      Result->ReturnValue, Result->RdOff, Result->WrOff, BufferText,
      (unsigned)Result->Cycles, Result->Passed ? "PASS" : "FAIL");
  }
  (void)SEGGER_RTT_printf(0u, "HW-04|END|PASS=%u|TOTAL=%u|RESULT=%s\n",
                          Passed, (unsigned)HW04_CASE_COUNT,
                          (Passed == HW04_CASE_COUNT) ? "PASS" : "FAIL");
}
