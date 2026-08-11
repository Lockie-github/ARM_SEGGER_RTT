#include "rtt_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void test_levels(void) {
  reset_output();
  log_info("value=%d", -12);
#if LOG_ENABLE_LITE
  expect_output("value=-12\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;32m[INFO] value=-12\x1B[0m\n");
#else
  expect_output("[INFO] value=-12\n");
#endif

  reset_output();
  log_debug("hex=%08x", 0x2Au);
#if LOG_ENABLE_LITE
  expect_output("hex=0000002A\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;34m[DEBUG] hex=0000002A\x1B[0m\n");
#else
  expect_output("[DEBUG] hex=0000002A\n");
#endif

  reset_output();
  log_warn("text=%s", "ready");
#if LOG_ENABLE_LITE
  expect_output("text=ready\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;33m[WARN] text=ready\x1B[0m\n");
#else
  expect_output("[WARN] text=ready\n");
#endif

  reset_output();
  log_err("error=%u", 7u);
#if LOG_ENABLE_LITE
  expect_output("error=7\n");
#elif RTT_LOG_USE_COLOR
  expect_output("\x1B[1;31m[ERROR] error=7\x1B[0m\n");
#else
  expect_output("[ERROR] error=7\n");
#endif
}

static void test_print_and_runtime_format(void) {
  const char * Format;

  reset_output();
  log_print("plain %% %c", 'A');
  expect_output("plain % A\n");

  Format = "runtime=%d";
  reset_output();
  log_info(Format, 23);
#if LOG_ENABLE_LITE
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
}

static void test_long_log(void) {
  char Message[97];
  char Expected[160];

  memset(Message, 'A', sizeof(Message) - 1u);
  Message[sizeof(Message) - 1u] = '\0';
#if LOG_ENABLE_LITE
  snprintf(Expected, sizeof(Expected), "%s\n", Message);
#elif RTT_LOG_USE_COLOR
  snprintf(Expected, sizeof(Expected), "\x1B[1;32m[INFO] %s\x1B[0m\n", Message);
#else
  snprintf(Expected, sizeof(Expected), "[INFO] %s\n", Message);
#endif

  reset_output();
  log_info("%s", Message);
  expect_output(Expected);
  if (WriteCount < 2u) {
    fprintf(stderr, "long log did not exercise multiple RTT writes\n");
    exit(1);
  }
}

int main(void) {
  test_levels();
  test_print_and_runtime_format();
  test_long_log();
  return 0;
}
