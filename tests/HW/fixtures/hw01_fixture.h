#ifndef HW01_FIXTURE_H
#define HW01_FIXTURE_H

#ifndef HW01_MCU_NAME
#error "HW01_MCU_NAME must identify the target MCU"
#endif

static uint32_t HW01_Heartbeat;

static void HW01_Init(void)
{
  SEGGER_RTT_Init();
  HW01_Heartbeat = 0u;
}

static void HW01_Run(void)
{
  ++HW01_Heartbeat;
  log_print("HW-01|%s|%s|%s|SHA=%s|HEARTBEAT=%u\n",
            HW01_MCU_NAME, HW_TEST_PROJECT_NAME, HW_TEST_BUILD_NAME,
            HW_TEST_LIBRARY_SHA, (unsigned)HW01_Heartbeat);
}

#endif
