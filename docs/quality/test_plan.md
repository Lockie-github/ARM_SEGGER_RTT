# ARM_SEGGER_RTT 发布测试计划

> 文档日期：2026-09-01
> 文档状态：现行计划，等待候选提交冻结  
> 被测基线：执行最终测试前填写候选提交完整 SHA  
> 适用对象：ARM_SEGGER_RTT 日志库及四款 MCU 的 Make/CMake 集成工程  
> 统一入口：`tests/NH/run_nh.py`、`tests/HW/run_hw.py`

本文档只定义测试范围、执行矩阵和验收标准，不记录测试结果。最终结果、缺陷和发布判定写入独立测试报告，原始证据保存在 `TEST_EVIDENCE/`。

## 1. 范围与发布边界

| 范围 | 项目 | 发布属性 |
|---|---|---|
| 非硬件回归 | NH01-NH12 | 必须全部通过 |
| 板级回归 | HW01-HW08 | 必须按本文矩阵全部通过 |
| 长时间稳定性 | HW09 | 后续扩展，不属于本版本发布阻塞项 |

HW09 保留为 8 h/24 h、Cache 和长期计数闭环的扩展稳定性项目。当前统一 HW runner 及 fixture 只实现 HW01-HW08；在 HW09 runner、目标 fixture、断点恢复和证据协议完成前，不得把临时人工运行标记为 HW09 PASS，也不得在发布说明中声明已完成长期稳定性验证。

## 2. 测试对象

### 2.1 工程矩阵

工程路径由 `tests/NH/config.local.json` 的 `workspace` 或自动加载且不提交的 `tests/HW/config.local.json` 配置；版本化配置不保存某台机器的绝对工程或工具链路径。HW 工具链可由本地配置、`HW_TOOLCHAIN_BIN`/`HW_NM` 环境变量或 `--toolchain-bin`/`--nm` 参数指定，按命令行、环境变量、本地配置、可移植默认值的顺序覆盖。八个外部工程是为 RTT 库提供真实 Make/CMake、MCU 和工具链环境的本地参考工程，不属于本仓库发布物，不要求上传、固定提交或保持干净工作树。测试结论表示 RTT 库已在报告所列组合中完成验证，不把该组合限定为用户必须采用的工程配置。

| MCU | 架构特征 | Make 目标 | CMake 目标 |
|---|---|---|---|
| STM32F042G6 | Cortex-M0，无 FPU、无 ARMv7-M RTT 汇编 | `f042_make` | `f042_cmake` |
| STM32F103C8 | Cortex-M3，无 FPU、支持 ARMv7-M RTT 汇编 | `f103_make` | `f103_cmake` |
| STM32F411CE | Cortex-M4F，硬件 FPU、支持 ARMv7-M RTT 汇编 | `f411_make` | `f411_cmake` |
| STM32H7B0VB | Cortex-M7，硬件 FPU、Cache、支持 ARMv7-M RTT 汇编 | `h7b0_make` | `h7b0_cmake` |

### 2.2 被测接口与配置

| 类别 | 接口或配置 |
|---|---|
| 等级日志 | `log_info`、`log_debug`、`log_warn`、`log_err` |
| 通用输出 | `log_print`、`log_string` |
| legacy 浮点 | `log_float`、`log_float_label` |
| typed 整数/指针 | `log_i32`、`log_u32`、`log_hex32`、`log_pointer` |
| typed 浮点 | `log_f32` |
| 功能开关 | `RTT_LOG_ENABLE`、各 `LOG_ENABLE_*`、Lite、颜色、日志通道 |
| 实现开关 | `RTT_FLOAT_USE_MODFF`、`RTT_LOG_FLOAT_FAST_PATH`、`RTT_WRITE_SKIP_USE_ASM` |

## 3. 冻结与执行规则

