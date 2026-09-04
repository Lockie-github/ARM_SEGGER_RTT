#ifndef ARM_SEGGER_RTT_CFG_H
#define ARM_SEGGER_RTT_CFG_H

#define RTT_USER_CFG_ENABLE 1
#if RTT_USER_CFG_ENABLE
#define RTT_LOG_BUFFER_INDEX            1u
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   2
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 2
#define BUFFER_SIZE_UP                  129
#define BUFFER_SIZE_DOWN                5
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   9u
#endif

#endif
