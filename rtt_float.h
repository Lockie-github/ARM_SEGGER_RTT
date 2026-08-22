#ifndef RTT_FLOAT_H
#define RTT_FLOAT_H

#include <rtt_cfg.h>

/*
 * 置 1 后允许使用 modff 拆分浮点数，适用于目标芯片和编译选项均启用
 * 硬件 FPU 的工程；否则使用不依赖浮点运行库的 IEEE-754 位解析路径。
 */
#ifndef HARD_FPU_ENABLE
  #define HARD_FPU_ENABLE 0
#endif

/*
 * 置 1 后，正常有限浮点数会直接组装完整帧并单次写入 RTT，以额外
 * Flash 占用换取更高的输出效率；默认使用兼容格式化路径。
 */
#ifndef RTT_LOG_FLOAT_FAST_PATH
  #define RTT_LOG_FLOAT_FAST_PATH 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 以固定三位小数输出一个单精度浮点数。
 *
 * 小数部分直接截断到三位，不执行四舍五入。NaN、正负无穷及超出
 * uint32_t 整数部分表示范围的值，分别输出对应的可读文本。
 *
 * @param Value        待输出的单精度浮点数。
 * @param sDescription 可选说明文字；传入 NULL 时只输出数值。
 */
void RTT_LogFloat3(float Value, const char * sDescription);

#ifdef __cplusplus
}
#endif

#endif /* RTT_FLOAT_H */
