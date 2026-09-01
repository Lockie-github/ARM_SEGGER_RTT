#include "rtt_float.h"
#include "rtt_log.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define FULL_WRITE UINT_MAX
#define MAX_WRITES 8u
#define OUTPUT_SIZE 512u

static unsigned Responses[MAX_WRITES];
static unsigned ResponseCount;
static unsigned WriteCalls;
static unsigned WriteStringCalls;
static unsigned Requested[MAX_WRITES];
static unsigned Returned[MAX_WRITES];
static unsigned AcceptedBytes;
static char Output[OUTPUT_SIZE];

static int fail(const char * pCase) {
  fprintf(stderr, "NH08 failure: %s\n", pCase);
  return 1;
}

static void reset_writes(const unsigned * pResponses, unsigned Count) {
  unsigned i;

  memset(Responses, 0, sizeof(Responses));
  for (i = 0u; i < Count; i++) {
    Responses[i] = pResponses[i];
  }
  ResponseCount = Count;
  WriteCalls = 0u;
  WriteStringCalls = 0u;
  memset(Requested, 0, sizeof(Requested));
  memset(Returned, 0, sizeof(Returned));
  AcceptedBytes = 0u;
  memset(Output, 0, sizeof(Output));
}

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  unsigned Result;
  unsigned Index;

  (void)BufferIndex;
  Index = WriteCalls++;
  if (Index >= MAX_WRITES) {
    return 0u;
  }
  Requested[Index] = NumBytes;
  if (Index >= ResponseCount) {
    Result = 0u;
  } else if (Responses[Index] == FULL_WRITE) {
    Result = NumBytes;
  } else {
    Result = Responses[Index] < NumBytes ? Responses[Index] : NumBytes;
  }
  Returned[Index] = Result;
  if ((AcceptedBytes + Result) <= sizeof(Output)) {
    memcpy(Output + AcceptedBytes, pBuffer, Result);
    AcceptedBytes += Result;
  }
  return Result;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  WriteStringCalls++;
  return SEGGER_RTT_Write(BufferIndex, pText, (unsigned)strlen(pText));
}

static int test_first_write_failures(void) {
  const unsigned Fail[] = {0u};

  reset_writes(Fail, 1u);
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "level=%d", 7) != -1 ||
      WriteCalls != 1u || AcceptedBytes != 0u) {
    return fail("level first-write failure");
  }

  reset_writes(Fail, 1u);
  if (SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "format=%d", 8) != -1 ||
      WriteCalls != 1u || AcceptedBytes != 0u) {
    return fail("formatter first-write failure");
  }

  reset_writes(Fail, 1u);
  if (RTT_LogString("raw") != 0 || WriteStringCalls != 1u ||
      WriteCalls != 1u || AcceptedBytes != 0u) {
    return fail("string first-write failure");
  }

  reset_writes(Fail, 1u);
  RTT_LogFloat3(1.25f, NULL);
  if (WriteCalls != 1u || AcceptedBytes != 0u) {
    return fail("float first-write failure");
  }
  return 0;
}

static int test_short_writes(void) {
  const unsigned Short3[] = {3u};
  const unsigned Short2[] = {2u};

  reset_writes(Short3, 1u);
  if (SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "format=%d", 8) != -1 ||
      WriteCalls != 1u || AcceptedBytes != 3u ||
      memcmp(Output, "for", 3u) != 0) {
    return fail("formatter short write");
  }

  reset_writes(Short3, 1u);
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "level=%d", 7) != -1 ||
      WriteCalls != 1u || AcceptedBytes != 3u ||
      memcmp(Output, "lev", 3u) != 0) {
    return fail("level short write");
  }

  reset_writes(Short2, 1u);
  if (RTT_LogString("raw") != 2 || WriteStringCalls != 1u ||
      WriteCalls != 1u || AcceptedBytes != 2u ||
      memcmp(Output, "ra", 2u) != 0) {
    return fail("string short write");
  }

  reset_writes(Short3, 1u);
  RTT_LogFloat3(1.25f, NULL);
  if (WriteCalls != 1u || Requested[0] != 6u || AcceptedBytes != 3u ||
      memcmp(Output, "1.2", 3u) != 0) {
    return fail("float short write");
  }
  return 0;
}

static int test_long_failure(unsigned FailureCall) {
  unsigned ResponsesForCase[3] = {FULL_WRITE, FULL_WRITE, FULL_WRITE};
  char Message[181];
  char Expected[187];
  unsigned ExpectedAccepted;

  memset(Message, 'A', sizeof(Message) - 1u);
  Message[sizeof(Message) - 1u] = '\0';
  memcpy(Expected, "long=", 5u);
  memcpy(Expected + 5u, Message, sizeof(Message) - 1u);
  Expected[185] = '\n';
  Expected[186] = '\0';

  ResponsesForCase[FailureCall - 1u] = 0u;
  reset_writes(ResponsesForCase, 3u);
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "long=%s", Message) != -1 ||
      WriteCalls != FailureCall) {
    return fail(FailureCall == 2u ? "second write failure calls" :
                                   "middle write failure calls");
  }
  ExpectedAccepted = (FailureCall - 1u) * SEGGER_RTT_PRINTF_BUFFER_SIZE;
  if (AcceptedBytes != ExpectedAccepted ||
      memcmp(Output, Expected, ExpectedAccepted) != 0) {
    return fail(FailureCall == 2u ? "second write duplicated data" :
                                   "middle write duplicated data");
  }
  if (Requested[0] != SEGGER_RTT_PRINTF_BUFFER_SIZE ||
      Requested[1] != SEGGER_RTT_PRINTF_BUFFER_SIZE ||
      (FailureCall == 3u && Requested[2] != 58u)) {
    return fail("long write request lengths");
  }
  return 0;
}

static int test_recovery(void) {
  const unsigned Fail[] = {0u};
  const unsigned Full[] = {FULL_WRITE};

  reset_writes(Fail, 1u);
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "bad") != -1) {
    return fail("recovery setup failure");
  }
  reset_writes(Full, 1u);
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "ok=%d", 9) != 5 ||
      WriteCalls != 1u || AcceptedBytes != 5u ||
      memcmp(Output, "ok=9\n", 5u) != 0) {
    return fail("formatter recovery");
  }

  reset_writes(Fail, 1u);
  if (RTT_LogString("bad") != 0) {
    return fail("string recovery setup failure");
  }
  reset_writes(Full, 1u);
  if (RTT_LogString("raw") != 3 || WriteStringCalls != 1u ||
      WriteCalls != 1u || AcceptedBytes != 3u ||
      memcmp(Output, "raw", 3u) != 0) {
    return fail("string recovery");
  }
  return 0;
}

int main(void) {
  if (test_first_write_failures() != 0 ||
      test_short_writes() != 0 ||
      test_long_failure(2u) != 0 ||
      test_long_failure(3u) != 0 ||
      test_recovery() != 0) {
    return 1;
  }
  printf("NH08 PASS failures: first=4 short=4 long=2 recovery=2\n");
  return 0;
}