1. 提交主仓库内的产品代码、测试源码、fixture、runner 和本计划后，冻结一个候选提交；最终测试只在该提交上执行。
2. 最终测试开始前，主仓库必须是已记录的干净工作树。八个外部工程只作为本地参考测试环境，其提交、未提交修改和未跟踪配置不作为 NH09、NH10 或 HW 用例的通过条件，也不要求保存或发布完整工程快照。
3. 外部工程实际使用的 `ARM_SEGGER_RTT` 目录必须解析到候选提交。报告记录实际 MCU/架构、Make/CMake、构建类型、工具链版本和影响 RTT 行为的关键配置；这些信息用于界定已验证组合，不用于固定外部工程基线。
4. HW runner 从主仓库读取完整 Git SHA，通过 Make/CMake overlay 注入 `HW_TEST_LIBRARY_SHA`。主仓库工作树不干净时标识追加 `-dirty`；这种结果只可用于预回归，不可作为最终发布证据。
   NH runner 在 `--all` 开始时冻结主仓库完整 Git SHA，正式执行拒绝 dirty 工作树；统一
   summary 和 NH01-NH12 各自 metadata 均记录该 SHA 与 dirty 状态，并在每个用例开始和
   结束时确认候选身份未变化。单用例 dirty 结果只可用于预回归。
5. F042 Make HW08 不输出普通 marker，其固件身份由 runner metadata、随机非零 `run_id`、归档 ELF 哈希和结果结构共同绑定。
6. 测试必须通过统一 runner 启动。不得直接运行带测试宏的工程 Makefile 代替 HW runner，也不得直接把单个 `run.sh` 的输出作为最终 NH 证据。
7. Make/CMake HW 测试使用纯 overlay，不修改工程的 `main.c`、中断源码、Makefile、`CMakeLists.txt`、linker script 或 `rtt_cfg.h`。
8. 每次切换目标、构建类型、profile 或缓冲参数均执行干净构建。同一测试组合开始后，参与该组合的外部工程源码、构建定义和有效配置在组合结束前不得变更；组合之间允许按测试目的使用不同配置。
9. 正式发布证据以候选提交的完整 Git SHA 为最小有效单位，不做跨主仓库提交的影响分析或证据复用。任何主仓库产品代码、runner、fixture、配置、计划或验收判据变更都必须形成新候选提交，并使上一候选的整套 NH/HW 发布证据不能用于新候选；新候选必须完整重跑 NH01-NH12 和第 6 节规定的 HW01-HW08 矩阵。旧证据仍作为其原候选提交的历史记录保留。外部工程配置变化只使依赖原配置的未完成组合或比较失效，不追溯否定已完成且证据闭合的组合。
10. 自动判定必须检查退出码、结束标识、计数闭环及 Fault/FAIL 禁止项。人工终端观察不能作为唯一结论。
11. 失败后保留原证据，修正测试设施时记录原因并使用新目录验证；禁止覆盖或改写原始证据。开发期间可以只定向重跑相关用例用于诊断或预回归，但测试设施修正提交成为新候选后，仍须按第 9 条完整重跑，定向结果不能替代正式发布矩阵。

## 4. 环境和证据

### 4.1 工具要求

| 类别 | 要求 |
|---|---|
| NH 主机质量 | Clang、LLVM `llvm-profdata`/`llvm-cov`、Cppcheck |
| Arm 构建 | Arm GNU Toolchain，记录 GCC、`nm`、`objdump`、`size` 版本 |
| Make 工程 | GNU Make |
| CMake 工程 | STM32 VS Code 插件提供的 `cube-cmake`、Ninja、STM32 Cube 工具 |
| HW 下载与采集 | J-Link、`JLinkExe`、`JLinkRTTClient`；非零 RTT 通道还需要 `nc` |

NH runner 支持 macOS，以及 Python 3 配合 Git Bash 的 Windows 环境。最终报告必须明确实际完成发布验收的主机系统和工具版本；跨平台可运行不等于两个平台都已获得发布证据。

### 4.2 证据目录

```text
TEST_EVIDENCE/
  NH_RUN_<timestamp>/
    SUMMARY.md
    NHxx/metadata.json
    NHxx/run.log
    NHxx/result.txt
    NHxx/artifacts/
  HW_RUN_<timestamp>/
    HWxx/<target>/<build>/profile-<n>/
    HW06/<target>/<build>/up-size-<n>/profile-0/
    HW08/<target>/<build>/<implementation>/profile-0/
```

使用 `--evidence-dir PATH` 将同一验收批次写入明确的新目录。`TEST_EVIDENCE/` 默认不提交；发布报告只提交环境、命令、结果、关键有效配置和证据索引。外部工程的 Git 状态可以作为诊断信息记录，但不得单独用于改变用例 PASS/FAIL；不要求归档或提交完整外部工程快照。

