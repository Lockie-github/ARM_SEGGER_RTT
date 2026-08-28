> **文档版本：3.0.0（2026/08/28）｜最近变化：重构 README 与 `docs/` 文档导航，并补充快速开始指南。详情见 [修订记录](#修订记录)。**

# 目录
- [目录](#目录)
- [描述](#描述)
- [快速开始](#快速开始)
- [移植](#移植)
- [日志配置](#日志配置)
- [API](#api)
- [对源码的修改](#对源码的修改)
- [修订记录](#修订记录)
- [更新记录](#更新记录)

---

# 描述

1. 本仓库基于 SEGGER RTT 8.64a，为 Cortex-M 目标提供可配置的 RTT 日志接口。
2. 编译和链接本库不依赖 J-Link。查看 RTT 输出需要支持 RTT 的调试探针及主机工具。
3. 本仓库提供 J-Link Commander、RTT Telnet、烧录和擦除辅助命令，
   相关脚本位于 `ARM_SEGGER_RTT/jlinkscript`。
4. 支持 STM32CubeMX 生成的 Make 工程，移植方式见 [Make](docs/port.md#make)。
5. 支持 STM32CubeMX 生成的 CMake 工程，以及 STM32 VS Code 插件提供的
   `cube-cmake`，移植方式见 [CMake](docs/port.md#cmake)。
6. 建议将本仓库作为 Git submodule 引入主工程。

---

# 快速开始

请查阅 [快速开始](docs/quick-start.md)

---

# 移植

Make、CMake 工程的接入方法，以及配置、编译、烧录和常见问题，请查看[移植指南](docs/port.md)。

---

# 日志配置

日志总开关默认已开启。
不确定应使用 Full、Lite、Typed 还是 Float 时，请先查看[输出模式指南](docs/Mode%20declaration.md)。
全部配置宏、默认值、推荐组合和配置覆盖规则，请查看[日志配置指南](docs/Configuration.md)。

---

# API

应用代码包含 `rtt_log.h` 后即可输出日志：

```c
#include "rtt_log.h"

log_info("system ready");
log_debug("counter=%u", 42u);
log_warn("voltage=%u mV", 3250u);
log_err("status=%d", -1);
```

完整 API 列表、输出规则、格式化支持和边界行为，请查看[API 指南](docs/API%20declaration.md)。

---

# 对源码的修改

此章仅记录相较于SEGGER RTT 8.64a的RTT部分的源码做出的修改,方便以后使用

| 文件与位置 | 修改内容 | 目的 | 引入提交 |
|---|---|---|---|
| `RTT/SEGGER_RTT_Conf.h`：配置声明区 | 引入 `rtt_cfg.h`，并将 `RTT_WRITE_SKIP_USE_ASM` 默认设为 `0` | 支持应用工程统一配置 RTT，同时保持默认 C 写入路径不变 | `d497659` 引入配置入口；`cc65eb3` 增加 Skip 汇编开关 |
| `RTT/SEGGER_RTT.c`：`SEGGER_RTT_WriteNoLock()` 的 `SEGGER_RTT_MODE_NO_BLOCK_SKIP` 分支 | Skip 模式可按配置调用 `SEGGER_RTT_WriteSkipNoLock()` 汇编实现，并转换返回值、处理零长度写入 | 在支持汇编的 Cortex-M 目标上在极短包、特定缓冲状态和特定构建中可能缩短非阻塞写入路径，同时保持 `SEGGER_RTT_WriteNoLock()` 的返回语义 | `2227ae9` 引入汇编路径；`cc65eb3` 增加独立开关 |
| `RTT/SEGGER_RTT_printf.c`：`_PrintInt()` | 在无符号域计算负数绝对值 | 避免格式化 `INT_MIN` 时发生有符号溢出 | `2ec88a7` |
| `RTT/SEGGER_RTT_printf.c`：`SEGGER_RTT_vprintf()` 的动态精度解析 | 动态精度先按 `int` 读取，负值视为未指定精度 | 使 `%.*s` 的负精度行为正确 | `2ec88a7` |
| `RTT/SEGGER_RTT_printf.c`：`SEGGER_RTT_vprintf()` 的转换符解析 | 遇到结尾不完整的转换格式时停止解析 | 避免越过格式字符串结尾读取 | `2ec88a7` |
| `RTT/SEGGER_RTT_printf.c`：`SEGGER_RTT_vprintf()` 的 `%u`、`%x`、`%X`、`%s` 和 `%p` 分支 | 按正确类型读取无符号数和指针；仅在设置字符串精度时递减计数 | 修正可变参数类型并避免无意义的精度回绕 | `2ec88a7` |
| `RTT/SEGGER_RTT_printf.c`：`SEGGER_RTT_vprintf()` 的尾部缓冲写入 | 检查最后一批数据是否完整写入，不再重复累加缓冲长度 | 写入失败时返回 `-1`，成功时返回准确字符数 | `2ec88a7` |

# 修订记录
| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|
|3.0.0|2026/08/28|将 README 重构为文档入口，新增文档版本和最近变化；将 submodule 引入、Make/CMake 接入、首条日志输出和 RTT 连接流程整理至 `docs/quick-start.md`；将移植步骤、RTT 主机连接、迁移验收和常见问题整理至 `docs/port.md`；将模式选择、全部配置项及完整 API 分别整理至 `docs/Mode declaration.md`、`docs/Configuration.md` 和 `docs/API declaration.md`，明确各文档职责与阅读顺序，README 仅保留最小使用示例及对应入口||
|2.1.1|2026/08/21|新增 `RTT_LOG_FLOAT_FAST_PATH` 编译期开关；消除 Cortex-M0 快速路径的整数除法运行库依赖；说明浮点直写性能与目标相关的 Flash 取舍，并补充 `RTT_WRITE_SKIP_USE_ASM` 独立分发开关；新增“对源码的修改”章节，按文件、函数或代码分支记录相对 `release` 的 RTT 源码修改、目的及引入提交||
|2.1.0|2026/08/19|新增 `RTT_USER_CFG_ENABLE` 自定义配置启用方式；补充完整日志、Lite 等级日志和极致精简模式；强化 `log_print`、`log_string` 的效率与 Flash 定位，并完善浮点转换配置说明||
|2.0.0|2026/08/13|重构 README 文档结构，将移植指南和更新记录拆分为独立文档；同步日志配置、API、精简格式化器及构建接入说明||
|1.1.0|2026/07/30|完善 CMake 构建、烧录和常见问题说明，修订记录与更新记录改为倒序排列||
|1.0.1|2026/04/07|修改了移植描述，[位于移植/Make/2.](port.md#make)||
|1.0.0|2026/03/27|更改了文档的结构||

# 更新记录

项目的版本更新内容和历史变更请查看 [CHANGELOG.md](CHANGELOG.md)。
