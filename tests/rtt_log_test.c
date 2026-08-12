#include "rtt_log.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef RTT_TEST_CONFIG_OVERRIDE
_Static_assert(SEGGER_RTT_MAX_NUM_UP_BUFFERS == 1,
               "rtt_cfg.h must override the up-buffer count");
_Static_assert(SEGGER_RTT_MAX_NUM_DOWN_BUFFERS == 1,
               "rtt_cfg.h must override the down-buffer count");
_Static_assert(BUFFER_SIZE_UP == 257,
               "rtt_cfg.h must override the up-buffer size");
_Static_assert(BUFFER_SIZE_DOWN == 3,
               "rtt_cfg.h must override the down-buffer size");
_Static_assert(SEGGER_RTT_PRINTF_BUFFER_SIZE == 17u,
               "rtt_cfg.h must override the printf buffer size");
_Static_assert(SEGGER_RTT__CB_SIZE == 72,
               "one up and one down channel must use a 72-byte RTT control block");
#endif

static char Output[512];
static unsigned OutputLength;
static unsigned WriteCount;

unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void * pBuffer, unsigned NumBytes) {
  if (BufferIndex != RTT_LOG_BUFFER_INDEX) {
    return 0u;
  }
  if ((OutputLength + NumBytes) > sizeof(Output)) {
    return 0u;
  }
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  WriteCount++;
  return NumBytes;
}

static void reset_output(void) {
  memset(Output, 0, sizeof(Output));
  OutputLength = 0u;
  WriteCount = 0u;
}

static void expect_output(const char * Expected) {
  size_t ExpectedLength;

  ExpectedLength = strlen(Expected);
  if ((OutputLength != ExpectedLength) ||
      (memcmp(Output, Expected, ExpectedLength) != 0)) {
    fprintf(stderr, "expected: %s\nactual:   %.*s\n",
            Expected, (int)OutputLength, Output);
    exit(1);
  }
}

static void expect_level_output(int Enabled,
                                const char * PlainPrefix,
                                const char * ColorPrefix,
                                const char * Body) {
  char Expected[512];

  (void)PlainPrefix;
  (void)ColorPrefix;
  if (!RTT_LOG_ENABLE || !Enabled) {
    expect_output("");
    return;
  }
#if LOG_ENABLE_LITE
  snprintf(Expected, sizeof(Expected), "%s\n", Body);
#elif RTT_LOG_USE_COLOR
  if (ColorPrefix[0] != '\0') {
    snprintf(Expected, sizeof(Expected), "%s%s\x1B[0m\n", ColorPrefix, Body);
  } else {
    snprintf(Expected, sizeof(Expected), "%s\n", Body);
  }
#else
  snprintf(Expected, sizeof(Expected), "%s%s\n", PlainPrefix, Body);
#endif
  expect_output(Expected);
}

static void test_levels(void) {
  reset_output();
  log_info("value=%d", -12);
#if !RTT_LOG_ENABLE || !LOG_ENABLE_INFO
  expect_output("");
#elif LOG_ENABLE_LITE
  expect_output("value=-12\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;32m[INFO] value=-12\x1B[0m\n");
#else
  expect_output("[INFO] value=-12\n");
#endif

  reset_output();
  log_debug("hex=%08x", 0x2Au);
#if !RTT_LOG_ENABLE || !LOG_ENABLE_DEBUG
  expect_output("");
#elif LOG_ENABLE_LITE
  expect_output("hex=0000002A\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;34m[DEBUG] hex=0000002A\x1B[0m\n");
#else
  expect_output("[DEBUG] hex=0000002A\n");
#endif

  reset_output();
  log_warn("text=%s", "ready");
#if !RTT_LOG_ENABLE || !LOG_ENABLE_WARN
  expect_output("");
#elif LOG_ENABLE_LITE
  expect_output("text=ready\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;33m[WARN] text=ready\x1B[0m\n");
#else
  expect_output("[WARN] text=ready\n");
#endif

  reset_output();
  log_err("error=%u", 7u);
#if !RTT_LOG_ENABLE || !LOG_ENABLE_ERROR
  expect_output("");
#elif LOG_ENABLE_LITE
  expect_output("error=7\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;31m[ERROR] error=7\x1B[0m\n");
#else
  expect_output("[ERROR] error=7\n");
#endif
}

static void test_types_for_every_level(void) {
  static const char Body[] =
    "pos=42 neg=-42 zero=0 min=-2147483648 max=2147483647 unsigned=4294967295 "
    "hex=00002A lower=2A plus=+7 left=9     precision=0007 "
    "char=Z text=ready short=abc null=(NULL) percent=%";
  static const char Format[] =
    "pos=%d neg=%d zero=%d min=%d max=%d unsigned=%u "
    "hex=%06X lower=%x plus=%+d left=%-5d precision=%.4d "
    "char=%c text=%s short=%.3s null=%s percent=%%";

  reset_output();
  log_info(Format, 42, -42, 0, INT_MIN, INT_MAX, 4294967295u,
           0x2Au, 0x2Au, 7, 9, 7, 'Z', "ready", "abcdef", (const char *)NULL);
  expect_level_output(LOG_ENABLE_INFO, "[INFO] ",
                      "\x1B[1;32m[INFO] ", Body);

  reset_output();
  log_debug(Format, 42, -42, 0, INT_MIN, INT_MAX, 4294967295u,
            0x2Au, 0x2Au, 7, 9, 7, 'Z', "ready", "abcdef", (const char *)NULL);
  expect_level_output(LOG_ENABLE_DEBUG, "[DEBUG] ",
                      "\x1B[1;34m[DEBUG] ", Body);

  reset_output();
  log_warn(Format, 42, -42, 0, INT_MIN, INT_MAX, 4294967295u,
           0x2Au, 0x2Au, 7, 9, 7, 'Z', "ready", "abcdef", (const char *)NULL);
  expect_level_output(LOG_ENABLE_WARN, "[WARN] ",
                      "\x1B[1;33m[WARN] ", Body);

  reset_output();
  log_err(Format, 42, -42, 0, INT_MIN, INT_MAX, 4294967295u,
          0x2Au, 0x2Au, 7, 9, 7, 'Z', "ready", "abcdef", (const char *)NULL);
  expect_level_output(LOG_ENABLE_ERROR, "[ERROR] ",
                      "\x1B[1;31m[ERROR] ", Body);

  reset_output();
  log_print(Format, 42, -42, 0, INT_MIN, INT_MAX, 4294967295u,
            0x2Au, 0x2Au, 7, 9, 7, 'Z', "ready", "abcdef", (const char *)NULL);
  expect_level_output(LOG_ENABLE_PRINT, "", "", Body);

  (void)Format;
}

