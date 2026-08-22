# 更新记录

## 目录

- [未发布](#未发布)
- [2.1.0 - 2026-07-30](#210---2026-07-30)
- [2.0.2 - 2026-07-01](#202---2026-07-01)
- [2.0.1 - 2026-04-07](#201---2026-04-07)
- [2.0.0 - 2026-03-27](#200---2026-03-27)
- [1.0.2](#102)
- [1.0.1](#101)
- [1.0.0](#100)

## 未发布

### Added

- 新增日志总开关、轻量模式和各日志接口的分层配置
- 新增项目级 `rtt_cfg.h` 配置覆盖；CMake/Make 优先使用应用工程配置目录，
  修改配置后需清理并重新编译 RTT 对象
- 新增标签在前的 `log_float_label(Label, Value)`、可配置输出通道和 ANSI 颜色开关
- 新增可独立裁剪的 `log_string(Text)` 原样字符串输出接口及
  `LOG_ENABLE_STRING` 开关
- 新增 RTT C 源的可配置 `-Os` 优化选项
- 新增 `RTT_LOG_FLOAT_FAST_PATH` 编译期开关；默认使用兼容格式化路径以控制 Flash
  占用，开启后以额外 Flash 换取常规有限浮点日志的输出效率
- 新增 `RTT_WRITE_SKIP_USE_ASM` 独立分发开关；默认保留 Up Buffer `NO_BLOCK_SKIP`
  的 C 写入路径，仅在该开关与 `RTT_USE_ASM` 同时启用时使用 ARMv7-M 汇编实现

### Changed

- 浮点实现由头文件移至 `rtt_float.c`，减少各调用点的重复代码
- 分级日志统一通过前缀、正文和后缀格式化流程输出
- 精简格式化器支持长消息分块和实际字符数返回，并完善整数边界、字段宽度、精度、字符串及指针处理
- 无 FPU 浮点路径改为解析 IEEE-754 binary32，避免 Cortex-M0 引入除法辅助函数
- 浮点小数改为固定三位截断；超出 `uint32_t` 整数范围时输出
  `Overflow`/`-Overflow`，无 FPU 路径将负零及绝对值小于 `0.001` 的负数
  规范为 `0.000`
- FPU 路径由编译器宏自动检测改为通过 `HARD_FPU_ENABLE` 显式选择，默认使用
  不依赖 `modff` 的无 FPU 实现
- 轻量模式只控制输出形式，不再隐式关闭各等级日志
- 日志写入失败时不再最多重试 100 次，而是立即返回错误，降低缓冲区满时的
  CPU 占用和不可预测延迟，并避免部分写入后重试造成重复输出
- 不支持的格式转换现在按原文本输出且不消费参数；不再兼容 `#` 标志及
  `h`、`l` 等长度修饰符，`%p` 改为按目标指针位宽输出
- CMake/Make 集成改为编译精简版 `rtt_printf.c`、`rtt_log.c` 和
  `rtt_float.c`，不再将原始 `RTT/SEGGER_RTT_printf.c` 加入 RTT 库目标
- Make 集成补充大写 `.S` 汇编源规则，修复全新构建缺少 RTT 汇编对象的问题
- `log_print(Format, ...)` 改为直接调用 `SEGGER_RTT_printf`，作为需要格式化参数时
  的高效率日志接口；该接口不自动换行，调用方必须在格式串中显式写入 `\n`
- 拆分无前后缀的 raw formatter 与分级日志 framed formatter，降低
  `log_print` 的调用周期和动态栈；格式输出、返回值、长消息分块及 RTT 短写
  语义保持不变。该优化不增加静态 RAM；相较优化前的精简格式化器，代价是
  小幅增加 Flash，并可能增加普通等级日志非数字格式路径的动态栈

### Fixed

- 修复浮点快速直写路径在 Cortex-M0/M0+ 上引入 `__aeabi_uidiv`、
  `__aeabi_uidivmod` 和 `__udivsi3` 的问题；无硬件整数除法时改用移位加减法，
  支持硬件整数除法的目标保持原路径
- 修复硬 FPU 路径对负零及绝对值小于 `0.001` 的负数保留负号的问题，统一
  软硬浮点路径的三位截断输出
- 增加日志通道索引的编译期边界检查，日志启用时拒绝
  `RTT_LOG_BUFFER_INDEX` 超出 Up Buffer 数量的配置
- 收紧格式化器和软浮点转换中的局部变量作用域及只读指针声明，消除未豁免的
  Cppcheck style 提示

### Removed

- 移除 `log_float_desc(Description, Value)`；使用参数顺序相同的
  `log_float_label(Label, Value)` 替代
- 移除内部浮点宏头文件 `rtt_core.h`；浮点转换改由 `rtt_float.c` 实现

## [2.1.0] - 2026-07-30

### Added

- 新增 `dr` 和 `rr` 目标，支持 Debug/Release 固件编译、烧录后直接运行
- 新增 STM32Cube CMake 工程配置、编译、烧录说明和常见问题章节

### Changed

- 修订记录和更新记录改为倒序排列，优先显示最新版本

## [2.0.2] - 2026-07-01

### Fixed

- 修复芯片型号带特殊版本后缀时自动获取 `MCU_ID` 错误的问题

## [2.0.1] - 2026-04-07

### Changed

- 调整 `segger_rtt.mk` 的变量命名，使语义更清晰并接近 STM32CubeMX 风格

## [2.0.0] - 2026-03-27

### Added

- 新增对 CMake 的支持

### Changed

- 修改下载脚本以兼容 CMake 与 Make；该脚本不再兼容 V1.0.0

## [1.0.2]

1. 添加带时间戳的日志，需要安装 `ts`
2. 若不方便安装 `ts`，也可使用 Bash 或 `gawk`

```bash
make rtt | while IFS= read -r line; do
  echo "$(date '+%Y-%m-%d %H:%M:%S') $line"
done | tee "$(RTT_LOGFILE)"
```

```bash
make rtt | gawk '{ print strftime("%Y-%m-%d %H:%M:%S"), $0 }' | tee "$(RTT_LOGFILE)"
```

## [1.0.1]

1. 添加有 FPU 的 MCU 浮点处理逻辑，提高性能
2. 增加浮点型 NaN 等特殊值的处理

## [1.0.0]

1. 源自 SEGGER RTT 8.64a
2. 添加分级日志功能，全部开启时 Flash 占用约 7.7 KB
   1. 添加浮点打印支持
   2. 拥有超时机制
   3. 拥有颜色等级区分
3. 添加 lite 模式，Flash 占用约 3.9 KB
   1. 仅保留基础打印功能
   2. 该功能开启后，除浮点外的分级日志将全部关闭
