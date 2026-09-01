#include "SEGGER_RTT.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char Output[4096];
static unsigned OutputLength;
static unsigned WriteCalls;
static unsigned ReturnMismatches;

unsigned SEGGER_RTT_Write(unsigned BufferIndex, const void * pBuffer, unsigned NumBytes) {
  (void)BufferIndex;
  if ((OutputLength + NumBytes) > sizeof(Output)) {
    return 0u;
  }
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  WriteCalls++;
  return NumBytes;
}

static void reset_output(void) {
  memset(Output, 0, sizeof(Output));
  OutputLength = 0u;
  WriteCalls = 0u;
}

static void check_result(const char * Name, int Actual, int Expected) {
  if (Actual != Expected) {
    fprintf(stderr, "return mismatch: %s actual=%d expected=%d\n",
            Name, Actual, Expected);
    ReturnMismatches++;
  }
}

static void expect_output(const char * Expected) {
  size_t Length;

  Length = strlen(Expected);
  if ((OutputLength != Length) || (memcmp(Output, Expected, Length) != 0)) {
    fprintf(stderr, "expected: %s\nactual:   %.*s\n",
            Expected, (int)OutputLength, Output);
    exit(1);
  }
}

static void test_common_formats(void) {
  reset_output();
  check_result("INT_MIN", SEGGER_RTT_printf(0u, "%d", INT_MIN), 11);
  expect_output("-2147483648");

  reset_output();
  check_result("INT_MAX", SEGGER_RTT_printf(0u, "%d", INT_MAX), 10);
  expect_output("2147483647");

  reset_output();
  check_result("UINT_MAX", SEGGER_RTT_printf(0u, "%u", UINT_MAX), 10);
  expect_output("4294967295");

  reset_output();
  check_result("hex", SEGGER_RTT_printf(0u, "%x/%X", 0x1234ABCDu, 0x1234ABCDu), 17);
  expect_output("1234ABCD/1234ABCD");

  reset_output();
  check_result("width", SEGGER_RTT_printf(0u, "%08x", 0x2Au), 8);
  expect_output("0000002A");

  reset_output();
  check_result("left", SEGGER_RTT_printf(0u, "%-8d", 42), 8);
  expect_output("42      ");

  reset_output();
  check_result("precision", SEGGER_RTT_printf(0u, "%.5d", 42), 5);
  expect_output("00042");

  reset_output();
  check_result("dynamic string", SEGGER_RTT_printf(0u, "%.*s", 3, "abcdef"), 3);
  expect_output("abc");

  reset_output();
  check_result("null string", SEGGER_RTT_printf(0u, "%s", (const char *)NULL), 6);
  expect_output("(NULL)");

  reset_output();
  check_result("char percent", SEGGER_RTT_printf(0u, "%c %%", 'A'), 3);
  expect_output("A %");
}

static void test_long_message(void) {
  char Message[513];

  memset(Message, 'A', sizeof(Message) - 1u);
  Message[sizeof(Message) - 1u] = '\0';
  reset_output();
  check_result("long message", SEGGER_RTT_printf(0u, "%s", Message), 512);
  expect_output(Message);
  if (WriteCalls < 2u) { fprintf(stderr, "long writes\n"); exit(1); }
}

static void test_known_compatibility_differences(void) {
  reset_output();
  (void)SEGGER_RTT_printf(0u, "%ld", 42L);
#if defined(RTT_COMPARE_RELEASE)
  expect_output("42");
#else
  expect_output("%ld");
#endif

  reset_output();
  (void)SEGGER_RTT_printf(0u, "%lu", 42UL);
#if defined(RTT_COMPARE_RELEASE)
  expect_output("42");
#else
  expect_output("%lu");
#endif

  reset_output();
  (void)SEGGER_RTT_printf(0u, "%hd", 42);
#if defined(RTT_COMPARE_RELEASE)
  expect_output("42");
#else
  expect_output("%hd");
#endif

  reset_output();
  (void)SEGGER_RTT_printf(0u, "%#x", 0x2Au);
#if defined(RTT_COMPARE_RELEASE)
  expect_output("2A");
#else
  expect_output("%#x");
#endif

  reset_output();
  (void)SEGGER_RTT_printf(0u, "%f", 1.25);
#if defined(RTT_COMPARE_RELEASE)
  expect_output("");
#else
  expect_output("%f");
#endif
}

int main(void) {
  test_common_formats();
  test_long_message();
  test_known_compatibility_differences();
  printf("return_mismatches=%u\n", ReturnMismatches);
  return 0;
}
