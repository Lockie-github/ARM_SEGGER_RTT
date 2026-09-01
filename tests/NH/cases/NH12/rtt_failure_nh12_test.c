#include "rtt_log.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FULL_WRITE UINT_MAX
#define MAX_WRITES 32u
#define OUTPUT_SIZE 2048u

typedef enum {
  WRITE_NORMAL,
  WRITE_ZERO,
  WRITE_SHORT,
  WRITE_FAIL_AT
} WRITE_MODE;

typedef enum {
  API_LEVEL_INFO,
  API_LEVEL_DEBUG,
  API_LEVEL_WARN,
  API_LEVEL_ERROR,
  API_PRINT,
  API_STRING,
  API_FLOAT,
  API_FLOAT_LABEL,
  API_I32,
  API_U32,
  API_HEX32,
  API_POINTER,
  API_F32,
  API_COUNT
} API_KIND;

typedef struct {
  API_KIND Kind;
  const char * pName;
  const char * pExpected;
  int SuccessReturn;
  int ZeroReturn;
  int ShortReturn;
} API_CASE;

static WRITE_MODE Mode;
static unsigned FailureCall;
static unsigned WriteCalls;
static unsigned WriteStringCalls;
static unsigned Requested[MAX_WRITES];
static unsigned Returned[MAX_WRITES];
static unsigned AcceptedBytes;
static unsigned LastBufferIndex;
static unsigned char Output[OUTPUT_SIZE];
static unsigned MatrixCases;

static void fail(const char * pCase) {
  fprintf(stderr, "NH12 failure: %s\n", pCase);
  exit(1);
}

static void reset_writes(WRITE_MODE NewMode, unsigned NewFailureCall) {
  Mode = NewMode;
  FailureCall = NewFailureCall;
  WriteCalls = 0u;
  WriteStringCalls = 0u;
  memset(Requested, 0, sizeof(Requested));
  memset(Returned, 0, sizeof(Returned));
  AcceptedBytes = 0u;
  LastBufferIndex = UINT_MAX;
  memset(Output, 0, sizeof(Output));
}

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  unsigned Index = WriteCalls++;
  unsigned Result;

  if (Index >= MAX_WRITES) {
    fail("write attempt exceeds capture capacity");
  }
  LastBufferIndex = BufferIndex;
  Requested[Index] = NumBytes;
  if (Mode == WRITE_ZERO) {
    Result = 0u;
  } else if (Mode == WRITE_SHORT) {
    Result = (NumBytes == 0u) ? 0u : (NumBytes - 1u);
  } else if ((Mode == WRITE_FAIL_AT) && ((Index + 1u) == FailureCall)) {
    Result = 0u;
  } else {
    Result = NumBytes;
  }
  Returned[Index] = Result;
  if (Result > (sizeof(Output) - AcceptedBytes)) {
    fail("accepted output exceeds capture capacity");
  }
  memcpy(Output + AcceptedBytes, pBuffer, Result);
  AcceptedBytes += Result;
  return Result;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  ++WriteStringCalls;
  return SEGGER_RTT_Write(BufferIndex, pText, (unsigned)strlen(pText));
}

static int invoke(API_KIND Kind) {
  switch (Kind) {
  case API_LEVEL_INFO:
    return RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "level_info");
  case API_LEVEL_DEBUG:
    return RTT_LogPrintf(RTT_LOG_LEVEL_DEBUG, "level_debug");
  case API_LEVEL_WARN:
    return RTT_LogPrintf(RTT_LOG_LEVEL_WARN, "level_warn");
  case API_LEVEL_ERROR:
    return RTT_LogPrintf(RTT_LOG_LEVEL_ERROR, "level_error");
  case API_PRINT:
    return SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "print=%d", 7);
  case API_STRING:
    return RTT_LogString("string");
  case API_FLOAT:
    RTT_LogFloat3(1.25f, NULL);
    return INT_MIN;
  case API_FLOAT_LABEL:
    RTT_LogFloat3(-2.5f, "float");
    return INT_MIN;
  case API_I32:
    return RTT_LogI32("i32", INT32_MIN);
  case API_U32:
    return RTT_LogU32("u32", UINT32_MAX);
  case API_HEX32:
    return RTT_LogHex32("hex", UINT32_C(0x89ABCDEF));
  case API_POINTER:
    return RTT_LogPointer("ptr", (const void *)(uintptr_t)1u);
  case API_F32:
    return RTT_LogF32("f32", -1.25f);
  default:
    fail("unknown API kind");
    return 0;
  }
}

static void verify_call(const API_CASE * pCase,
                        const char * pMode,
                        WRITE_MODE WriteMode,
                        int ExpectedReturn,
                        unsigned ExpectedAccepted) {
  size_t ExpectedLength = strlen(pCase->pExpected);
  int Result;

  reset_writes(WriteMode, 0u);
  Result = invoke(pCase->Kind);
  if ((Result != ExpectedReturn) || (WriteCalls != 1u) ||
      (LastBufferIndex != RTT_LOG_BUFFER_INDEX) ||
      (Requested[0] != ExpectedLength) ||
      (AcceptedBytes != ExpectedAccepted) ||
      (memcmp(Output, pCase->pExpected, ExpectedAccepted) != 0)) {
    fail(pCase->pName);
  }
  if (((pCase->Kind == API_STRING) ? 1u : 0u) != WriteStringCalls) {
    fail("WriteString dispatch count mismatch");
  }
  printf("api\t%s\t%s\t%d\t%u\t%u\t%u\n",
         pCase->pName, pMode, Result, WriteCalls, Requested[0], AcceptedBytes);
  ++MatrixCases;
}

