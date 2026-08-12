#ifndef ARM_SEGGER_RTT_CFG_H
#define ARM_SEGGER_RTT_CFG_H

#define RTT_LOG_ENABLE       1
#define LOG_ENABLE_LITE      1
#define LOG_ENABLE_INFO      0
#define LOG_ENABLE_DEBUG     1
#define LOG_ENABLE_WARN      1
#define LOG_ENABLE_ERROR     1
#define LOG_ENABLE_PRINT     1
#define LOG_ENABLE_FLOAT     1
#define RTT_LOG_BUFFER_INDEX 0u
#define RTT_LOG_USE_COLOR    0

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   1
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 1
#define BUFFER_SIZE_UP                  257
#define BUFFER_SIZE_DOWN                3
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   17u

#endif
