#include "SEGGER_RTT.h"

#include <stdio.h>
#include <string.h>

static char Output[8];
static unsigned OutputLength;
static unsigned WriteCalls;

unsigned SEGGER_RTT_Write(unsigned BufferIndex,
                          const void * pBuffer,
                          unsigned NumBytes) {
  (void)BufferIndex;
  if (NumBytes > (sizeof(Output) - OutputLength)) {
    return 0u;
  }
  memcpy(Output + OutputLength, pBuffer, NumBytes);
  OutputLength += NumBytes;
  WriteCalls++;
  return NumBytes;
}

int main(void) {
  int Result;

  Result = SEGGER_RTT_printf(0u, "%s", "abc");
  if ((Result != 3) || (OutputLength != 3u) ||
      (memcmp(Output, "abc", 3u) != 0) || (WriteCalls != 3u)) {
    return 1;
  }
  puts("NH07 PASS minimum printf buffer: size=1 output=3 writes=3");
  return 0;
}
