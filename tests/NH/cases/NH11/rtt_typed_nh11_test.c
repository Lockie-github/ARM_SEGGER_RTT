#include "rtt_log.h"
#include "rtt_format_prv.h"

#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(uintptr_t) == 8u,
               "NH11 host run requires a 64-bit pointer target");

typedef enum {
  API_I32,
  API_U32,
  API_HEX32,
  API_POINTER
} API_KIND;

static unsigned char Requested[256];
static unsigned RequestedLength;
static unsigned AcceptedLength;
static unsigned WriteAttempts;
static unsigned LastBufferIndex;
static unsigned ReturnOverride = UINT_MAX;
static unsigned FormatterCalls;
static unsigned WriteStringCalls;
static unsigned CaseCount;

static void fail(const char * pMessage) {
  fprintf(stderr, "NH11 failure: %s\n", pMessage);
  exit(1);
}

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  unsigned Result = NumBytes;

  ++WriteAttempts;
  LastBufferIndex = BufferIndex;
  RequestedLength = NumBytes;
  if (NumBytes > sizeof(Requested)) {
    fail("write request exceeds capture buffer");
  }
  memcpy(Requested, pBuffer, NumBytes);
  if (ReturnOverride != UINT_MAX) {
    Result = ReturnOverride;
    if (Result > NumBytes) {
      Result = NumBytes;
    }
  }
  AcceptedLength = Result;
  return Result;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  (void)BufferIndex;
  (void)pText;
  ++WriteStringCalls;
  return 0u;
}

int RTT_vprintfFramed(unsigned BufferIndex,
                      const char * pPrefix,
                      const char * pFormat,
                      va_list * pParamList,
                      const char * pSuffix) {
  (void)BufferIndex;
  (void)pPrefix;
  (void)pFormat;
  (void)pParamList;
  (void)pSuffix;
  ++FormatterCalls;
  return -1;
}

static void reset_capture(void) {
  memset(Requested, 0, sizeof(Requested));
  RequestedLength = 0u;
  AcceptedLength = 0u;
  WriteAttempts = 0u;
  LastBufferIndex = UINT_MAX;
  ReturnOverride = UINT_MAX;
  FormatterCalls = 0u;
  WriteStringCalls = 0u;
}

static unsigned build_expected(char * pOutput,
                               const char * pLabel,
                               const char * pValue) {
  unsigned Length = 0u;
  unsigned LabelLength = 0u;
  size_t ValueLength;

  if ((pLabel != NULL) && (*pLabel != '\0')) {
    while ((LabelLength < 46u) && (pLabel[LabelLength] != '\0')) {
      ++LabelLength;
    }
  }
  if (LabelLength != 0u) {
    memcpy(pOutput, pLabel, LabelLength);
    Length = LabelLength;
    pOutput[Length++] = ':';
    pOutput[Length++] = ' ';
  }
  ValueLength = strlen(pValue);
  memcpy(pOutput + Length, pValue, ValueLength);
  Length += (unsigned)ValueLength;
  return Length;
}

static int call_api(API_KIND Api, uint64_t Value, const char * pLabel) {
  switch (Api) {
  case API_I32:
    return RTT_LogI32(pLabel, (int32_t)(uint32_t)Value);
  case API_U32:
    return RTT_LogU32(pLabel, (uint32_t)Value);
  case API_HEX32:
    return RTT_LogHex32(pLabel, (uint32_t)Value);
  case API_POINTER:
    return RTT_LogPointer(pLabel, (const void *)(uintptr_t)Value);
  default:
    fail("unknown API");
    return -1;
  }
}

static void print_hex(const unsigned char * pData, unsigned Length) {
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    printf("%02X", pData[Index]);
  }
}

