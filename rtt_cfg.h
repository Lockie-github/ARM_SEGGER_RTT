#ifndef ARM_SEGGER_RTT_CFG_H
#define ARM_SEGGER_RTT_CFG_H

/*
 * 将本文件复制到应用工程的配置目录，再取消需要覆盖的配置项注释并修改
 * 取值。CMake 和 Make 集成会优先搜索应用工程的配置目录。
 *
 * 布尔开关统一使用 0（关闭）或 1（开启）。关闭模块或级别后，对应日志宏
 * 展开为空语句，其参数不会求值，也不会产生 RTT 输出代码。
 */

/* 日志模块总开关。 */
// #define RTT_LOG_ENABLE       1     ///< 1：启用全部日志功能；0：关闭日志模块。

/* 日志输出形式。 */
// #define LOG_ENABLE_LITE      0     ///< 1：轻量输出，不添加级别前缀及 ANSI 颜色；0：完整输出。建议在资源紧张的MCU配置此选项为1

/* 各日志接口独立开关，仅在 RTT_LOG_ENABLE 为 1 时生效。 */
// #define LOG_ENABLE_INFO      1     ///< 1：启用 log_info；0：移除信息级日志及其参数求值。
// #define LOG_ENABLE_DEBUG     1     ///< 1：启用 log_debug；0：移除调试级日志及其参数求值。
// #define LOG_ENABLE_WARN      1     ///< 1：启用 log_warn；0：移除警告级日志及其参数求值。
// #define LOG_ENABLE_ERROR     1     ///< 1：启用 log_err；0：移除错误级日志及其参数求值。
// #define LOG_ENABLE_PRINT     1     ///< 1：启用无级别前缀的 log_print；0：移除该接口调用。
// #define LOG_ENABLE_STRING    1     ///< 1：启用 log_string；0：移除字符串 RTT 写入实现。
// #define LOG_ENABLE_FLOAT     1     ///< 1：启用 log_float/log_float_label；0：移除浮点日志代码。

/* 浮点转换实现选择。 */
// #define HARD_FPU_ENABLE      0     ///< 1：使用 modff 硬件浮点路径；仅在芯片及 ABI 均启用 FPU 时设置。

/* 日志输出通道及终端显示。 */
// #define RTT_LOG_BUFFER_INDEX 0u    ///< 日志写入的 RTT Up Buffer 索引，默认终端通道为 0。
// #define RTT_LOG_USE_COLOR    1     ///< 1：完整模式输出 ANSI 颜色；0：只输出纯文本级别前缀。

/* SEGGER RTT 通道及静态缓冲区配置。 */
// #define SEGGER_RTT_MAX_NUM_UP_BUFFERS   3     ///< Target 到 Host 的 Up Buffer 总数，须大于日志通道索引。
// #define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 3     ///< Host 到 Target 的 Down Buffer 总数；不用下行通道时仍需保留通道 0。
// #define BUFFER_SIZE_UP                  1024  ///< 默认 Up Buffer 容量（字节）；增大可降低高频日志丢失概率。
// #define BUFFER_SIZE_DOWN                16    ///< 默认 Down Buffer 容量（字节），用于主机向目标发送数据。
// #define SEGGER_RTT_PRINTF_BUFFER_SIZE   64u   ///< 格式化临时缓冲区容量（字节），不限制单条日志总长度。

#endif
