#ifndef RTT_FLOAT_H
#define RTT_FLOAT_H

#include <rtt_cfg.h>

#ifndef HARD_FPU_ENABLE
  #define HARD_FPU_ENABLE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

void RTT_LogFloat3(float Value, const char * sDescription);

#ifdef __cplusplus
}
#endif

#endif // RTT_FLOAT_H