static void run_success(API_KIND Api,
                        const char * pApi,
                        const char * pCase,
                        const char * pLabel,
                        uint64_t Value,
                        const char * pValueText) {
  char Expected[256];
  unsigned ExpectedLength;
  int Result;

  ExpectedLength = build_expected(Expected, pLabel, pValueText);
  reset_capture();
  Result = call_api(Api, Value, pLabel);
  if ((Result != (int)ExpectedLength) || (WriteAttempts != 1u) ||
      (LastBufferIndex != RTT_LOG_BUFFER_INDEX) ||
      (RequestedLength != ExpectedLength) ||
      (AcceptedLength != ExpectedLength) ||
      (memcmp(Requested, Expected, ExpectedLength) != 0) ||
      (FormatterCalls != 0u) || (WriteStringCalls != 0u)) {
    fail("successful typed frame violated its contract");
  }
  printf("success\t%s\t%s\t%u\t%d\t%u\t%u\t",
         pApi, pCase, LastBufferIndex, Result, WriteAttempts, RequestedLength);
  print_hex(Requested, RequestedLength);
  putchar('\n');
  ++CaseCount;
}

static void run_failure(API_KIND Api,
                        const char * pApi,
                        unsigned Override,
                        const char * pFailure) {
  static const char Label[] = "failure%label";
  const char * pValueText;
  uint64_t Value;
  char Expected[256];
  unsigned ExpectedLength;
  int Result;

  switch (Api) {
  case API_I32:
    Value = (uint32_t)INT32_MIN;
    pValueText = "-2147483648\n";
    break;
  case API_U32:
    Value = UINT32_MAX;
    pValueText = "4294967295\n";
    break;
  case API_HEX32:
    Value = UINT32_C(0x89ABCDEF);
    pValueText = "0x89ABCDEF\n";
    break;
  case API_POINTER:
    Value = UINT64_C(0x8000000000000001);
    pValueText = "0x8000000000000001\n";
    break;
  default:
    fail("unknown failure API");
    return;
  }
  ExpectedLength = build_expected(Expected, Label, pValueText);
  reset_capture();
  ReturnOverride = (Override == UINT_MAX) ? (ExpectedLength - 1u) : Override;
  Result = call_api(Api, Value, Label);
  if ((Result != -1) || (WriteAttempts != 1u) ||
      (LastBufferIndex != RTT_LOG_BUFFER_INDEX) ||
      (RequestedLength != ExpectedLength) ||
      (memcmp(Requested, Expected, ExpectedLength) != 0) ||
      (FormatterCalls != 0u) || (WriteStringCalls != 0u)) {
    fail("short/failed typed write retried or returned an invalid result");
  }
  printf("failure\t%s\t%s\t%u\t%d\t%u\t%u\t%u\n",
         pApi, pFailure, LastBufferIndex, Result, WriteAttempts,
         RequestedLength, AcceptedLength);
  ++CaseCount;
}

static void fill_label(char * pLabel, unsigned Length) {
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    pLabel[Index] = (char)('A' + (Index % 26u));
  }
  pLabel[Length] = '\0';
}

static void run_numeric_boundaries(void) {
  char PointerText[32];
  uintptr_t PointerValues[] = {
    (uintptr_t)0u,
    (uintptr_t)1u,
    (uintptr_t)UINT64_C(0x8000000000000000),
    (uintptr_t)UINTPTR_MAX
  };
  unsigned Index;

  run_success(API_I32, "i32", "zero", "num", 0u, "0\n");
  run_success(API_I32, "i32", "one", "num", 1u, "1\n");
  run_success(API_I32, "i32", "minus_one", "num", UINT32_MAX, "-1\n");
  run_success(API_I32, "i32", "max", "num", INT32_MAX, "2147483647\n");
  run_success(API_I32, "i32", "min", "num", (uint32_t)INT32_MIN,
              "-2147483648\n");

  run_success(API_U32, "u32", "zero", "num", 0u, "0\n");
  run_success(API_U32, "u32", "one", "num", 1u, "1\n");
  run_success(API_U32, "u32", "max", "num", UINT32_MAX, "4294967295\n");

  run_success(API_HEX32, "hex32", "zero", "num", 0u, "0x00000000\n");
  run_success(API_HEX32, "hex32", "one", "num", 1u, "0x00000001\n");
  run_success(API_HEX32, "hex32", "high", "num", UINT32_C(0x89ABCDEF),
              "0x89ABCDEF\n");
  run_success(API_HEX32, "hex32", "max", "num", UINT32_MAX,
              "0xFFFFFFFF\n");

  for (Index = 0u; Index < sizeof(PointerValues) / sizeof(PointerValues[0]);
       ++Index) {
    if (snprintf(PointerText, sizeof(PointerText), "0x%016" PRIXPTR "\n",
                 PointerValues[Index]) != 19) {
      fail("pointer oracle formatting failed");
    }
    run_success(API_POINTER, "pointer", "width64", "ptr",
                (uint64_t)PointerValues[Index], PointerText);
  }
}

