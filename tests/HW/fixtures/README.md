# HW fixture 目录

这里是八个实际 STM32 工程目标侧测试源码的唯一维护位置。应用工程不保存 fixture 副本，
只包含对应 MCU 的 `hw_suite.h`，并稳定调用 `HW_TestInit()`、`HW_TestRun()`。

| 目录 | 目标 | 用例 |
|---|---|---|
| `stm32f0/` | STM32F042G6 / Cortex-M0 | HW02～HW07、两种 HW08 |
| `stm32f1/` | STM32F103C8 / Cortex-M3 | HW02～HW08 |
| `stm32f4/` | STM32F411CE / Cortex-M4F | HW02～HW07、两种 HW08 |
| `stm32h7/` | STM32H7B0VB / Cortex-M7 | HW02～HW08 |

HW01 的通用启动与心跳 marker 位于 `hw01_fixture.h`。它不承担烧录和重连判定；这些
主机侧步骤由 `run_hw.py` 完成。

`hw_suite.h` 根据 `HW_TEST_CASE=1..8` 选择 fixture，并把统一的工程名、构建类型、
profile 和缓冲参数映射到历史 fixture 宏。HW07 的 TIM1 ISR 入口由工程中断文件在
`HW_TEST_CASE == 7` 时调用。库配置由 `../rtt_test_config.h` 同步选择，确保应用和
RTT 库使用同一组功能开关与缓冲布局。

F042 HW08 是协议例外：Make Release 使用 `hw08_gated_throughput.h`，CMake 使用
`hw08_marker.h`。选择由 runner 传入 `HW08_GATED_THROUGHPUT`，不再修改 `main.c`。

F411 HW08 默认使用 `hw08_fixture.inc` 完成性能、资源、栈和表征吞吐；正式 0.10 MiB/s
资格测试使用 `hw08_gated_throughput.inc`。runner 仅在显式指定
`--throughput-qualification` 时启用后者，并要求三次独立运行。

这些 fixture 包含 MCU 时钟、计时器、Cache、复位寄存器和 RAM 预算差异。公共生命周期
可以共享，但不能把 F0 的缓冲值或计时实现直接套用到 F1/F4/H7。
