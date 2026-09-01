#include "SEGGER_RTT.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char Output[4096];
static unsigned OutputLength;
static unsigned WriteCalls;
static unsigned Cases;

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  if ((BufferIndex != 0u) || (NumBytes > (sizeof(Output) - OutputLength))) {
    return 0u;
  }
  WriteCalls++;
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  return NumBytes;
}

static void reset_capture(void) {
  memset(Output, 0, sizeof(Output));
  OutputLength = 0u;
  WriteCalls = 0u;
}

static void check_case(const char * Name,
                       int Result,
                       const char * Expected,
                       unsigned ExpectedWrites) {
  size_t ExpectedLength;

  ExpectedLength = strlen(Expected);
  if (Result != (int)ExpectedLength) {
    fprintf(stderr, "NH04 %s: return=%d expected=%zu\n",
            Name, Result, ExpectedLength);
    exit(1);
  }
  if ((OutputLength != ExpectedLength) ||
      (memcmp(Output, Expected, ExpectedLength) != 0)) {
    fprintf(stderr, "NH04 %s: output bytes differ\n", Name);
    exit(1);
  }
  if (WriteCalls != ExpectedWrites) {
    fprintf(stderr, "NH04 %s: writes=%u expected=%u\n",
            Name, WriteCalls, ExpectedWrites);
    exit(1);
  }
  Cases++;
}

#define CHECK_CALL(Name, Expected, ExpectedWrites, Call) \
  do {                                                    \
    int TestResult;                                       \
    reset_capture();                                      \
    TestResult = (Call);                                  \
    check_case((Name), TestResult, (Expected), (ExpectedWrites)); \
  } while (0)

#ifndef NH04_SMALL_BUFFER_ONLY

static void test_conversions_and_values(void) {
  static int Marker;
  char PointerExpected[(sizeof(uintptr_t) * 2u) + 1u];
  int Result;

  CHECK_CALL("char", "Z", 1u, SEGGER_RTT_printf(0u, "%c", 'Z'));
  CHECK_CALL("d/zero", "0", 1u, SEGGER_RTT_printf(0u, "%d", 0));
  CHECK_CALL("d/positive", "42", 1u, SEGGER_RTT_printf(0u, "%d", 42));
  CHECK_CALL("d/negative", "-42", 1u, SEGGER_RTT_printf(0u, "%d", -42));
  CHECK_CALL("d/min", "-2147483648", 1u,
             SEGGER_RTT_printf(0u, "%d", INT_MIN));
  CHECK_CALL("d/max", "2147483647", 1u,
             SEGGER_RTT_printf(0u, "%d", INT_MAX));
  CHECK_CALL("u/zero", "0", 1u, SEGGER_RTT_printf(0u, "%u", 0u));
  CHECK_CALL("u/max", "4294967295", 1u,
             SEGGER_RTT_printf(0u, "%u", UINT_MAX));
  CHECK_CALL("hex/zero", "0/0", 1u,
             SEGGER_RTT_printf(0u, "%x/%X", 0u, 0u));
  CHECK_CALL("hex/value", "12AB/12AB", 1u,
             SEGGER_RTT_printf(0u, "%x/%X", 0x12ABu, 0x12ABu));
  CHECK_CALL("string", "ready", 1u,
             SEGGER_RTT_printf(0u, "%s", "ready"));
  CHECK_CALL("string/empty", "", 0u,
             SEGGER_RTT_printf(0u, "%s", ""));
  CHECK_CALL("string/null", "(NULL)", 1u,
             SEGGER_RTT_printf(0u, "%s", (const char *)NULL));
  CHECK_CALL("percent", "A %", 1u,
             SEGGER_RTT_printf(0u, "%c %%", 'A'));

  (void)snprintf(PointerExpected, sizeof(PointerExpected), "%0*lX",
                 (int)(sizeof(uintptr_t) * 2u),
                 (unsigned long)(uintptr_t)&Marker);
  reset_capture();
  Result = SEGGER_RTT_printf(0u, "%p", (void *)&Marker);
  check_case("pointer", Result, PointerExpected, 1u);
}

