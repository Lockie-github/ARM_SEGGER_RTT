#ifndef RTT_FLOAT_H
#define RTT_FLOAT_H

#include <rtt_cfg.h>

#ifdef HARD_FPU_ENABLE
  #error "HARD_FPU_ENABLE was renamed to RTT_FLOAT_USE_MODFF"
#endif

/*
 * 当前固定三位小数接口默认使用紧凑的 IEEE-754 binary32 位解析实现。
 * modff 路径不作为性能优化，仅为未来的动态小数位数接口、非 IEEE-754
 * 平台或更通用的浮点格式化需求保留。
 */
#ifndef RTT_FLOAT_USE_MODFF
  #define RTT_FLOAT_USE_MODFF 0
#endif

/*
 * 置 1 后，正常有限浮点数会直接组装完整帧并单次写入 RTT，以额外
 * Flash 占用换取更高的输出效率；默认使用兼容格式化路径。
 */
#ifndef RTT_LOG_FLOAT_FAST_PATH
  #define RTT_LOG_FLOAT_FAST_PATH 0
#endif

#ifndef LOG_ENABLE_TYPED_FLOAT
  #define LOG_ENABLE_TYPED_FLOAT 0
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

#if RTT_LOG_ENABLE && LOG_ENABLE_TYPED_FLOAT
/**
 * @brief 以 typed 规则输出固定三位小数的单精度浮点数。
 *
 * 标签最多输出前 46 B，超出部分直接截断；数值部分始终完整。整帧通过
 * 一次非格式化 RTT 写入提交，不受 RTT_LOG_FLOAT_FAST_PATH 影响。
 *
 * @param pLabel 可选标签；NULL 或空字符串表示无标签。
 * @param Value  待输出的单精度浮点数。
 * @return 成功时返回实际写入字节数，短写时返回 -1。
 */
int RTT_LogF32(const char * pLabel, float Value);
#endif

#ifdef __cplusplus
}
#endif

#endif /* RTT_FLOAT_H */
