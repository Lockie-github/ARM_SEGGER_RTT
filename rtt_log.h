#ifndef ARM_SEGGER_RTT_LOG_H
#define ARM_SEGGER_RTT_LOG_H

#include <rtt_cfg.h>

#include "SEGGER_RTT.h"

#ifndef RTT_LOG_ENABLE
  #define RTT_LOG_ENABLE 1
#endif
#ifndef LOG_ENABLE_INFO
  #define LOG_ENABLE_INFO  1
#endif
#ifndef LOG_ENABLE_ERROR
  #define LOG_ENABLE_ERROR 1
#endif
#ifndef LOG_ENABLE_DEBUG
  #define LOG_ENABLE_DEBUG 1
#endif
#ifndef LOG_ENABLE_WARN
  #define LOG_ENABLE_WARN  1
#endif
#ifndef LOG_ENABLE_PRINT
  #define LOG_ENABLE_PRINT 1
#endif
#ifndef LOG_ENABLE_FLOAT
  #define LOG_ENABLE_FLOAT 1
#endif
#ifndef LOG_ENABLE_LITE
  #define LOG_ENABLE_LITE  0
#endif

#ifndef RTT_LOG_BUFFER_INDEX
  #define RTT_LOG_BUFFER_INDEX 0u
#endif
#ifndef RTT_LOG_USE_COLOR
  #define RTT_LOG_USE_COLOR 1
#endif

#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  #include "rtt_float.h"
#endif

#if defined(__GNUC__) || defined(__clang__)
  #define RTT_LOG_FORMAT_ATTRIBUTE(FormatIndex, FirstArgument) \
    __attribute__((format(printf, FormatIndex, FirstArgument)))
#else
  #define RTT_LOG_FORMAT_ATTRIBUTE(FormatIndex, FirstArgument)
#endif

typedef enum {
  RTT_LOG_LEVEL_INFO,
  RTT_LOG_LEVEL_DEBUG,
  RTT_LOG_LEVEL_WARN,
  RTT_LOG_LEVEL_ERROR,
  RTT_LOG_LEVEL_PRINT
} RTT_LOG_LEVEL;

#ifdef __cplusplus
extern "C" {
#endif

int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...)
  RTT_LOG_FORMAT_ATTRIBUTE(2, 3);

#undef RTT_LOG_FORMAT_ATTRIBUTE

#ifdef __cplusplus
}
#endif

#if RTT_LOG_ENABLE
  #if LOG_ENABLE_INFO
    #if LOG_ENABLE_LITE
      #define log_info(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, __VA_ARGS__); } while (0)
    #else
      #define log_info(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_INFO, __VA_ARGS__); } while (0)
    #endif
  #else
    #define log_info(...) do {} while (0)
  #endif

  #if LOG_ENABLE_DEBUG
    #if LOG_ENABLE_LITE
      #define log_debug(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, __VA_ARGS__); } while (0)
    #else
      #define log_debug(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_DEBUG, __VA_ARGS__); } while (0)
    #endif
  #else
    #define log_debug(...) do {} while (0)
  #endif

  #if LOG_ENABLE_WARN
    #if LOG_ENABLE_LITE
      #define log_warn(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, __VA_ARGS__); } while (0)
    #else
      #define log_warn(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_WARN, __VA_ARGS__); } while (0)
    #endif
  #else
    #define log_warn(...) do {} while (0)
  #endif

  #if LOG_ENABLE_ERROR
    #if LOG_ENABLE_LITE
      #define log_err(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, __VA_ARGS__); } while (0)
    #else
      #define log_err(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_ERROR, __VA_ARGS__); } while (0)
    #endif
  #else
    #define log_err(...) do {} while (0)
  #endif

  #if LOG_ENABLE_PRINT
    #define log_print(...) do { (void)RTT_LogPrintf(RTT_LOG_LEVEL_PRINT, __VA_ARGS__); } while (0)
  #else
    #define log_print(...) do {} while (0)
  #endif

#else
  #define log_info(...)  do {} while (0)
  #define log_debug(...) do {} while (0)
  #define log_warn(...)  do {} while (0)
  #define log_err(...)   do {} while (0)
  #define log_print(...) do {} while (0)
#endif

#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  #define log_float(Value) \
    do { RTT_LogFloat3((float)(Value), NULL); } while (0)
  #define log_float_desc(Description, Value) \
    do { RTT_LogFloat3((float)(Value), (Description)); } while (0)
#else
  #define log_float(Value) do {} while (0)
  #define log_float_desc(Description, Value) do {} while (0)
#endif

#endif
