#include "rtt_log.h"

#include <stddef.h>

_Static_assert(RTT_USER_CFG_ENABLE == EXPECT_USER_CFG,
               "unexpected RTT_USER_CFG_ENABLE");
_Static_assert(SEGGER_RTT_MAX_NUM_UP_BUFFERS == EXPECT_UP,
               "unexpected up-buffer count");
_Static_assert(SEGGER_RTT_MAX_NUM_DOWN_BUFFERS == EXPECT_DOWN,
               "unexpected down-buffer count");
_Static_assert(BUFFER_SIZE_UP == EXPECT_UP_SIZE,
               "unexpected up-buffer size");
_Static_assert(BUFFER_SIZE_DOWN == EXPECT_DOWN_SIZE,
               "unexpected down-buffer size");
_Static_assert(SEGGER_RTT_PRINTF_BUFFER_SIZE == EXPECT_PRINTF_SIZE,
               "unexpected printf buffer size");
_Static_assert(RTT_LOG_BUFFER_INDEX == EXPECT_LOG_INDEX,
               "unexpected log buffer index");
_Static_assert(SEGGER_RTT__CB_SIZE == EXPECT_CB_SIZE,
               "unexpected RTT control-block layout");
_Static_assert(sizeof(((SEGGER_RTT_CB *)0)->aUp) /
                 sizeof(SEGGER_RTT_BUFFER_UP) == EXPECT_UP,
               "up-buffer array layout differs");
_Static_assert(sizeof(((SEGGER_RTT_CB *)0)->aDown) /
                 sizeof(SEGGER_RTT_BUFFER_DOWN) == EXPECT_DOWN,
               "down-buffer array layout differs");

int nh07_config_probe(void) {
  return (int)sizeof(SEGGER_RTT_CB);
}
