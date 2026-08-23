#ifndef ARM_SEGGER_RTT_LOG_H
#define ARM_SEGGER_RTT_LOG_H

#include <rtt_cfg.h>

#include "SEGGER_RTT.h"

#include <stdint.h>

/*
 * 日志配置分为三层：
 * 1. RTT_LOG_ENABLE 控制整个日志模块；
 * 2. LOG_ENABLE_LITE 控制是否省略级别前缀和终端颜色；
 * 3. LOG_ENABLE_xxx 分别控制各级别、字符串及浮点日志。
 *
 * 应用可在自己的 rtt_cfg.h 或编译选项中预先定义这些宏。这里仅提供
 * 默认值，因而无需修改本文件即可裁剪不需要的日志代码。
 */
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
#ifndef LOG_ENABLE_STRING
  #define LOG_ENABLE_STRING 1
#endif
#ifndef LOG_ENABLE_FLOAT
  #define LOG_ENABLE_FLOAT 1
#endif
#ifndef LOG_ENABLE_TYPED
  #define LOG_ENABLE_TYPED 0
#endif
#ifndef LOG_ENABLE_LITE
  #define LOG_ENABLE_LITE  0
#endif

/* 日志写入的 RTT Up Buffer 通道号，默认使用终端通道 0。 */
#ifndef RTT_LOG_BUFFER_INDEX
  #define RTT_LOG_BUFFER_INDEX 0u
#endif
/* 非轻量模式下是否输出 ANSI 颜色控制序列。 */
#ifndef RTT_LOG_USE_COLOR
  #define RTT_LOG_USE_COLOR 1
#endif

#if RTT_LOG_ENABLE && \
    (RTT_LOG_BUFFER_INDEX >= SEGGER_RTT_MAX_NUM_UP_BUFFERS)
  #error "RTT_LOG_BUFFER_INDEX must be less than SEGGER_RTT_MAX_NUM_UP_BUFFERS"
#endif

/* 关闭日志或浮点日志后，不再引入浮点格式化接口。 */
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  #include "rtt_float.h"
#endif

/* 让 GCC/Clang 在编译期检查 RTT_LogPrintf 的格式串和可变参数。 */
#if defined(__GNUC__) || defined(__clang__)
  #define RTT_LOG_FORMAT_ATTRIBUTE(FormatIndex, FirstArgument) \
    __attribute__((format(printf, FormatIndex, FirstArgument)))
#else
  #define RTT_LOG_FORMAT_ATTRIBUTE(FormatIndex, FirstArgument)
#endif

/* PRINT 用于无级别前缀的普通输出，其余枚举值对应带级别的日志。 */
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

/**
 * @brief 按指定级别格式化日志，并写入 RTT_LOG_BUFFER_INDEX 通道。
 *
 * 非轻量模式会根据 Level 添加级别前缀；启用颜色时还会添加颜色和复位
 * 控制序列。每条日志统一以换行符结尾。RTT_LOG_LEVEL_PRINT 只输出正文
 * 和换行符，不添加级别或颜色。
 *
 * @param Level   日志级别。
 * @param pFormat printf 风格格式字符串。
 * @return 格式化及写入结果；模块关闭时固定返回 0。
 */
int RTT_LogPrintf(RTT_LOG_LEVEL Level, const char * pFormat, ...)
  RTT_LOG_FORMAT_ATTRIBUTE(2, 3);

/**
 * @brief Internal implementation for the log_string macro.
 *
 * A NULL pointer is rejected with -1. Applications should use log_string()
 * instead of calling this implementation directly.
 */
int RTT_LogString(const char * pText);

#if RTT_LOG_ENABLE && LOG_ENABLE_TYPED
/** @brief Write a labeled signed 32-bit decimal value and a newline. */
int RTT_LogI32(const char * pLabel, int32_t Value);

/** @brief Write a labeled unsigned 32-bit decimal value and a newline. */
int RTT_LogU32(const char * pLabel, uint32_t Value);

/** @brief Write a labeled, fixed-width uppercase hexadecimal value. */
int RTT_LogHex32(const char * pLabel, uint32_t Value);

/** @brief Write a labeled pointer using the target's full pointer width. */
int RTT_LogPointer(const char * pLabel, const void * pValue);
#endif

#undef RTT_LOG_FORMAT_ATTRIBUTE

#ifdef __cplusplus
}
#endif

/*
 * 对外优先使用以下宏，而不是直接调用 RTT_LogPrintf。关闭某个级别后，
 * 对应宏展开为空语句，传入的表达式也不会被求值。do-while(0) 使宏在
 * if/else 等语句中保持与普通函数调用相同的使用方式。
 *
 * 轻量模式仍保留各级别的独立开关，但统一按 PRINT 输出，省略级别前缀
 * 和颜色，从而减小固件体积。
 */
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
    #define log_print(...) do { (void)SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, __VA_ARGS__); } while (0)
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

#if RTT_LOG_ENABLE && LOG_ENABLE_STRING
  #define log_string(Text) (RTT_LogString((Text)))
#else
  #define log_string(Text) (0)
#endif

/*
 * Fixed-format typed logging bypasses the general formatter. Each call adds
 * one newline; a NULL or empty label writes only the value.
 */
#if RTT_LOG_ENABLE && LOG_ENABLE_TYPED
  #define log_i32(Label, Value) \
    do { (void)RTT_LogI32((Label), (int32_t)(Value)); } while (0)
  #define log_u32(Label, Value) \
    do { (void)RTT_LogU32((Label), (uint32_t)(Value)); } while (0)
  #define log_hex32(Label, Value) \
    do { (void)RTT_LogHex32((Label), (uint32_t)(Value)); } while (0)
  #define log_pointer(Label, Value) \
    do { (void)RTT_LogPointer((Label), (const void *)(Value)); } while (0)
#else
  #define log_i32(Label, Value) do {} while (0)
  #define log_u32(Label, Value) do {} while (0)
  #define log_hex32(Label, Value) do {} while (0)
  #define log_pointer(Label, Value) do {} while (0)
#endif

/*
 * 浮点日志固定输出三位小数。Label 非空时输出 "Label: Value\n"，
 * 否则只输出 "Value\n"。
 */
#if RTT_LOG_ENABLE && LOG_ENABLE_FLOAT
  #define log_float(Value) \
    do { RTT_LogFloat3((float)(Value), NULL); } while (0)
  #define log_float_label(Label, Value) \
    do { RTT_LogFloat3((float)(Value), (Label)); } while (0)
#else
  #define log_float(Value) do {} while (0)
  #define log_float_label(Label, Value) do {} while (0)
#endif

#endif