static void test_width_and_flags(void) {
  CHECK_CALL("width/zero", "7", 1u,
             SEGGER_RTT_printf(0u, "%0d", 7));
  CHECK_CALL("width/exact", "42", 1u,
             SEGGER_RTT_printf(0u, "%2d", 42));
  CHECK_CALL("width/right", "   42", 1u,
             SEGGER_RTT_printf(0u, "%5d", 42));
  CHECK_CALL("width/left", "42   ", 1u,
             SEGGER_RTT_printf(0u, "%-5d", 42));
  CHECK_CALL("width/zero-pad", "00042", 1u,
             SEGGER_RTT_printf(0u, "%05d", 42));
  CHECK_CALL("sign", "+42", 1u,
             SEGGER_RTT_printf(0u, "%+d", 42));
  CHECK_CALL("sign/zero-pad", "+0042", 1u,
             SEGGER_RTT_printf(0u, "%+05d", 42));
  CHECK_CALL("sign/left", "+42  ", 1u,
             SEGGER_RTT_printf(0u, "%-+5d", 42));
}

static void test_precision(void) {
  CHECK_CALL("precision/d", "00042", 1u,
             SEGGER_RTT_printf(0u, "%.5d", 42));
  CHECK_CALL("precision/negative", "-00042", 1u,
             SEGGER_RTT_printf(0u, "%.5d", -42));
  CHECK_CALL("precision/overrides-zero", "   00042", 1u,
             SEGGER_RTT_printf(0u, "%08.5d", 42));
  CHECK_CALL("precision/u", "007", 1u,
             SEGGER_RTT_printf(0u, "%.3u", 7u));
  CHECK_CALL("precision/x", "002A", 1u,
             SEGGER_RTT_printf(0u, "%.4x", 0x2Au));
  CHECK_CALL("precision/string", "abc", 1u,
             SEGGER_RTT_printf(0u, "%.3s", "abcdef"));
  CHECK_CALL("precision/dynamic", "abc", 1u,
             SEGGER_RTT_printf(0u, "%.*s", 3, "abcdef"));
  CHECK_CALL("precision/dynamic-negative", "abcdef", 1u,
             SEGGER_RTT_printf(0u, "%.*s", -1, "abcdef"));
  CHECK_CALL("precision/string-zero", "", 0u,
             SEGGER_RTT_printf(0u, "%.0s", "abcdef"));
}

static void test_runtime_and_invalid_formats(void) {
  const char * RuntimeFormat;

  RuntimeFormat = "runtime=%d";
  CHECK_CALL("runtime", "runtime=23", 1u,
             SEGGER_RTT_printf(0u, RuntimeFormat, 23));
  CHECK_CALL("unsupported/no-consume", "bad=%q next=77", 1u,
             SEGGER_RTT_printf(0u, "bad=%q next=%d", 77));
  CHECK_CALL("unsupported/group", "%ld/%f/%#x", 1u,
             SEGGER_RTT_printf(0u, "%ld/%f/%#x", 42L, 1.25, 0x2Au));
  CHECK_CALL("unsupported/dynamic-precision", "%.*q/77", 1u,
             SEGGER_RTT_printf(0u, "%.*q/%d", 3, 77));
  CHECK_CALL("trailing-percent", "tail%", 1u,
             SEGGER_RTT_printf(0u, "tail%"));
}

#else

static void test_small_buffer_segments(void) {
  char NumberExpected[26];
  static const char Literal[] = "abcdefghijklmnopqrst";
  static const char StringValue[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  memset(NumberExpected, '0', sizeof(NumberExpected) - 1u);
  NumberExpected[sizeof(NumberExpected) - 2u] = '7';
  NumberExpected[sizeof(NumberExpected) - 1u] = '\0';

  CHECK_CALL("segment/literal", Literal, 3u,
             SEGGER_RTT_printf(0u, Literal));
  CHECK_CALL("segment/number-padding", NumberExpected, 4u,
             SEGGER_RTT_printf(0u, "%025u", 7u));
  CHECK_CALL("segment/string", StringValue, 4u,
             SEGGER_RTT_printf(0u, "%s", StringValue));
}

#endif

int main(void) {
#ifndef NH04_SMALL_BUFFER_ONLY
  test_conversions_and_values();
  test_width_and_flags();
  test_precision();
  test_runtime_and_invalid_formats();
  printf("NH04 PASS current: cases=%u\n", Cases);
#else
  test_small_buffer_segments();
  printf("NH04 PASS buffer-8: cases=%u writes=3/4/4\n", Cases);
#endif
  return 0;
}

#undef CHECK_CALL
