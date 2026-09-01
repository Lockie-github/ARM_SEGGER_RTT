#ifndef HW02_FIXTURE_H
#define HW02_FIXTURE_H

#include "rtt_log.h"

#include <stdint.h>
#include <string.h>

#define HW02_LIBRARY_SHA HW_TEST_LIBRARY_SHA

static float HW02_FloatFromBits(uint32_t Bits)
{
  float Value;

  memcpy(&Value, &Bits, sizeof(Value));
  return Value;
}

static void HW02_CaseBoundary(unsigned int Case, const char * Api, const char * State)
{
  log_print("HW-02|CASE|%02u|API=%s|%s\n", Case, Api, State);
  HAL_Delay(5u);
}

static void HW02_Run(void)
{
  SEGGER_RTT_Init();
  log_print("HW-02|BEGIN|MCU=STM32F042G6|PROJECT=%s|BUILD=%s|SHA=%s|CASES=13\n",
            HW02_PROJECT_NAME, HW02_BUILD_NAME, HW02_LIBRARY_SHA);

  HW02_CaseBoundary(1u, "log_info", "BEGIN");
  log_info("signed=%d", -42);
  HW02_CaseBoundary(1u, "log_info", "END");

  HW02_CaseBoundary(2u, "log_debug", "BEGIN");
  log_debug("hex=%08X", 0x12AB34CDu);
  HW02_CaseBoundary(2u, "log_debug", "END");

  HW02_CaseBoundary(3u, "log_warn", "BEGIN");
  log_warn("text=%s", "warning");
  HW02_CaseBoundary(3u, "log_warn", "END");

  HW02_CaseBoundary(4u, "log_err", "BEGIN");
  log_err("code=%u", 4294967295u);
  HW02_CaseBoundary(4u, "log_err", "END");

  HW02_CaseBoundary(5u, "log_print", "BEGIN");
  log_print("plain=-17/23\n");
  HW02_CaseBoundary(5u, "log_print", "END");

  HW02_CaseBoundary(6u, "log_string", "BEGIN");
  (void)log_string("raw-string\n");
  HW02_CaseBoundary(6u, "log_string", "END");

  HW02_CaseBoundary(7u, "log_float", "BEGIN");
  log_float(1.25f);
  HAL_Delay(5u);
  log_float(HW02_FloatFromBits(0x7FC00000u));
  HAL_Delay(5u);
  log_float(HW02_FloatFromBits(0x7F800000u));
  HAL_Delay(5u);
  log_float(HW02_FloatFromBits(0xFF800000u));
  HW02_CaseBoundary(7u, "log_float", "END");

  HW02_CaseBoundary(8u, "log_float_label", "BEGIN");
  log_float_label("temperature", -2.5f);
  HAL_Delay(5u);
  log_float_label(NULL, HW02_FloatFromBits(0x80000000u));
  HW02_CaseBoundary(8u, "log_float_label", "END");

  HW02_CaseBoundary(9u, "log_i32", "BEGIN");
  log_i32("i32_min", INT32_MIN);
  HW02_CaseBoundary(9u, "log_i32", "END");

  HW02_CaseBoundary(10u, "log_u32", "BEGIN");
  log_u32("", UINT32_MAX);
  HW02_CaseBoundary(10u, "log_u32", "END");

  HW02_CaseBoundary(11u, "log_hex32", "BEGIN");
  log_hex32(NULL, 0x89ABCDEFu);
  HW02_CaseBoundary(11u, "log_hex32", "END");

  HW02_CaseBoundary(12u, "log_pointer", "BEGIN");
  log_pointer("pointer", (const void *)(uintptr_t)0x20000000u);
  HW02_CaseBoundary(12u, "log_pointer", "END");

  HW02_CaseBoundary(13u, "log_f32", "BEGIN");
  log_f32("voltage", 3.3f);
  HAL_Delay(5u);
  log_f32("", HW02_FloatFromBits(0x7FC00000u));
  HAL_Delay(5u);
  log_f32(NULL, HW02_FloatFromBits(0xFF800000u));
  HW02_CaseBoundary(13u, "log_f32", "END");

  log_print("HW-02|END|PROJECT=%s|BUILD=%s|CASES=13\n",
            HW02_PROJECT_NAME, HW02_BUILD_NAME);
}

#endif
