#include "rtt_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPTURE_SIZE 128u
#define RANDOM_CASES 100000u

static unsigned char Output[CAPTURE_SIZE];
static unsigned OutputLength;
static unsigned WriteCalls;

static void fail(uint32_t Bits, const char * pReason) {
  fprintf(stderr, "NH12 corpus failure bits=%08X: %s\n", Bits, pReason);
  exit(1);
}

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  if ((BufferIndex != RTT_LOG_BUFFER_INDEX) ||
      (NumBytes > (sizeof(Output) - OutputLength))) {
    return 0u;
  }
  ++WriteCalls;
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  return NumBytes;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  return SEGGER_RTT_Write(BufferIndex, pText, (unsigned)strlen(pText));
}

static float from_bits(uint32_t Bits) {
  union {
    uint32_t Bits;
    float Value;
  } Number;

  Number.Bits = Bits;
  return Number.Value;
}

static void reset_capture(void) {
  memset(Output, 0, sizeof(Output));
  OutputLength = 0u;
  WriteCalls = 0u;
}

static unsigned capture_legacy(uint32_t Bits,
                               const char * pLabel,
                               unsigned char * pResult) {
  reset_capture();
  RTT_LogFloat3(from_bits(Bits), pLabel);
  if (WriteCalls == 0u) {
    fail(Bits, "legacy emitted no write");
  }
  memcpy(pResult, Output, OutputLength);
  return OutputLength;
}

static unsigned capture_typed(uint32_t Bits,
                              const char * pLabel,
                              unsigned char * pResult) {
  int ReturnValue;

  reset_capture();
  ReturnValue = RTT_LogF32(pLabel, from_bits(Bits));
  if ((WriteCalls != 1u) || (ReturnValue != (int)OutputLength)) {
    fail(Bits, "typed did not use one complete write");
  }
  memcpy(pResult, Output, OutputLength);
  return OutputLength;
}

static unsigned numeric_offset(const unsigned char * pData, unsigned Length) {
  unsigned Index;

  for (Index = 0u; Index + 1u < Length; ++Index) {
    if ((pData[Index] == ':') && (pData[Index + 1u] == ' ')) {
      return Index + 2u;
    }
  }
  return 0u;
}

static void compare_value(uint32_t Bits, int Emit) {
  unsigned char Legacy[CAPTURE_SIZE];
  unsigned char Typed[CAPTURE_SIZE];
  unsigned LegacyLength;
  unsigned TypedLength;

  LegacyLength = capture_legacy(Bits, NULL, Legacy);
  TypedLength = capture_typed(Bits, NULL, Typed);
  if ((LegacyLength != TypedLength) ||
      (memcmp(Legacy, Typed, LegacyLength) != 0)) {
    fail(Bits, "legacy and typed numeric text differ");
  }
  if (Emit != 0) {
    printf("R\t%08X\t", Bits);
    (void)fwrite(Legacy, 1u, LegacyLength, stdout);
  }
}

static void compare_label(uint32_t Bits,
                          const char * pKind,
                          const char * pLabel) {
  unsigned char Legacy[CAPTURE_SIZE];
  unsigned char Typed[CAPTURE_SIZE];
  unsigned LegacyLength;
  unsigned TypedLength;
  unsigned LegacyOffset;
  unsigned TypedOffset;

  LegacyLength = capture_legacy(Bits, pLabel, Legacy);
  TypedLength = capture_typed(Bits, pLabel, Typed);
  LegacyOffset = numeric_offset(Legacy, LegacyLength);
  TypedOffset = numeric_offset(Typed, TypedLength);
  if (((LegacyLength - LegacyOffset) != (TypedLength - TypedOffset)) ||
      (memcmp(Legacy + LegacyOffset, Typed + TypedOffset,
              LegacyLength - LegacyOffset) != 0)) {
    fail(Bits, "normalized label numeric text differs");
  }
  printf("L\t%08X\t%s\t", Bits, pKind);
  (void)fwrite(Legacy + LegacyOffset, 1u, LegacyLength - LegacyOffset, stdout);
}

static void fill_label(char * pLabel, unsigned Length) {
  unsigned Index;

  for (Index = 0u; Index < Length; ++Index) {
    pLabel[Index] = (char)('A' + (Index % 26u));
  }
  pLabel[Length] = '\0';
}

int main(int argc, char ** argv) {
  static const uint32_t Fixed[] = {
    UINT32_C(0x00000000), UINT32_C(0x80000000), UINT32_C(0x00000001),
    UINT32_C(0x80000001), UINT32_C(0x3A83126F), UINT32_C(0xBA83126F),
    UINT32_C(0x3F800000), UINT32_C(0xBF800000), UINT32_C(0x3F9E0419),
    UINT32_C(0xBF9E0419), UINT32_C(0x3FFFFFFF), UINT32_C(0x40000000),
    UINT32_C(0x4F7FFFFF), UINT32_C(0xCF7FFFFF), UINT32_C(0x4F800000),
    UINT32_C(0xCF800000), UINT32_C(0x7F800000), UINT32_C(0xFF800000),
    UINT32_C(0x7FC00000), UINT32_C(0xFFC00000)
  };
  const char * LabelKinds[] = {
    "null", "empty", "normal", "percent", "label46", "label47", "long"
  };
  const char * Labels[7];
  char Label46[47];
  char Label47[48];
  char LabelLong[101];
  uint32_t State = UINT32_C(0x0E5413B1);
  unsigned Index;
  unsigned LabelIndex;

  fill_label(Label46, 46u);
  fill_label(Label47, 47u);
  fill_label(LabelLong, 100u);
  Labels[0] = NULL;
  Labels[1] = "";
  Labels[2] = "value";
  Labels[3] = "rate%done";
  Labels[4] = Label46;
  Labels[5] = Label47;
  Labels[6] = LabelLong;

  if (argc != 2) {
    fprintf(stderr, "usage: %s random|labels\n", argv[0]);
    return 2;
  }
  if (strcmp(argv[1], "random") == 0) {
    for (Index = 0u; Index < sizeof(Fixed) / sizeof(Fixed[0]); ++Index) {
      compare_value(Fixed[Index], 1);
    }
    for (Index = 0u; Index < RANDOM_CASES; ++Index) {
      State ^= State << 13;
      State ^= State >> 17;
      State ^= State << 5;
      compare_value(State, 1);
    }
    printf("SUMMARY\trandom=%u\tfixed=%u\tPASS\n",
           RANDOM_CASES,
           (unsigned)(sizeof(Fixed) / sizeof(Fixed[0])));
  } else if (strcmp(argv[1], "labels") == 0) {
    for (Index = 0u; Index < sizeof(Fixed) / sizeof(Fixed[0]); ++Index) {
      for (LabelIndex = 0u; LabelIndex < 7u; ++LabelIndex) {
        compare_label(Fixed[Index], LabelKinds[LabelIndex], Labels[LabelIndex]);
      }
    }
    printf("SUMMARY\tlabels=%u\tPASS\n",
           (unsigned)(sizeof(Fixed) / sizeof(Fixed[0])) * 7u);
  } else {
    fprintf(stderr, "unknown corpus mode: %s\n", argv[1]);
    return 2;
  }
  return 0;
}
