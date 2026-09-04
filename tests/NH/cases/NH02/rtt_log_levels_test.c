#include "rtt_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  unsigned BufferIndex;
  unsigned Offset;
  unsigned Length;
} WRITE_RECORD;

static unsigned char Output[2048];
static unsigned OutputLength;
static WRITE_RECORD Writes[32];
static unsigned WriteCount;

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  if ((WriteCount >= (sizeof(Writes) / sizeof(Writes[0]))) ||
      (NumBytes > (sizeof(Output) - OutputLength))) {
    return 0u;
  }
  Writes[WriteCount].BufferIndex = BufferIndex;
  Writes[WriteCount].Offset = OutputLength;
  Writes[WriteCount].Length = NumBytes;
  WriteCount++;
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  return NumBytes;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  if (pText == NULL) {
    return 0u;
  }
  return SEGGER_RTT_Write(BufferIndex, pText, (unsigned)strlen(pText));
}

static void reset_capture(void) {
  memset(Output, 0, sizeof(Output));
  memset(Writes, 0, sizeof(Writes));
  OutputLength = 0u;
  WriteCount = 0u;
}

static void fail_case(const char * Name, const char * Reason) {
  fprintf(stderr, "NH02 %s failed: %s\n", Name, Reason);
  exit(1);
}

static void expect_capture(const char * Name,
                           const char * Expected,
                           unsigned ExpectedWrites) {
  size_t ExpectedLength;
  unsigned Index;

  ExpectedLength = strlen(Expected);
  if ((OutputLength != ExpectedLength) ||
      (memcmp(Output, Expected, ExpectedLength) != 0)) {
    fail_case(Name, "output bytes differ");
  }
  if (WriteCount != ExpectedWrites) {
    fail_case(Name, "unexpected RTT write count");
  }
  for (Index = 0u; Index < WriteCount; Index++) {
    if (Writes[Index].BufferIndex != RTT_LOG_BUFFER_INDEX) {
      fail_case(Name, "write used the wrong RTT channel");
    }
    if ((Writes[Index].Length == 0u) ||
        (Writes[Index].Offset + Writes[Index].Length > OutputLength)) {
      fail_case(Name, "invalid RTT write record");
    }
  }
}

static void test_info(void) {
  reset_capture();
  log_info("ready");
  expect_capture("info/plain", "\x1B[1;32m[INFO] ready\x1B[0m\n", 1u);

  reset_capture();
  log_info("");
  expect_capture("info/empty", "\x1B[1;32m[INFO] \x1B[0m\n", 1u);

  reset_capture();
  log_info("value=%d", -12);
  expect_capture("info/formatted", "\x1B[1;32m[INFO] value=-12\x1B[0m\n", 1u);
}

static void test_debug(void) {
  reset_capture();
  log_debug("ready");
  expect_capture("debug/plain", "\x1B[1;34m[DEBUG] ready\x1B[0m\n", 1u);

  reset_capture();
  log_debug("");
  expect_capture("debug/empty", "\x1B[1;34m[DEBUG] \x1B[0m\n", 1u);

  reset_capture();
  log_debug("hex=%08x", 0x2Au);
  expect_capture("debug/formatted", "\x1B[1;34m[DEBUG] hex=0000002A\x1B[0m\n", 1u);
}

static void test_warn(void) {
  reset_capture();
  log_warn("ready");
  expect_capture("warn/plain", "\x1B[1;33m[WARN] ready\x1B[0m\n", 1u);

  reset_capture();
  log_warn("");
  expect_capture("warn/empty", "\x1B[1;33m[WARN] \x1B[0m\n", 1u);

  reset_capture();
  log_warn("text=%s", "ready");
  expect_capture("warn/formatted", "\x1B[1;33m[WARN] text=ready\x1B[0m\n", 1u);
}

static void test_error(void) {
  reset_capture();
  log_err("ready");
  expect_capture("error/plain", "\x1B[1;31m[ERROR] ready\x1B[0m\n", 1u);

  reset_capture();
  log_err("");
  expect_capture("error/empty", "\x1B[1;31m[ERROR] \x1B[0m\n", 1u);

  reset_capture();
  log_err("error=%u", 7u);
  expect_capture("error/formatted", "\x1B[1;31m[ERROR] error=7\x1B[0m\n", 1u);
}

static void test_consecutive_levels(void) {
  static const char Expected[] =
    "\x1B[1;32m[INFO] one\x1B[0m\n"
    "\x1B[1;34m[DEBUG] two\x1B[0m\n"
    "\x1B[1;33m[WARN] three\x1B[0m\n"
    "\x1B[1;31m[ERROR] four\x1B[0m\n";

  reset_capture();
  log_info("one");
  log_debug("two");
  log_warn("three");
  log_err("four");
  expect_capture("consecutive", Expected, 4u);
}

static void test_return_values(void) {
  static const RTT_LOG_LEVEL Levels[] = {
    RTT_LOG_LEVEL_INFO,
    RTT_LOG_LEVEL_DEBUG,
    RTT_LOG_LEVEL_WARN,
    RTT_LOG_LEVEL_ERROR
  };
  static const char * const Expected[] = {
    "\x1B[1;32m[INFO] ret=0\x1B[0m\n",
    "\x1B[1;34m[DEBUG] ret=1\x1B[0m\n",
    "\x1B[1;33m[WARN] ret=2\x1B[0m\n",
    "\x1B[1;31m[ERROR] ret=3\x1B[0m\n"
  };
  unsigned Index;

  for (Index = 0u; Index < (sizeof(Levels) / sizeof(Levels[0])); Index++) {
    int Result;

    reset_capture();
    Result = RTT_LogPrintf(Levels[Index], "ret=%u", Index);
    if (Result != (int)strlen(Expected[Index])) {
      fail_case("return", "formatted byte count differs");
    }
    expect_capture("return", Expected[Index], 1u);
  }
}

int main(void) {
  test_info();
  test_debug();
  test_warn();
  test_error();
  test_consecutive_levels();
  test_return_values();
  puts("NH02 PASS: 12 level cases, 1 consecutive sequence, 4 return paths");
  return 0;
}