## 5. NH01-NH12

运行完整非硬件回归：

```sh
python3 tests/NH/run_nh.py --all \
  --evidence-dir TEST_EVIDENCE/NH_RELEASE_<candidate>
```

### NH01 核心质量门

- 以 C11、`-Wall -Wextra -Werror -pedantic` 运行核心主机测试。
- 分别运行 ASan 和 UBSan，stderr 不得出现 sanitizer 诊断。
- 对 `rtt_printf.c`、`rtt_log.c`、`rtt_float.c` 及公开头文件执行 Cppcheck。
- 使用 LLVM 覆盖率统计三个实现文件，总行覆盖率不低于 90%，总分支覆盖率不低于 80%。
- 使用 Arm GCC 为 M0、M3、M4F、M7 分别构建 `-O0`、`-Os` 最终 ELF，共 8 个；M0 不得新增整数除法辅助符号。

### NH02-NH08 API、配置和写入路径

| 用例 | 验证重点 | 验收标准 |
|---|---|---|
| NH02 | 四级日志、颜色、Lite、失败传播和独立裁剪 | 输出逐字节一致；禁用后无输出和参数副作用 |
| NH03 | `log_print`、`log_string`、长字符串、NULL 和功能关闭 | 格式、长度、返回值和禁用行为符合 API 约定 |
| NH04 | formatter 格式、宽度、精度、分段 flush 和 release 对比 | 无截断、越界、重复或错误参数消费 |
| NH05 | legacy/typed 浮点、bit/modff、compat/fast、46/47 B 标签 | 规范输出等价；短写返回错误且不重试 |
| NH06 | 关键配置矩阵、参数副作用、符号和代码裁剪 | 开关互不串扰；非法通道编译失败；依赖可裁剪 |
| NH07 | Make/CMake 自定义 `rtt_cfg.h` 搜索和增量重建 | 配置传播一致；非法配置失败；无旧对象污染 |
| NH08 | Skip C/ASM、0 B、边界、回绕、TRIM/BLOCK | C/ASM 状态等价；拒绝写入不改变缓冲状态 |

### NH09 实际工程集成

- 八个工程分别构建 Debug/Release，检查架构、FPU、优化参数和完整产物。
- 检查无变化增量构建、清理重建、Release 可复现性、默认 ASM 和强制 C 路径。
- 检查 typed、typed float、float fast 和 Skip ASM 的配置传播。
- NH09 会删除并重新生成外部工程常规 `build/` 目录，但不得修改工程源码和构建定义。
- 外部工程可使用本地定制、未提交或未跟踪的配置；工作树是否干净不参与判定。runner 必须确认实际链接的 RTT 库为候选提交，并记录架构、工具链、构建类型和影响本次结论的 RTT 配置。
- NH09 PASS 只声明候选 RTT 库已在所记录的八个真实工程组合中完成编译、链接、配置传播和产物检查；不声明外部工程已发布，不要求用户采用相同配置，也不替代板级运行测试。

### NH10 资源和可复现构建

- 每个配置执行三次干净构建并比较 ELF 哈希和 section 尺寸。
- 最小镜像：Flash 不超过 16384 B，静态 RAM 不超过 2048 B，最大固定栈帧不超过 256 B。
- 当前默认配置：Flash 不超过同环境重建 release 加 1024 B，静态 RAM 不超过 release。
- legacy float fast：Flash 不超过 fast-off 加 256 B，固定栈不超过 fast-off 加 64 B，静态 RAM 不增加。
- Skip ASM 与 Skip C 的 Flash 绝对差不超过 256 B，静态 RAM 不增加；M0 保持 C 路径。
- typed-only、typed-float-only 不保留 formatter；M0 默认和 typed 路径不引入整数除法辅助符号。
- NH10 在临时目录构建实际工程，并在 metadata 中记录测试脚本、工具链、构建环境和证据哈希。
- 外部工程的本地配置和 dirty 状态不参与资源预算或 PASS/FAIL 判定。资源数据只适用于报告中记录的工具链、架构、构建模式和关键 RTT 配置，用作已验证参考值，不构成用户工程的固定资源保证。
- 资源门禁全部归 NH10：formatter、typed integer、legacy float fast-off/on、Skip C/ASM
  分别由 `default/print_string`、`typed`、`legacy_fast_off/on`、`skip_c/asm` 配置测量。
  这些项目不需要开发板，不得再作为 HW08 build-only 组合重复执行。

