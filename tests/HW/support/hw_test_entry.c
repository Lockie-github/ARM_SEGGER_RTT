#include "main.h"
#include "rtt_log.h"

#ifndef HW_TEST_LIBRARY_SHA
#error "HW test runner must provide HW_TEST_LIBRARY_SHA"
#endif

#if defined(HW_TEST_BUILD_RELEASE)
#define HW_TEST_BUILD_NAME "RELEASE"
#elif defined(HW_TEST_BUILD_DEBUG)
#define HW_TEST_BUILD_NAME "DEBUG"
#else
#error "CMake HW overlay must define the test build type"
#endif

#if defined(HW_TEST_PROJECT_MAKE)
#define HW_TEST_PROJECT_NAME "MAKE"
#else
#define HW_TEST_PROJECT_NAME "CMAKE"
#endif

#if HW_TEST_MCU_FAMILY == 0
#include "tests/HW/fixtures/stm32f0/hw_suite.h"
#elif HW_TEST_MCU_FAMILY == 1
#include "tests/HW/fixtures/stm32f1/hw_suite.h"
#elif HW_TEST_MCU_FAMILY == 4
#include "tests/HW/fixtures/stm32f4/hw_suite.h"
#elif HW_TEST_MCU_FAMILY == 7
#include "tests/HW/fixtures/stm32h7/hw_suite.h"
#else
#error "Unsupported CMake HW test MCU family"
#endif

#if HW_TEST_CASE == 7
void HW_TestProjectIRQHandler(void);

void HW_TEST_IRQ_HANDLER(void)
{
#if HW_TEST_MCU_FAMILY == 0
  HW07_ISR_Begin();
  HW_TestProjectIRQHandler();
  HW07_ISR_End();
#else
  HW07_ISR_Entry();
  HW_TestProjectIRQHandler();
#endif
}
#endif

__attribute__((noreturn))
void HW_TestEntry(void)
{
  HW_TestInit();
  for (;;) {
    HW_TestRun();
  }
}
