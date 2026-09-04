#ifndef ARM_SEGGER_RTT_CFG_H
#define ARM_SEGGER_RTT_CFG_H

#define RTT_USER_CFG_ENABLE 1
#if RTT_USER_CFG_ENABLE
#define RTT_LOG_BUFFER_INDEX            0u
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   1
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 1
#define BUFFER_SIZE_UP                  257
#define BUFFER_SIZE_DOWN                3
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   17u
#endif

#endif
