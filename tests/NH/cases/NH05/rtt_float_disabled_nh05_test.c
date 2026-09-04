#include "rtt_log.h"

#include <stdio.h>
#include <string.h>

static unsigned WriteCalls;
static unsigned SideEffects;

#if defined(__GNUC__) || defined(__clang__)
  #define TEST_UNUSED __attribute__((unused))
#else
  #define TEST_UNUSED
#endif

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  (void)BufferIndex;
  (void)pBuffer;
  WriteCalls++;
  return NumBytes;
}

unsigned SEGGER_RTT_WriteString(unsigned BufferIndex, const char * pText) {
  return SEGGER_RTT_Write(BufferIndex, pText, (unsigned)strlen(pText));
}

static TEST_UNUSED float side_effect_value(void) {
  SideEffects++;
  return 1.25f;
}

static TEST_UNUSED const char * side_effect_label(void) {
  SideEffects++;
  return "value";
}

int main(void) {
  log_float(side_effect_value());
  log_float_label(side_effect_label(), side_effect_value());
  if ((SideEffects != 0u) || (WriteCalls != 0u)) {
    return 1;
  }
  printf("NH05 PASS disabled: side-effects=%u writes=%u\n",
         SideEffects, WriteCalls);
  return 0;
}

#undef TEST_UNUSED
