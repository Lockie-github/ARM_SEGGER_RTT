#include "rtt_log.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char Output[256];
static unsigned OutputLength;
static unsigned WriteCalls;
static unsigned Cases;

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  if ((BufferIndex != RTT_LOG_BUFFER_INDEX) ||
      (NumBytes > (sizeof(Output) - OutputLength))) {
    return 0u;
  }
  WriteCalls++;
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

static void check_output(const char * Name, const char * Expected) {
  size_t ExpectedLength;

  ExpectedLength = strlen(Expected);
  if ((OutputLength != ExpectedLength) ||
      (memcmp(Output, Expected, ExpectedLength) != 0)) {
    fprintf(stderr, "NH05 %s: output bytes differ\n", Name);
    exit(1);
  }
  if (WriteCalls == 0u) {
    fprintf(stderr, "NH05 %s: no RTT write recorded\n", Name);
    exit(1);
  }
  printf("%s=%.*s", Name, (int)OutputLength, Output);
  Cases++;
}

static void check_value(const char * Name, float Value, const char * Expected) {
  reset_capture();
  log_float(Value);
  check_output(Name, Expected);
}

static void check_label(const char * Name,
                        const char * Label,
                        float Value,
                        const char * Expected) {
  reset_capture();
  log_float_label(Label, Value);
  check_output(Name, Expected);
}

int main(void) {
  check_value("positive", 1.25f, "1.250\n");
  check_value("negative", -2.5f, "-2.500\n");
  check_value("positive_zero", 0.0f, "0.000\n");
  check_value("negative_zero", from_bits(0x80000000u), "0.000\n");
  check_value("positive_integer", 42.0f, "42.000\n");
  check_value("negative_integer", -42.0f, "-42.000\n");
  check_value("exact_fraction", 1.125f, "1.125\n");
  check_value("truncate_positive", 1.2349f, "1.234\n");
  check_value("truncate_negative", -1.2349f, "-1.234\n");

  check_value("below_0_001", 0.000999f, "0.000\n");
  check_value("at_0_001", 0.001f, "0.001\n");
  check_value("negative_below_0_001", -0.000999f, "0.000\n");
  check_value("negative_at_0_001", -0.001f, "-0.001\n");
  check_value("below_integer", from_bits(0x3FFFFFFFu), "1.999\n");
  check_value("at_integer", from_bits(0x40000000u), "2.000\n");

  check_value("below_2p32", from_bits(0x4F7FFFFFu), "4294967040.000\n");
  check_value("negative_below_2p32", from_bits(0xCF7FFFFFu),
              "-4294967040.000\n");
  check_value("at_2p32", from_bits(0x4F800000u), "Overflow\n");
  check_value("negative_at_2p32", from_bits(0xCF800000u), "-Overflow\n");

  check_value("min_subnormal", from_bits(0x00000001u), "0.000\n");
  check_value("negative_min_subnormal", from_bits(0x80000001u), "0.000\n");
  check_value("nan", from_bits(0x7FC00000u), "NaN\n");
  check_value("negative_nan", from_bits(0xFFC00000u), "NaN\n");
  check_value("infinity", from_bits(0x7F800000u), "Inf\n");
  check_value("negative_infinity", from_bits(0xFF800000u), "-Inf\n");

  check_label("valid_label", "value", 1.25f, "value: 1.250\n");
  check_label("empty_label", "", 1.25f, ": 1.250\n");
  check_label("null_label", NULL, 1.25f, "1.250\n");

  printf("NH05 PASS enabled: cases=%u\n", Cases);
  return 0;
}
