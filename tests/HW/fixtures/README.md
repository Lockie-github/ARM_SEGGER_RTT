# HW fixture 目录

这里是八个实际 STM32 工程所用目标侧测试源码的唯一维护位置。应用工程不保存 fixture
副本，也不需要包含 `hw_suite.h` 或调用测试函数。runner 通过 Make/CMake overlay 将
`support/hw_test_entry.c` 和对应 MCU fixture 加入 ELF；烧录并完成原工程初始化后，再通过
J-Link 把 PC 设置到 `HW_TestEntry`。

| 目录 | 目标 | 用例 |
|---|---|---|
| `stm32f0/` | STM32F042G6 / Cortex-M0 | HW02～HW07、两种 HW08 |
| `stm32f1/` | STM32F103C8 / Cortex-M3 | HW02～HW08 |
| `stm32f4/` | STM32F411CE / Cortex-M4F | HW02～HW07、两种 HW08 |
| `stm32h7/` | STM32H7B0VB / Cortex-M7 | HW02～HW08 |

HW01 的通用启动与心跳 marker 位于 `hw01_fixture.h`。它不承担烧录和重连判定；这些
主机侧步骤由 `run_hw.py` 完成。

`hw_suite.h` 根据 `HW_TEST_CASE=1..8` 选择 fixture，并把统一的工程名、构建类型、
profile 和缓冲参数映射到现有 fixture 宏。HW07 由 overlay 重命名工程 TIM1 handler，
测试入口提供代理 handler：先执行 fixture hook，再调用原工程 handler。库配置由
`../rtt_test_config.h` 同步选择，确保应用和 RTT 库使用同一组功能开关与缓冲布局。

F042 HW08 是协议例外：Make Release 使用 `hw08_gated_throughput.h`，CMake 使用
`hw08_marker.h`。选择由 runner 传入 `HW08_GATED_THROUGHPUT`，不再修改 `main.c`。

F411 HW08 默认使用 `hw08_fixture.inc` 完成性能、运行时栈和表征吞吐；正式 0.10 MiB/s
资格组合使用 `hw08_gated_throughput.inc`。`release_matrix.json` 通过
`throughput_qualification` 启用后者，并要求三次独立运行；命令行开关只用于定向诊断。

formatter、typed integer、legacy float fast-off/on 和 Skip C/ASM 的静态资源测量不需要
开发板，现统一由 NH10 负责；HW08 fixture 只保留真机运行才能得到的周期、吞吐和运行时
栈证据。

这些 fixture 包含 MCU 时钟、计时器、Cache、复位寄存器和 RAM 预算差异。公共生命周期
可以共享，但不能把 F0 的缓冲值或计时实现直接套用到 F1/F4/H7。
