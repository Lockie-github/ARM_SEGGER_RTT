#include "SEGGER_RTT.h"

#include <stdio.h>
#include <string.h>

static char Ring[8];

static int fail(const char * pCase) {
  fprintf(stderr, "NH08 WriteNoLock failure: %s\n", pCase);
  return 1;
}

static void reset_ring(unsigned WrOff, unsigned RdOff) {
  memset(&_SEGGER_RTT, 0, sizeof(_SEGGER_RTT));
  memset(Ring, '.', sizeof(Ring));
  _SEGGER_RTT.aUp[0].pBuffer = Ring;
  _SEGGER_RTT.aUp[0].SizeOfBuffer = sizeof(Ring);
  _SEGGER_RTT.aUp[0].WrOff = WrOff;
  _SEGGER_RTT.aUp[0].RdOff = RdOff;
  _SEGGER_RTT.aUp[0].Flags = SEGGER_RTT_MODE_NO_BLOCK_SKIP;
}

int main(void) {
  char Before[sizeof(Ring)];

  reset_ring(0u, 0u);
  memcpy(Before, Ring, sizeof(Ring));
  if (SEGGER_RTT_WriteNoLock(0u, "Z", 0u) != 0u ||
      _SEGGER_RTT.aUp[0].WrOff != 0u ||
      memcmp(Ring, Before, sizeof(Ring)) != 0) {
    return fail("zero length");
  }

  reset_ring(0u, 0u);
  if (SEGGER_RTT_WriteNoLock(0u, "ABC", 3u) != 3u ||
      _SEGGER_RTT.aUp[0].WrOff != 3u ||
      memcmp(Ring, "ABC.....", sizeof(Ring)) != 0) {
    return fail("capacity available");
  }

  reset_ring(7u, 0u);
  memcpy(Before, Ring, sizeof(Ring));
  if (SEGGER_RTT_WriteNoLock(0u, "Z", 1u) != 0u ||
      _SEGGER_RTT.aUp[0].WrOff != 7u ||
      memcmp(Ring, Before, sizeof(Ring)) != 0) {
    return fail("full buffer rejection");
  }

  reset_ring(6u, 4u);
  if (SEGGER_RTT_WriteNoLock(0u, "WXYZ", 4u) != 4u ||
      _SEGGER_RTT.aUp[0].WrOff != 2u ||
      memcmp(Ring, "YZ....WX", sizeof(Ring)) != 0) {
    return fail("wrap-around write");
  }

  printf("NH08 PASS WriteNoLock: zero, capacity, full, wrap\n");
  return 0;
}
