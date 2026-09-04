#ifndef HW_TEST_SUITE_LIFECYCLE_H
#define HW_TEST_SUITE_LIFECYCLE_H

#if HW_TEST_CASE <= 4
unsigned __real_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes);

unsigned __wrap_SEGGER_RTT_Write(unsigned BufferIndex,
                                 const void *pBuffer,
                                 unsigned NumBytes)
{
  return __real_SEGGER_RTT_Write(BufferIndex, pBuffer, NumBytes);
}
#endif

static unsigned HW_TestCompleted;

static void HW_TestInit(void)
{
#if HW_TEST_CASE == 1
  HW01_Init();
#elif HW_TEST_CASE == 5
  SEGGER_RTT_Init();
  (void)SEGGER_RTT_SetFlagsUpBuffer(0u, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
#elif HW_TEST_CASE == 6
  SEGGER_RTT_Init();
  HW06_Init();
#elif HW_TEST_CASE == 7
  SEGGER_RTT_Init();
  HW07_Init();
#elif HW_TEST_CASE == 8
  SEGGER_RTT_Init();
  HW08_Init();
#else
  SEGGER_RTT_Init();
#endif
  HW_TestCompleted = 0u;
}

static void HW_TestRun(void)
{
#if HW_TEST_CASE == 1
  HW01_Run();
  HAL_Delay(500u);
#elif HW_TEST_CASE == 6
  HW06_Tick();
#else
  if (HW_TestCompleted == 0u) {
#if HW_TEST_CASE == 2
    HW02_Run();
#elif HW_TEST_CASE == 3
    HW03_Run();
#elif HW_TEST_CASE == 4
    HW04_Run();
#elif HW_TEST_CASE == 5
    HW05_Run();
#elif HW_TEST_CASE == 7
    HW07_Run();
#elif HW_TEST_CASE == 8
    HW08_Run();
#endif
    HW_TestCompleted = 1u;
  }
  HAL_Delay(1000u);
#endif
}

#endif
