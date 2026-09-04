#ifndef HW03_FIXTURE_H
#define HW03_FIXTURE_H

#include "rtt_log.h"

#include <stdint.h>

#define HW03_LIBRARY_SHA HW_TEST_LIBRARY_SHA

#if HW03_PROFILE == 0
#define HW03_PROFILE_NAME "DEFAULT"
#define HW03_CHANNEL_TEXT "0"
#define HW03_EXPECTED 9u
#define HW03_EXPECTED_TEXT "9"
#elif HW03_PROFILE == 1
#define HW03_PROFILE_NAME "NO_COLOR"
#define HW03_CHANNEL_TEXT "0"
#define HW03_EXPECTED 9u
#define HW03_EXPECTED_TEXT "9"
#elif HW03_PROFILE == 2
#define HW03_PROFILE_NAME "LITE"
#define HW03_CHANNEL_TEXT "0"
#define HW03_EXPECTED 9u
#define HW03_EXPECTED_TEXT "9"
#elif HW03_PROFILE == 3
#define HW03_PROFILE_NAME "TYPED_ONLY"
#define HW03_CHANNEL_TEXT "0"
#define HW03_EXPECTED 8u
#define HW03_EXPECTED_TEXT "8"
#elif HW03_PROFILE == 4
#define HW03_PROFILE_NAME "TYPED_FLOAT_ONLY"
#define HW03_CHANNEL_TEXT "0"
#define HW03_EXPECTED 2u
#define HW03_EXPECTED_TEXT "2"
#elif HW03_PROFILE == 5
#define HW03_PROFILE_NAME "TYPED_COMBO"
#define HW03_CHANNEL_TEXT "0"
#define HW03_EXPECTED 10u
#define HW03_EXPECTED_TEXT "10"
#elif HW03_PROFILE == 6
#define HW03_PROFILE_NAME "CHANNEL1"
#define HW03_CHANNEL_TEXT "1"
#define HW03_EXPECTED 19u
#define HW03_EXPECTED_TEXT "19"
#else
#error "Unsupported HW03_PROFILE"
#endif

static unsigned int HW03_SideEffects;

#if HW03_PROFILE == 6
static char HW03_Channel1Buffer[1024];
#endif

static int __attribute__((unused)) HW03_Value(void)
{
  ++HW03_SideEffects;
  return 1;
}

static const char * __attribute__((unused)) HW03_Label(void)
{
  ++HW03_SideEffects;
  return "SIDE";
}

static __attribute__((unused)) const char * HW03_Text(void)
{
  ++HW03_SideEffects;
  return "SIDE|log_string\n";
}

static void HW03_Emit(const char * pText)
{
  (void)SEGGER_RTT_WriteString(RTT_LOG_BUFFER_INDEX, pText);
}

static void HW03_Run(void)
{
  SEGGER_RTT_Init();
#if HW03_PROFILE == 6
  if (SEGGER_RTT_ConfigUpBuffer(1u, "HW03_CH1", HW03_Channel1Buffer,
                                sizeof(HW03_Channel1Buffer),
                                SEGGER_RTT_MODE_NO_BLOCK_SKIP) < 0) {
    (void)SEGGER_RTT_WriteString(0u, "HW-03|CHANNEL_CONFIG=FAIL\n");
    return;
  }
#endif

  HW03_Emit("HW-03|BEGIN|STM32F042G6|" HW03_PROJECT_NAME "|"
            HW03_BUILD_NAME "|" HW03_PROFILE_NAME "|CH=" HW03_CHANNEL_TEXT
            "|SHA=" HW03_LIBRARY_SHA "\n");

  log_info("API|log_info");
  log_debug("API|log_debug");
  log_warn("API|log_warn");
  log_err("API|log_err");
  log_print("API|log_print\n");
  (void)log_string("API|log_string\n");
  log_float(1.25f);
  log_float_label("API|log_float_label", -2.5f);
  log_i32("API|log_i32", -1);
  log_u32("API|log_u32", 1u);
  log_hex32("API|log_hex32", UINT32_C(0x89ABCDEF));
  log_pointer("API|log_pointer", (const void *)(uintptr_t)1u);
  log_f32("API|log_f32", 1.25f);

  log_info("SIDE|log_info|%d", HW03_Value());
  log_debug("SIDE|log_debug|%d", HW03_Value());
  log_warn("SIDE|log_warn|%d", HW03_Value());
  log_err("SIDE|log_err|%d", HW03_Value());
  log_print("SIDE|log_print|%d\n", HW03_Value());
  (void)log_string(HW03_Text());
  log_float((float)HW03_Value());
  log_float_label(HW03_Label(), (float)HW03_Value());
  log_i32(HW03_Label(), HW03_Value());
  log_u32(HW03_Label(), HW03_Value());
  log_hex32(HW03_Label(), HW03_Value());
  log_pointer(HW03_Label(), (const void *)(uintptr_t)HW03_Value());
  log_f32(HW03_Label(), (float)HW03_Value());

  if (HW03_SideEffects == HW03_EXPECTED) {
    HW03_Emit("HW-03|END|SIDE_EFFECT=PASS|EXPECTED=" HW03_EXPECTED_TEXT "\n");
  } else {
    HW03_Emit("HW-03|END|SIDE_EFFECT=FAIL\n");
  }
}

#endif