static void test_print_and_runtime_format(void) {
  const char * Format;

  reset_output();
  log_print("plain %% %c", 'A');
#if RTT_LOG_ENABLE && LOG_ENABLE_PRINT
  expect_output("plain % A\n");
#else
  expect_output("");
#endif

  Format = "runtime=%d";
  reset_output();
  log_info(Format, 23);
  (void)Format;
#if !RTT_LOG_ENABLE || !LOG_ENABLE_INFO
  expect_output("");
#elif LOG_ENABLE_LITE
  expect_output("runtime=23\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;32m[INFO] runtime=23\x1B[0m\n");
#else
  expect_output("[INFO] runtime=23\n");
#endif

  reset_output();
  if (SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "raw=%d", 9) != 5) {
    fprintf(stderr, "SEGGER_RTT_printf returned an unexpected count\n");
    exit(1);
  }
  expect_output("raw=9");

  reset_output();
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "direct=%d", 1) < 0) {
    fprintf(stderr, "RTT_LogPrintf returned an unexpected error\n");
    exit(1);
  }
#if !RTT_LOG_ENABLE
  expect_output("");
#elif LOG_ENABLE_LITE
  expect_output("direct=1\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;32m[INFO] direct=1\x1B[0m\n");
#else
  expect_output("[INFO] direct=1\n");
#endif
}

static void test_long_log(void) {
  char Message[97];
  char Expected[160];

  memset(Message, 'A', sizeof(Message) - 1u);
  Message[sizeof(Message) - 1u] = '\0';
#if !RTT_LOG_ENABLE || !LOG_ENABLE_INFO
  Expected[0] = '\0';
#elif LOG_ENABLE_LITE
  snprintf(Expected, sizeof(Expected), "%s\n", Message);
#elif RTT_LOG_USE_COLOR
  snprintf(Expected, sizeof(Expected), "\x1B[1;32m[INFO] %s\x1B[0m\n", Message);
#else
  snprintf(Expected, sizeof(Expected), "[INFO] %s\n", Message);
#endif

  reset_output();
  log_info("%s", Message);
  expect_output(Expected);
#if RTT_LOG_ENABLE && LOG_ENABLE_INFO
  if (WriteCount < 2u) {
    fprintf(stderr, "long log did not exercise multiple RTT writes\n");
    exit(1);
  }
#else
  if (WriteCount != 0u) {
    fprintf(stderr, "disabled log unexpectedly wrote output\n");
    exit(1);
  }
#endif
}

static void test_float(void) {
  union {
    uint32_t Bits;
    float Value;
  } Special;

  reset_output();
  log_float_desc("value", 1.25f);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("value: 1.250\n");
#else
  expect_output("");
#endif

  reset_output();
  log_float(2.5f);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("2.500\n");
#else
  expect_output("");
#endif

  reset_output();
  log_float_desc("negative", -2.5f);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("negative: -2.500\n");
#else
  expect_output("");
#endif

  reset_output();
  log_float_desc("zero", 0.0f);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("zero: 0.000\n");
#else
  expect_output("");
#endif

  reset_output();
  log_float_desc("truncate", 1.9996f);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("truncate: 1.999\n");
#else
  expect_output("");
#endif

  reset_output();
  log_float_desc("negative truncate", -1.9996f);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("negative truncate: -1.999\n");
#else
  expect_output("");
#endif

  Special.Bits = 0x4F000000u;
  reset_output();
  log_float_desc("above int max", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("above int max: 2147483648.000\n");
#else
  expect_output("");
#endif

  Special.Bits = 0xCF000000u;
  reset_output();
  log_float_desc("below int min", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("below int min: -2147483648.000\n");
#else
  expect_output("");
#endif

  Special.Bits = 0x4F800000u;
  reset_output();
  log_float_desc("overflow", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("overflow: Overflow\n");
#else
  expect_output("");
#endif

  Special.Bits = 0xCF800000u;
  reset_output();
  log_float_desc("negative overflow", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("negative overflow: -Overflow\n");
#else
  expect_output("");
#endif

  Special.Bits = 0x7FC00000u;
  reset_output();
  log_float_desc("nan", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("nan: NaN\n");
#else
  expect_output("");
#endif

  Special.Bits = 0x7F800000u;
  reset_output();
  log_float_desc("infinity", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("infinity: Inf\n");
#else
  expect_output("");
#endif

  Special.Bits = 0xFF800000u;
  reset_output();
  log_float_desc("negative infinity", Special.Value);
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  expect_output("negative infinity: -Inf\n");
#else
  expect_output("");
#endif
}

int main(void) {
  test_levels();
  test_types_for_every_level();
  test_print_and_runtime_format();
  test_long_log();
  test_float();
  return 0;
}