### NH11-NH12 typed 与回归语料

| 用例 | 验证重点 | 验收标准 |
|---|---|---|
| NH11 | typed 整数、指针、标签边界、单次写入和非零通道 | 数值和位宽正确；标签最多 46 B；短写返回错误 |
| NH12 | 全部 API 写入失败、路径等价和 100000 个 binary32 随机模式 | 失败不重试、不继承状态；三组路径在规范内等价 |

NH01-NH12 必须全部退出 0，且统一 `SUMMARY.md` 中全部为 PASS。

## 6. HW01-HW08 执行矩阵

正式发布必须从主仓库根目录使用版本化矩阵 `tests/HW/release_matrix.json` 和
`--release-suite` 入口执行。runner 按目标组织 178 个唯一组合、182 次实际运行；MCU 的
先后顺序属于现场调度，可用 `--mcu-order` 调整，不参与测试判定。固定一块板后完成该板
全部 Make/CMake 组合，再提示更换开发板；任一组合失败立即停止并保留已有证据。
单独指定 case/profile 的命令只用于诊断和预回归，不构成完整发布矩阵。

### 6.1 构建类型和参数

| 用例 | Debug | Release | 参数矩阵 |
|---|---:|---:|---|
| HW01 | 全部 8 目标 | 全部 8 目标 | profile 0 |
| HW02 | 不要求 | 全部 8 目标 | profile 0 |
| HW03 | 不要求 | 全部 8 目标 | `--profile 0..6` |
| HW04 | 不要求 | 全部 8 目标 | profile 0；F103/F411/H7 另跑 profile 1 |
| HW05 | 不要求 | 全部 8 目标 | `--profile 0..3` |
| HW06 | 不要求 | 全部 8 目标 | `--up-size 128`、`--up-size 256` |
| HW07 | 不要求 | 全部 8 目标 | profile 0 |
| HW08 marker | 不要求 | 除 `f042_make` 外 7 目标 | `f042_cmake`：`--float-fast 0/1`、固定 `--skip-asm 0`；其余 6 目标：`--float-fast 0/1` × `--skip-asm 0/1` |
| HW08 gated | 不适用 | `f042_make` | 默认协议，`--repeat 3` |
| HW08 qualification | 不适用 | `f411_make` | `--throughput-qualification --repeat 3` |

Debug 用于确认两类工程的构建、烧录和 RTT 建链；完整功能、边界、吞吐与性能资格线冻结在 Release。F042 Make HW08 协议不接受 marker 参数。

### 6.2 通用命令形式

正式发布命令为：

```sh
python3 tests/HW/run_hw.py --release-suite \
  --mcu-order STM32H7B0VB --mcu-order STM32F042G6 \
  --evidence-dir TEST_EVIDENCE/HW_RELEASE_<candidate>
```

`--release-suite` 要求主仓库为已提交且干净的候选，拒绝 case/profile/build-only/repeat 等
手工矩阵覆盖。`--mcu-order` 按参数出现顺序设置优先级，未列出的 MCU 自动追加；它只改变
运行顺序，不改变矩阵或证据集合。全部运行结束后，入口会自动执行精确证据验收；也可
独立复核已有证据：

```sh
python3 tests/HW/run_hw.py \
  --verify-evidence TEST_EVIDENCE/HW_RELEASE_<candidate>
```

验收器依据同一版本化矩阵检查 182 个精确路径、每项 `metadata.json` 与 `result.txt`、
全部参数、每项均为硬件 `PASS`、候选 SHA，以及主仓库和工程内实际库均为干净的
同一候选提交；任何缺失项或多余项均失败。验证历史候选证据时追加
`--candidate-sha <完整 SHA>`。

以下单组合形式仅用于失败定位和开发阶段预回归：

```sh
python3 tests/HW/run_hw.py \
  --target <target> --case <HWxx> --build-type <debug|release> \
  --evidence-dir TEST_EVIDENCE/HW_RELEASE_<candidate>
```

profile 和缓冲参数按 6.1 表追加。F042 Make HW08 的定向诊断命令为：

