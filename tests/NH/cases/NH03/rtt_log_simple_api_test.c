#include "rtt_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char Output[1024];
static unsigned OutputLength;
static unsigned WriteCount;
static unsigned WriteStringCount;
static unsigned SideEffects;

#if defined(__GNUC__) || defined(__clang__)
  #define TEST_UNUSED __attribute__((unused))
#else
  #define TEST_UNUSED
#endif

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  if ((BufferIndex != RTT_LOG_BUFFER_INDEX) ||
      (NumBytes > (sizeof(Output) - OutputLength))) {
    return 0u;
  }
  WriteCount++;
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  return NumBytes;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  size_t Length;

  WriteStringCount++;
  Length = strlen(pText);
  return SEGGER_RTT_Write(BufferIndex, pText, (unsigned)Length);
}

static void reset_capture(void) {
  memset(Output, 0, sizeof(Output));
  OutputLength = 0u;
  WriteCount = 0u;
  WriteStringCount = 0u;
}

static void fail_case(const char * Name, const char * Reason) {
  fprintf(stderr, "NH03 %s failed: %s\n", Name, Reason);
  exit(1);
}

static void expect_capture(const char * Name,
                           const char * Expected,
                           unsigned ExpectedWrites,
                           unsigned ExpectedStringWrites) {
  size_t ExpectedLength;

  ExpectedLength = strlen(Expected);
  if ((OutputLength != ExpectedLength) ||
      (memcmp(Output, Expected, ExpectedLength) != 0)) {
    fail_case(Name, "output bytes differ");
  }
  if (WriteCount != ExpectedWrites) {
    fail_case(Name, "unexpected RTT write count");
  }
  if (WriteStringCount != ExpectedStringWrites) {
    fail_case(Name, "unexpected RTT WriteString count");
  }
}

static TEST_UNUSED int side_effect_int(void) {
  SideEffects++;
  return 7;
}

static TEST_UNUSED const char * side_effect_format(void) {
  SideEffects++;
  return "value=%d";
}

static TEST_UNUSED const char * side_effect_text(void) {
  SideEffects++;
  return "text";
}

#if LOG_ENABLE_PRINT
static void test_print_enabled(void) {
  reset_capture();
  log_print("plain text");
  expect_capture("print/plain", "plain text", 1u, 0u);

  reset_capture();
  log_print("value=%d hex=%X", -12, 0x2Au);
  expect_capture("print/formatted", "value=-12 hex=2A", 1u, 0u);

  reset_capture();
  log_print("load=100%%");
  expect_capture("print/percent", "load=100%", 1u, 0u);

  reset_capture();
  log_print("caller newline\n");
  expect_capture("print/newline", "caller newline\n", 1u, 0u);
}
#else
static void test_print_disabled(void) {
  reset_capture();
  log_print("plain text");
  log_print("value=%d hex=%X", -12, 0x2Au);
  log_print("load=100%%");
  log_print("caller newline\n");
  SideEffects = 0u;
  log_print(side_effect_format(), side_effect_int());
  if (SideEffects != 0u) {
    fail_case("print/disabled", "argument expression was evaluated");
  }
  expect_capture("print/disabled", "", 0u, 0u);
}
#endif

#if LOG_ENABLE_STRING
static void expect_string(const char * Name,
                          const char * Text,
                          const char * Expected) {
  int Result;

  reset_capture();
  Result = log_string(Text);
  if (Result != (int)strlen(Expected)) {
    fail_case(Name, "return value differs from written length");
  }
  expect_capture(Name, Expected, 1u, 1u);
}

static void test_string_enabled(void) {
  expect_string("string/plain", "raw text", "raw text");
  expect_string("string/empty", "", "");
  expect_string("string/newline", "line 1\nline 2", "line 1\nline 2");
  expect_string("string/percent", "rate=%d 100%", "rate=%d 100%");

  reset_capture();
  if (log_string(NULL) != -1) {
    fail_case("string/null", "NULL did not return -1");
  }
  expect_capture("string/null", "", 0u, 0u);
}
#else
static void test_string_disabled(void) {
  int Result;

  reset_capture();
  Result = log_string("raw text");
  Result |= log_string("");
  Result |= log_string("line 1\nline 2");
  Result |= log_string("rate=%d 100%");
  Result |= log_string(NULL);
  SideEffects = 0u;
  Result |= log_string(side_effect_text());
  if (Result != 0) {
    fail_case("string/disabled", "disabled API did not return 0");
  }
  if (SideEffects != 0u) {
    fail_case("string/disabled", "argument expression was evaluated");
  }
  expect_capture("string/disabled", "", 0u, 0u);
}
#endif

int main(void) {
#if LOG_ENABLE_PRINT
  test_print_enabled();
#else
  test_print_disabled();
#endif

#if LOG_ENABLE_STRING
  test_string_enabled();
#else
  test_string_disabled();
#endif

#if LOG_ENABLE_PRINT && LOG_ENABLE_STRING
  puts("NH03 PASS enabled: print=4 string=5");
#elif !LOG_ENABLE_PRINT && LOG_ENABLE_STRING
  puts("NH03 PASS print-disabled: repeated=5 side-effects=0");
#elif LOG_ENABLE_PRINT && !LOG_ENABLE_STRING
  puts("NH03 PASS string-disabled: repeated=6 side-effects=0");
#else
  #error "NH03 test requires at least one simple API to remain enabled"
#endif
  return 0;
}

#undef TEST_UNUSED