static void run_label_boundaries(void) {
  char Label45[46];
  char Label46[47];
  char Label47[48];
  char LabelLong[129];
  const char * Labels[8];
  const char * Cases[8] = {
    "null", "empty", "normal", "percent", "label45", "label46",
    "label47", "overlong"
  };
  unsigned Index;

  fill_label(Label45, 45u);
  fill_label(Label46, 46u);
  fill_label(Label47, 47u);
  fill_label(LabelLong, 128u);
  Labels[0] = NULL;
  Labels[1] = "";
  Labels[2] = "normal";
  Labels[3] = "rate%done";
  Labels[4] = Label45;
  Labels[5] = Label46;
  Labels[6] = Label47;
  Labels[7] = LabelLong;

  for (Index = 0u; Index < 8u; ++Index) {
    run_success(API_I32, "i32", Cases[Index], Labels[Index],
                (uint32_t)-123, "-123\n");
    run_success(API_U32, "u32", Cases[Index], Labels[Index],
                123u, "123\n");
    run_success(API_HEX32, "hex32", Cases[Index], Labels[Index],
                UINT32_C(0x89ABCDEF), "0x89ABCDEF\n");
    run_success(API_POINTER, "pointer", Cases[Index], Labels[Index],
                UINT64_C(0x8000000000000001), "0x8000000000000001\n");
  }
}

static void run_macro_smoke(void) {
  reset_capture();
  log_i32("macro", -7);
  if ((WriteAttempts != 1u) || (RequestedLength != 10u) ||
      (memcmp(Requested, "macro: -7\n", 10u) != 0)) {
    fail("log_i32 macro output mismatch");
  }
  reset_capture();
  log_u32("macro", 7u);
  if ((WriteAttempts != 1u) || (RequestedLength != 9u) ||
      (memcmp(Requested, "macro: 7\n", 9u) != 0)) {
    fail("log_u32 macro output mismatch");
  }
  reset_capture();
  log_hex32("macro", UINT32_C(0x89ABCDEF));
  if ((WriteAttempts != 1u) || (RequestedLength != 18u) ||
      (memcmp(Requested, "macro: 0x89ABCDEF\n", 18u) != 0)) {
    fail("log_hex32 macro output mismatch");
  }
  reset_capture();
  log_pointer("macro", (const void *)(uintptr_t)UINT64_C(0x8000000000000001));
  if ((WriteAttempts != 1u) || (RequestedLength != 26u) ||
      (memcmp(Requested, "macro: 0x8000000000000001\n", 26u) != 0)) {
    fail("log_pointer macro output mismatch");
  }
  printf("macro\tall\tsmoke\t%u\tPASS\n", RTT_LOG_BUFFER_INDEX);
  CaseCount += 4u;
}

int main(void) {
  printf("kind\tapi\tcase\tchannel\tresult\twrites\trequest_bytes\tdata_or_accepted\n");
  run_numeric_boundaries();
  run_label_boundaries();
  run_macro_smoke();
  run_failure(API_I32, "i32", 0u, "return_zero");
  run_failure(API_I32, "i32", UINT_MAX, "short_write");
  run_failure(API_U32, "u32", 0u, "return_zero");
  run_failure(API_U32, "u32", UINT_MAX, "short_write");
  run_failure(API_HEX32, "hex32", 0u, "return_zero");
  run_failure(API_HEX32, "hex32", UINT_MAX, "short_write");
  run_failure(API_POINTER, "pointer", 0u, "return_zero");
  run_failure(API_POINTER, "pointer", UINT_MAX, "short_write");
  printf("summary\tall\tcompleted\t%u\tPASS\n", CaseCount);
  return 0;
}
