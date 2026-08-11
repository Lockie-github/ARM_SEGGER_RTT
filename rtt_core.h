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
    #include <stdint.h>

    /* Cortex-M toolchains use IEEE-754 binary32 for float. */
    static void __rtt_float_to_milli(float value, uint32_t* pMilli, unsigned* pNegative, unsigned* pSpecial) {
        union {
            float f;
            uint32_t u;
        } Bits;
        uint32_t Exponent;
        uint32_t Mantissa;
        uint32_t Milli;
        int Shift;

        Bits.f = value;
        *pNegative = Bits.u >> 31;
        Exponent = (Bits.u >> 23) & 0xFFu;
        Mantissa = Bits.u & 0x7FFFFFu;
        *pSpecial = 0u;
        if (Exponent == 0xFFu) {
            *pSpecial = (Mantissa == 0u) ? 2u : 1u;  /* Inf : NaN */
            return;
        }
        if (Exponent == 0u) {
            *pMilli = 0u;  /* Subnormal values are below 0.001. */
            return;
        }

        Mantissa |= 0x800000u;
        /*
         * Mantissa * 1000 * 2^(Exponent - 150) is equivalent to
         * Mantissa * 125 * 2^(Exponent - 147).  The former product fits
         * in 32 bits, so Cortex-M0 needs neither a 64-bit helper nor FP code.
         */
        Milli = (Mantissa << 7) - (Mantissa << 1) - Mantissa;
        Shift = (int)Exponent - 147;
        if (Shift >= 0) {
            if ((Shift >= 32) || (Milli > (UINT32_MAX >> Shift))) {
                *pSpecial = 3u;  /* The three-decimal fixed-point range overflowed. */
                return;
            }
            Milli <<= Shift;
        } else if (Shift <= -32) {
            Milli = 0u;
        } else {
            Milli >>= (unsigned)-Shift;
        }
        *pMilli = (uint32_t)Milli;
    }

    static void __rtt_milli_to_text(uint32_t Milli, unsigned Negative, char* pText) {
        static const uint32_t _aPlaces[] = {
            1000000000u, 100000000u, 10000000u, 1000000u, 100000u,
            10000u, 1000u, 100u, 10u, 1u
        };
        unsigned Digit;
        unsigned Index;
        unsigned Started;

        if ((Negative != 0u) && (Milli != 0u)) {
            *pText++ = '-';
        }
        Started = 0u;
        for (Index = 0u; Index < 10u; Index++) {
            Digit = 0u;
            while (Milli >= _aPlaces[Index]) {
                Milli -= _aPlaces[Index];
                Digit++;
            }
            if ((Started != 0u) || (Digit != 0u) || (Index == 6u)) {
                *pText++ = (char)('0' + Digit);
                Started = 1u;
            }
            if (Index == 6u) {
                *pText++ = '.';
            }
        }
        *pText = '\0';
    }

    static void __rtt_log_float_text(const char* sDesc, const char* sText) {
        volatile int Timeout = 100;

        while (Timeout-- > 0) {
            int Result;

            if (sDesc != NULL) {
                Result = SEGGER_RTT_printf(0, "%s: %s\n", sDesc, sText);
            } else {
                Result = SEGGER_RTT_printf(0, "%s\n", sText);
            }
            if (Result >= 0) {
                break;
            }
        }
    }

    #define __LOG_FLOAT_PROCESS_FIXED(value, desc) \
        do { \
            uint32_t __milli; \
            unsigned __negative; \
            unsigned __special; \
            char __text[14]; \
            __rtt_float_to_milli((float)(value), &__milli, &__negative, &__special); \
            if (__special == 1u) { \
                __rtt_log_float_text((desc), "NaN"); \
            } else if (__special == 2u) { \
                __rtt_log_float_text((desc), __negative ? "-Inf" : "Inf"); \
            } else if (__special == 3u) { \
                __rtt_log_float_text((desc), __negative ? "-Overflow" : "Overflow"); \
            } else { \
                __rtt_milli_to_text(__milli, __negative, __text); \
                __rtt_log_float_text((desc), __text); \
            } \
        } while(0)
#endif

#endif // __RTT_CORE_H__
