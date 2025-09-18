#ifndef __RTT_H__
#define __RTT_H__

#include "SEGGER_RTT.h"

#define LOG_ENABLE_INFO     1  // Enable info-level logs
#define LOG_ENABLE_ERROR    1  // Enable error-level logs
#define LOG_ENABLE_DEBUG    1  // Enable debug-level logs
#define LOG_ENABLE_WARN     1  // Enable warning-level logs
#define LOG_ENABLE_PRINT    1  // Enable print-level logs
#define LOG_ENABLE_FLOAT    1  // Enable float-value logs

#define LOG_ENABLE_LITE     0  // Enable lite-level logs

#if LOG_ENABLE_FLOAT
    #include "rtt_core.h"
#endif

//
// Info-level log macro
//
#if LOG_ENABLE_INFO
    #define log_info(fmt, ...) \
        do { \
            volatile int __timeout = 100; \
            while (__timeout-- > 0) { \
                int __result = SEGGER_RTT_printf(0, "%s[INFO] " fmt "%s\n", \
                                              RTT_CTRL_TEXT_BRIGHT_GREEN, \
                                              ##__VA_ARGS__, \
                                              RTT_CTRL_RESET); \
                if (__result >= 0) break; \
            } \
        } while(0)
#elif LOG_ENABLE_LITE
    #undef  log_info   
#else
    #define log_info(fmt,...) do {} while(0)
#endif

//
// Debug-level log macro
//
#if LOG_ENABLE_DEBUG
    #define log_debug(fmt, ...) \
        do { \
            volatile int __timeout = 100; \
            while (__timeout-- > 0) { \
                int __result = SEGGER_RTT_printf(0, "%s[DEBUG] " fmt "%s\n", \
                                              RTT_CTRL_TEXT_BRIGHT_BLUE, \
                                              ##__VA_ARGS__, \
                                              RTT_CTRL_RESET); \
                if (__result >= 0) break; \
            } \
        } while(0)
#elif LOG_ENABLE_LITE
    #undef  log_debug   
#else
    #define log_debug(fmt,...) do {} while(0)
#endif

//
// Error-level log macro
//
#if LOG_ENABLE_ERROR
    #define log_err(fmt, ...) \
        do { \
            volatile int __timeout = 100; \
            while (__timeout-- > 0) { \
                int __result = SEGGER_RTT_printf(0, "%s[ERROR] " fmt "%s\n", \
                                              RTT_CTRL_TEXT_BRIGHT_RED, \
                                              ##__VA_ARGS__, \
                                              RTT_CTRL_RESET); \
                if (__result >= 0) break; \
            } \
        } while(0)
#elif LOG_ENABLE_LITE
    #undef  log_err 
#else
    #define log_err(fmt,...) do {} while(0)
#endif

//
// Warning-level log macro
//
#if LOG_ENABLE_WARN
    #define log_warn(fmt, ...) \
        do { \
            volatile int __timeout = 100; \
            while (__timeout-- > 0) { \
                int __result = SEGGER_RTT_printf(0, "%s[WARN] " fmt "%s\n", \
                                              RTT_CTRL_TEXT_BRIGHT_YELLOW, \
                                              ##__VA_ARGS__, \
                                              RTT_CTRL_RESET); \
                if (__result >= 0) break; \
            } \
        } while(0)
#elif LOG_ENABLE_LITE
    #undef  log_warn 
#else
    #define log_warn(fmt,...) do {} while(0)
#endif

//
// Normal print log macro
//
#if LOG_ENABLE_PRINT
    #define log_print(fmt, ...) \
        do { \
            volatile int __timeout = 100; \
            while (__timeout-- > 0) { \
                int __result = SEGGER_RTT_printf(0, fmt "\n", ##__VA_ARGS__); \
                if (__result >= 0) break; \
            } \
        } while(0)
#elif LOG_ENABLE_LITE
    #undef  log_print 
#else
    #define log_print(fmt,...) do {} while(0)
#endif

//
// Float-value logging macros
//
#if LOG_ENABLE_FLOAT
    /**
     * 打印浮点数值
     */
    #define log_float(value) \
        __LOG_FLOAT_PROCESS((value), __LOG_FLOAT_CORE(__sign_str, __int_part_abs, __dec_part))

    /**
     * 打印带描述的浮点数值
     */
    #define log_float_desc(desc, value) \
        __LOG_FLOAT_PROCESS((value), __LOG_FLOAT_DESC_CORE((desc), __sign_str, __int_part_abs, __dec_part))

#else

    #define log_float(value) do {} while(0)
    #define log_float_desc(desc, value) do {} while(0)

#endif // LOG_ENABLE_FLOAT


#if LOG_ENABLE_LITE

    #if 0
        #define log_base(...)   do {SEGGER_RTT_printf(0, __VA_ARGS__);} while(0)
    #else
        #define log_base(fmt, ...) \
        do { \
            SEGGER_RTT_printf(0, fmt "\n", ##__VA_ARGS__); \
        } while(0)
    
    #endif
    
    #define log_err(fmt,...) log_base(fmt,##__VA_ARGS__)
    #define log_warn(fmt,...) log_base(fmt,##__VA_ARGS__)
    #define log_debug(fmt,...) log_base(fmt,##__VA_ARGS__)
    #define log_info(fmt,...) log_base(fmt,##__VA_ARGS__)
    #define log_print(fmt,...) log_base(fmt,##__VA_ARGS__)

#endif


#endif // __RTT_H__

/*************************** End of file ****************************/
