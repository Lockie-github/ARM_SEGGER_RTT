#include "rtt_log.h"
#include "rtt_printf.h"

#include <stdarg.h>

#if RTT_LOG_ENABLE

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
  Result = RTT_vprintfFramed(RTT_LOG_BUFFER_INDEX,
                             rtt_log_prefix(Level),
                             pFormat,
                             &ParamList,
                             rtt_log_suffix(Level));
  va_end(ParamList);
  return Result;
}

#else

int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...) {
  (void)Level;
  (void)pFormat;
  return 0;
}

#endif
