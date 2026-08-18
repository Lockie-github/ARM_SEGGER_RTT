#include "rtt_log.h"
#include "rtt_format_prv.h"

#include <stdarg.h>

#if RTT_LOG_ENABLE

/*
 * 返回一条日志的帧头。轻量模式没有帧头；PRINT 是原样打印接口，也不带
 * 级别标记。函数只返回静态字符串，不产生额外缓冲区或动态内存开销。
 */
static const char * rtt_log_prefix(RTT_LOG_LEVEL Level) {
#if LOG_ENABLE_LITE
  (void)Level;
  return "";
#elif RTT_LOG_USE_COLOR
  switch (Level) {
  case RTT_LOG_LEVEL_INFO:
    return RTT_CTRL_TEXT_BRIGHT_GREEN "[INFO] ";
  case RTT_LOG_LEVEL_DEBUG:
    return RTT_CTRL_TEXT_BRIGHT_BLUE "[DEBUG] ";
  case RTT_LOG_LEVEL_WARN:
    return RTT_CTRL_TEXT_BRIGHT_YELLOW "[WARN] ";
  case RTT_LOG_LEVEL_ERROR:
    return RTT_CTRL_TEXT_BRIGHT_RED "[ERROR] ";
  default:
    return "";
  }
#else
  switch (Level) {
  case RTT_LOG_LEVEL_INFO:
    return "[INFO] ";
  case RTT_LOG_LEVEL_DEBUG:
    return "[DEBUG] ";
  case RTT_LOG_LEVEL_WARN:
    return "[WARN] ";
  case RTT_LOG_LEVEL_ERROR:
    return "[ERROR] ";
  default:
    return "";
  }
#endif
}

/*
 * 所有日志均以换行结束。彩色级别日志还需先复位终端属性，避免颜色影响
 * 后续 RTT 输出；PRINT 未设置颜色，因此无需发送复位序列。
 */
static const char * rtt_log_suffix(RTT_LOG_LEVEL Level) {
#if LOG_ENABLE_LITE
  (void)Level;
#elif RTT_LOG_USE_COLOR
  if (Level != RTT_LOG_LEVEL_PRINT) {
    return RTT_CTRL_RESET "\n";
  }
#else
  (void)Level;
#endif
  return "\n";
}

int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...) {
  int Result;
  va_list ParamList;

  va_start(ParamList, pFormat);
  /* 前缀、正文和后缀由同一次格式化过程连续写入，保持一条日志的帧结构。 */
  Result = RTT_vprintfFramed(RTT_LOG_BUFFER_INDEX,
                             rtt_log_prefix(Level),
                             pFormat,
                             &ParamList,
                             rtt_log_suffix(Level));
  va_end(ParamList);
  return Result;
}

int RTT_LogString(const char * pText) {
#if LOG_ENABLE_STRING
  unsigned Length;

  if (pText == NULL) {
    return -1;
  }
  Length = SEGGER_RTT_WriteString(RTT_LOG_BUFFER_INDEX, pText);
  return (int)Length;
#else
  (void)pText;
  return 0;
#endif
}

#else

/*
 * 保留禁用配置下的函数符号，兼容直接调用 RTT_LogPrintf 的代码；正常使用
 * log_* 宏时调用会在预处理阶段被完全移除。
 */
int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...) {
  (void)Level;
  (void)pFormat;
  return 0;
}

int RTT_LogString(const char * pText) {
  (void)pText;
  return 0;
}

#endif