```sh
python3 tests/HW/run_hw.py \
  --target f042_make --case HW08 --build-type release --repeat 3 \
  --evidence-dir TEST_EVIDENCE/HW_RELEASE_<candidate>
```

F411 正式吞吐资格组合已包含在 `--release-suite` 中；其定向诊断命令为：

```sh
python3 tests/HW/run_hw.py \
  --target f411_make --case HW08 --build-type release \
  --throughput-qualification --repeat 3 \
  --evidence-dir TEST_EVIDENCE/HW_RELEASE_<candidate>
```

### 6.3 用例验收标准

| 用例 | 验证内容 | 验收标准 |
|---|---|---|
| HW01 | 构建、烧录、RTT 初次连接和无复位重连 | 两次会话均含候选 SHA；重连后心跳严格递增 |
| HW02 | 13 个公开 API 端到端输出 | 数量、顺序、内容、通道和换行正确，无 Fault/FAIL |
| HW03 | default、no-color、Lite、typed-only、typed-float-only、typed-combo、channel 1 | profile 与输出匹配，非零通道可采集，禁用功能不输出 |
| HW04 | Skip C/ASM 边界状态 | 10/10 用例 PASS；C/ASM 返回值、字节和偏移一致 |
| HW05 | bit/modff、compat/fast、随机语料和 46/47 B 标签 | corpus、4 个边界和 40000 次循环全部 PASS，无栈破坏 |
| HW06 | Up Buffer 临界、突发、无读取窗口和重连 | 11/11 用例 PASS；接受/拒绝闭环；重连后序号推进 |
| HW07 | 主循环与 TIM1 ISR 六阶段并发 | 6/6 phase PASS；零丢帧；控制块、哨兵和栈有效 |
| HW08 | 周期、吞吐和运行时栈 | marker 路径 6/6、样本完整；栈保护通过；资格吞吐闭环 |

### 6.4 HW07/HW08 特殊协议

- H7 HW07 在正式负载前等待 `HW-07|READY|WAIT_HOST`，runner 确认新的 RTT 控制块后通过 Down channel 发送 gate。
- marker HW08 每轮要求 7168 条周期样本和完整 PASS 结尾。
- H7 HW08 额外通过 ELF 符号内存 gate 启动 16384 个 64 B 帧；目标速率为 0.36 MiB/s，允许误差 1%，目标和主机帧数、序号及 CRC 必须闭环。
- F042 Make HW08 使用随机非零 `run_id` 和结果结构，发送 8192 个 64 B 帧；目标速率为 0.02 MiB/s，允许误差 1%，三次独立运行均须 PASS。
- F411 marker HW08 的 2048 帧突发写入只作 `CHARACTERIZATION`，不形成吞吐保证结论。正式资格模式使用随机非零 `run_id`、内存 gate 和结果结构，以 0.10 MiB/s 发送 16384 个带序号和 CRC 的 64 B 帧；目标零拒绝、主机完整捕获且三次独立运行均须 PASS，由此回归冻结的 0.08 MiB/s 保证线。
- `--build-only` 只能形成构建结论，不能标记为硬件 PASS；资源结论由 NH10 形成。

## 7. 最终发布关闭条件

1. 被测主仓库是一个已记录且工作树干净的候选提交；NH09、NH10 和 HW 实际使用的 RTT 库 SHA 均与该候选提交一致。八个外部工程不要求干净、固定提交或发布。
2. NH01-NH12 在候选提交上完整运行并全部 PASS。
3. 四块板、八个工程按第 6 节完成 HW01-HW08，所有必需组合均 PASS。
4. runner metadata、目标 marker 或 gated 证据均能关联候选 SHA，不存在旧固定 SHA。
5. 原始证据目录只读保留，最终报告列出环境、命令、矩阵、工具链、关键有效配置、失败尝试、有效重跑及证据索引，并明确外部工程组合属于已验证参考环境而非用户配置限制。
6. 最终测试后不得修改主仓库产品代码、runner、fixture、配置或本计划；如有修改，必须创建新候选提交并完整重跑 NH01-NH12 和第 6 节规定的 HW01-HW08 矩阵，不得把上一候选的 PASS 拼接到新候选报告中。
7. HW09 未执行必须在发布说明中列为已知验证边界，不得宣称已完成长期稳定性测试。