static void run_api_matrix(void) {
  static const API_CASE Cases[API_COUNT] = {
    {API_LEVEL_INFO, "level_info", "level_info\n", 11, -1, -1},
    {API_LEVEL_DEBUG, "level_debug", "level_debug\n", 12, -1, -1},
    {API_LEVEL_WARN, "level_warn", "level_warn\n", 11, -1, -1},
    {API_LEVEL_ERROR, "level_error", "level_error\n", 12, -1, -1},
    {API_PRINT, "print", "print=7", 7, -1, -1},
    {API_STRING, "string", "string", 6, 0, 5},
    {API_FLOAT, "float", "1.250\n", INT_MIN, INT_MIN, INT_MIN},
    {API_FLOAT_LABEL, "float_label", "float: -2.500\n", INT_MIN, INT_MIN,
     INT_MIN},
    {API_I32, "i32", "i32: -2147483648\n", 17, -1, -1},
    {API_U32, "u32", "u32: 4294967295\n", 16, -1, -1},
    {API_HEX32, "hex32", "hex: 0x89ABCDEF\n", 16, -1, -1},
    {API_POINTER, "pointer", "ptr: 0x0000000000000001\n", 24, -1, -1},
    {API_F32, "f32", "f32: -1.250\n", 12, -1, -1}
  };
  unsigned Index;

  for (Index = 0u; Index < API_COUNT; ++Index) {
    const API_CASE * pCase = &Cases[Index];
    unsigned Length = (unsigned)strlen(pCase->pExpected);

    verify_call(pCase, "normal", WRITE_NORMAL, pCase->SuccessReturn, Length);
    verify_call(pCase, "zero", WRITE_ZERO, pCase->ZeroReturn, 0u);
    verify_call(pCase, "recover_zero", WRITE_NORMAL,
                pCase->SuccessReturn, Length);
    verify_call(pCase, "short", WRITE_SHORT, pCase->ShortReturn, Length - 1u);
    verify_call(pCase, "recover_short", WRITE_NORMAL,
                pCase->SuccessReturn, Length);
  }
}

static void run_formatter_failures(void) {
  char Message[321];
  unsigned char Golden[OUTPUT_SIZE];
  unsigned GoldenLength;
  unsigned FullWrites;
  unsigned FailurePoints[4];
  unsigned Index;

  memset(Message, 'A', sizeof(Message) - 1u);
  Message[sizeof(Message) - 1u] = '\0';
  reset_writes(WRITE_NORMAL, 0u);
  if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "%s", Message) != 321) {
    fail("formatter long normal return");
  }
  GoldenLength = AcceptedBytes;
  FullWrites = WriteCalls;
  memcpy(Golden, Output, GoldenLength);
  if ((GoldenLength != 321u) || (FullWrites < 5u)) {
    fail("formatter long message did not produce enough flushes");
  }
  FailurePoints[0] = 1u;
  FailurePoints[1] = 2u;
  FailurePoints[2] = (FullWrites + 1u) / 2u;
  FailurePoints[3] = FullWrites;

  for (Index = 0u; Index < 4u; ++Index) {
    unsigned Point = FailurePoints[Index];
    unsigned ExpectedAccepted = 0u;
    unsigned Call;

    reset_writes(WRITE_FAIL_AT, Point);
    if (RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "%s", Message) != -1) {
      fail("formatter injected failure return");
    }
    if (WriteCalls != Point) {
      fail("formatter continued after injected failure");
    }
    for (Call = 0u; Call + 1u < Point; ++Call) {
      ExpectedAccepted += Requested[Call];
    }
    if ((AcceptedBytes != ExpectedAccepted) ||
        (memcmp(Output, Golden, ExpectedAccepted) != 0)) {
      fail("formatter repeated or changed accepted prefix");
    }
    printf("formatter\tfail_flush_%u_of_%u\t-1\t%u\t%u\t%u\n",
           Point, FullWrites, WriteCalls, Requested[Point - 1u],
           AcceptedBytes);
  }

  reset_writes(WRITE_NORMAL, 0u);
  if ((RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "after") != 6) ||
      (WriteCalls != 1u) || (AcceptedBytes != 6u) ||
      (memcmp(Output, "after\n", 6u) != 0)) {
    fail("formatter did not recover after flush failures");
  }
  printf("formatter\trecovery\t6\t1\t6\t6\n");
}

int main(void) {
  printf("kind\tpath\tmode\treturn\twrites\trequest\taccepted\n");
  run_api_matrix();
  run_formatter_failures();
  printf("summary\tapi_matrix\tcompleted\t%u\tPASS\n", MatrixCases);
  return 0;
}
