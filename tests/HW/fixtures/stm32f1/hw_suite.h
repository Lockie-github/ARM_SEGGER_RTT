#ifndef HW_TEST_SUITE_STM32F1_H
#define HW_TEST_SUITE_STM32F1_H

#define HW01_MCU_NAME "STM32F103C8"

#if HW_TEST_CASE == 1
#include "../hw01_fixture.h"
#elif HW_TEST_CASE == 2
#define HW02_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW02_BUILD_TYPE HW_TEST_BUILD_NAME
#include "hw02_fixture.inc"
#elif HW_TEST_CASE == 3
#define HW03_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW03_BUILD_TYPE HW_TEST_BUILD_NAME
#define HW03_PROFILE HW_TEST_PROFILE
#include "hw03_fixture.inc"
#elif HW_TEST_CASE == 4
#define HW04_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW04_BUILD_TYPE HW_TEST_BUILD_NAME
#define HW04_SKIP_ASM HW_TEST_PROFILE
#include "hw04_fixture.inc"
#elif HW_TEST_CASE == 5
#define HW05_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW05_BUILD_TYPE HW_TEST_BUILD_NAME
#define HW05_MODFF ((HW_TEST_PROFILE >> 1) & 1)
#define HW05_FAST (HW_TEST_PROFILE & 1)
#include "hw05_fixture.inc"
#elif HW_TEST_CASE == 6
#define HW06_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW06_BUILD_TYPE HW_TEST_BUILD_NAME
#define HW06_UP_SIZE HW_TEST_UP_SIZE
#include "hw06_fixture.inc"
#elif HW_TEST_CASE == 7
#define HW07_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW07_BUILD_TYPE HW_TEST_BUILD_NAME
#include "hw07_fixture.inc"
#elif HW_TEST_CASE == 8
#define HW08_PROJECT_NAME HW_TEST_PROJECT_NAME
#define HW08_BUILD_TYPE HW_TEST_BUILD_NAME
#include "hw08_fixture.inc"
#else
#error "HW_TEST_CASE must be in the range 1..8"
#endif

#include "../hw_suite_lifecycle.h"

#endif
