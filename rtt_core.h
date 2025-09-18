#ifndef __RTT_CORE_H__
#define __RTT_CORE_H__

#define PRV_ISNAN(x) ((x) != (x))
#define PRV_ISINF(x) (!PRV_ISNAN(x) && PRV_ISNAN((x) - (x)))
/**
 * =========================================
 * 智能检测：GCC / Clang / Keil AC6 / Keil AC5
 * =========================================
 */

#ifndef HAS_FPU
    #if defined(__ARM_FP) && (__ARM_FP & 4)
        // GCC / Clang / Keil AC6
        #define HAS_FPU 1
        #define FPU_DETECT_METHOD "__ARM_FP"
    #elif defined(__TARGET_FPU_VFP)
        // Keil ARM Compiler 5 (AC5)
        #define HAS_FPU 1
        #define FPU_DETECT_METHOD "__TARGET_FPU_VFP"
    #else
        #define HAS_FPU 0
    #endif
#endif

#if HAS_FPU
    #include <math.h>
#endif

/**
 * 内部辅助宏：打印浮点数核心逻辑（可复用）
 * __sign: 符号字符串，如 "-" 或 ""
 * __int_part: 整数部分绝对值
 * __dec_part: 小数部分（0~999）
 */
#define __LOG_FLOAT_CORE(__sign, __int_part, __dec_part) \
    do { \
        volatile int __timeout = 100; \
        while (__timeout-- > 0) { \
            int __result = SEGGER_RTT_printf(0, "%s%d.%03d\n", (__sign), (__int_part), (__dec_part)); \
            if (__result >= 0) break; \
        } \
    } while(0)

/**
 * 带描述的浮点数打印核心
 */
#define __LOG_FLOAT_DESC_CORE(__desc, __sign, __int_part, __dec_part) \
    do { \
        volatile int __timeout = 100; \
        while (__timeout-- > 0) { \
            int __result = SEGGER_RTT_printf(0, "%s: %s%d.%03d\n", (__desc), (__sign), (__int_part), (__dec_part)); \
            if (__result >= 0) break; \
        } \
    } while(0)

/**
 * 公共浮点处理逻辑
 */
#if HAS_FPU
#define __LOG_FLOAT_PROCESS(value, then_do) \
    do { \
        float __f_val = (float)(value); \
        if (PRV_ISNAN(__f_val)) { \
            SEGGER_RTT_printf(0, "NaN\n"); \
            break; }\
        if (PRV_ISINF(__f_val)) { \
            SEGGER_RTT_printf(0, "%sInf\n", __f_val < 0 ? "-" : ""); \
            break; }\
        float __int_raw = 0.0f; \
        float __frac = modff(__f_val, &__int_raw); \
        int __int_part_abs = (int)(__int_raw < 0 ? -__int_raw : __int_raw); \
        const char* __sign_str = (__f_val < 0) ? "-" : ""; \
        int __dec_part = (int)(__frac * 1000.0f + (__frac < 0 ? -0.5f : 0.5f)); \
        if (__dec_part < 0) __dec_part = -__dec_part; \
        if (__dec_part > 999) __dec_part = 999; \
        then_do; \
    } while(0)

#else 
#define __LOG_FLOAT_PROCESS(value, then_do) \
    do { \
        float __f_val = (float)(value); \
        if (PRV_ISNAN(__f_val)) { \
            SEGGER_RTT_printf(0, "NaN\n"); \
            break; }\
        if (PRV_ISINF(__f_val)) { \
            SEGGER_RTT_printf(0, "%sInf\n", __f_val < 0 ? "-" : ""); \
            break; }\
        int __int_part_abs = (int)(__f_val < 0 ? -__f_val : __f_val); \
        const char* __sign_str = (__f_val < 0) ? "-" : ""; \
        float __frac = __f_val - (int)__f_val; \
        int __dec_part = (int)(__frac * 1000.0f + (__frac < 0 ? -0.5f : 0.5f)); \
        if (__dec_part < 0) __dec_part = -__dec_part; \
        if (__dec_part > 999) __dec_part = 999; \
        then_do; \
    } while(0)
#endif

#endif // __RTT_CORE_H__