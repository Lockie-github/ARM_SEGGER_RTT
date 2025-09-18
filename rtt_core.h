#ifndef __RTT_CORE_H__
#define __RTT_CORE_H__

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
#define __LOG_FLOAT_PROCESS(value, then_do) \
    do { \
        float __f_val = (float)(value); \
        int __int_part_abs = (int)(__f_val < 0 ? -__f_val : __f_val); \
        const char* __sign_str = (__f_val < 0) ? "-" : ""; \
        float __frac = __f_val - (int)__f_val; \
        int __dec_part = (int)(__frac * 1000.0f + (__frac < 0 ? -0.5f : 0.5f)); \
        if (__dec_part < 0) __dec_part = -__dec_part; \
        if (__dec_part > 999) __dec_part = 999; \
        then_do; \
    } while(0)

#endif // __RTT_CORE_H__