#include "SEGGER_RTT.h"

#include <stdint.h>
#include <string.h>

#define HW08_TP_FRAME_SIZE       64u
#define HW08_TP_FRAME_COUNT      8192u
#define HW08_TP_RESULT_MAGIC     UINT32_C(0x48575450)
#define HW08_TP_RESULT_GUARD     UINT32_C(0xA55A2000)

#ifndef HW08_TP_BASE_TICKS
#error "HW08_TP_BASE_TICKS must be provided by the test runner"
#endif
#ifndef HW08_TP_REMAINDER_STEP
#error "HW08_TP_REMAINDER_STEP must be provided by the test runner"
#endif
#ifndef HW08_TP_REMAINDER_DENOM
#error "HW08_TP_REMAINDER_DENOM must be provided by the test runner"
#endif
#ifndef HW08_TP_RUN_ID
#error "HW08_TP_RUN_ID must be provided by the test runner"
#endif
#if HW08_TP_RUN_ID == 0
#error "HW08_TP_RUN_ID must be nonzero"
#endif
#if HW08_TP_REMAINDER_STEP >= HW08_TP_REMAINDER_DENOM
#error "HW08_TP_REMAINDER_STEP must be smaller than its denominator"
#endif

typedef struct {
  uint32_t Magic;
  uint32_t Version;
  uint32_t RunId;
  uint32_t TargetHz;
  uint32_t FrameSize;
  uint32_t Attempted;
  uint32_t Accepted;
  uint32_t Rejected;
  uint32_t AcceptedBytes;
  uint32_t Cycles;
  uint32_t StartRdOff;
  uint32_t StartWrOff;
  uint32_t EndRdOff;
  uint32_t EndWrOff;
  uint32_t ResetFlags;
  uint32_t Guard;
} HW08_ThroughputResult;

volatile HW08_ThroughputResult HW08_Result;

unsigned __real_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes);

unsigned __wrap_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes)
{
  return __real_SEGGER_RTT_Write(BufferIndex, pBuffer, NumBytes);
}

static uint32_t HW08_Crc32Update(uint32_t Crc, uint8_t Byte)
{
  unsigned Bit;

  Crc ^= Byte;
  for (Bit = 0u; Bit < 8u; ++Bit) {
    Crc = (Crc >> 1) ^ ((Crc & 1u) ? UINT32_C(0xEDB88320) : 0u);
  }
  return Crc;
}

static void HW08_WriteHex(char *Destination, uint32_t Value)
{
  static const char Hex[] = "0123456789ABCDEF";
  unsigned Index;

  for (Index = 0u; Index < 8u; ++Index) {
    Destination[7u - Index] = Hex[Value & 0xFu];
    Value >>= 4;
  }
}

static void HW08_MakeFrame(char Frame[HW08_TP_FRAME_SIZE], uint32_t Sequence)
{
  static const char Magic[] = "H8TP0002";
  uint32_t Crc = UINT32_C(0xFFFFFFFF);
  unsigned Index;

  memcpy(Frame, Magic, 8u);
  HW08_WriteHex(&Frame[8], (uint32_t)HW08_TP_RUN_ID);
  HW08_WriteHex(&Frame[16], Sequence);
  memset(&Frame[24], '0', 8u);
  for (Index = 32u; Index < 63u; ++Index) {
    Frame[Index] = (char)('A' + ((Sequence + Index) % 26u));
  }
  Frame[63] = '\n';

  for (Index = 0u; Index < 24u; ++Index) {
    Crc = HW08_Crc32Update(Crc, (uint8_t)Frame[Index]);
  }
  for (Index = 32u; Index < 63u; ++Index) {
    Crc = HW08_Crc32Update(Crc, (uint8_t)Frame[Index]);
  }
  HW08_WriteHex(&Frame[24], ~Crc);
}

static void HW08_Init(void)
{
  memset((void *)&HW08_Result, 0, sizeof(HW08_Result));
  (void)SEGGER_RTT_SetFlagsUpBuffer(0u, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}

static void HW08_Run(void)
{
  static unsigned HasRun;
  SEGGER_RTT_BUFFER_UP *Up;
  char Frame[HW08_TP_FRAME_SIZE];
  uint32_t Start;
  uint32_t Deadline;
  uint32_t End;
  unsigned Remainder = 0u;
  unsigned Accepted = 0u;
  unsigned Sequence;

  if (HasRun != 0u) {
    return;
  }
  HasRun = 1u;
  Up = &_SEGGER_RTT.aUp[0];

  /* Do not start until the attached host explicitly opens this run's gate. */
  while (HW08_Result.Magic != (uint32_t)HW08_TP_RUN_ID) {
  }
  HW08_Result.Magic = 0u;
  __DMB();

  /* Allow the host reader to attach and drain any startup traffic. */
  HAL_Delay(2000u);
  HW08_Result.StartRdOff = Up->RdOff;
  HW08_Result.StartWrOff = Up->WrOff;
  Start = HAL_GetTick();
  Deadline = Start;

  for (Sequence = 0u; Sequence < HW08_TP_FRAME_COUNT; ++Sequence) {
    HW08_MakeFrame(Frame, Sequence);
    Deadline += HW08_TP_BASE_TICKS;
    Remainder += HW08_TP_REMAINDER_STEP;
    if (Remainder >= HW08_TP_REMAINDER_DENOM) {
      Deadline++;
      Remainder -= HW08_TP_REMAINDER_DENOM;
    }
    while ((int32_t)(HAL_GetTick() - Deadline) < 0) {
    }
    if (SEGGER_RTT_Write(0u, Frame, sizeof(Frame)) == sizeof(Frame)) {
      Accepted++;
    }
  }
  End = HAL_GetTick();

  /* Keep the probe connected while the accepted data drains. */
  HAL_Delay(2000u);
  HW08_Result.Version = 2u;
  HW08_Result.RunId = (uint32_t)HW08_TP_RUN_ID;
  HW08_Result.TargetHz = 1000u;
  HW08_Result.FrameSize = HW08_TP_FRAME_SIZE;
  HW08_Result.Attempted = HW08_TP_FRAME_COUNT;
  HW08_Result.Accepted = Accepted;
  HW08_Result.Rejected = HW08_TP_FRAME_COUNT - Accepted;
  HW08_Result.AcceptedBytes = Accepted * HW08_TP_FRAME_SIZE;
  HW08_Result.Cycles = End - Start;
  HW08_Result.EndRdOff = Up->RdOff;
  HW08_Result.EndWrOff = Up->WrOff;
  HW08_Result.ResetFlags = RCC->CSR;
  HW08_Result.Guard = HW08_TP_RESULT_GUARD;
  __DMB();
  HW08_Result.Magic = HW08_TP_RESULT_MAGIC;
}
