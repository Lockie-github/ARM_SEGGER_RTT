#ifndef RTT_CORE_H
#define RTT_CORE_H

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

#ifdef __cplusplus
extern "C" {
#endif

void RTT_LogFloat3(float Value, const char * sDescription);

#ifdef __cplusplus
}
#endif

#endif // RTT_CORE_H
