# 统一硬件测试 Runner

`run_hw.py` 是 HW01～HW08、四款 MCU、Make/CMake 工程的统一主机侧入口。
`targets.json` 只保存可移植的工程、构建、烧录和探针参数；用例选择、RTT 采集和结果判定
由 runner 维护。目标侧源码集中在 [`fixtures/`](fixtures/README.md)。

CMake 工程使用 STM32 VS Code 插件提供的 `cube-cmake`，不是系统 `cmake`。
runner 会依次从 `CUBE_CMAKE`、`PATH` 和已安装的 STM32 VS Code 插件目录定位
`cube-cmake`，不会回退到系统 `cmake`。

runner 会读取当前 ARM_SEGGER_RTT Git 提交，并通过 overlay 将其作为
`HW_TEST_LIBRARY_SHA` 注入目标固件和 metadata。工作树不干净时标识追加 `-dirty`；
这种结果适合预回归，但不能作为最终发布证据。
构建前，runner 还会校验工程内实际参与编译的 `ARM_SEGGER_RTT` Git 提交与上述提交
完全一致。独立的嵌套库必须是干净工作树；仅当工程直接引用 runner 所在的同一工作树时，
才允许带 `-dirty` 标识执行开发阶段的预回归。

## 运行前提

- 目标工程已按正常移植方式引入 ARM_SEGGER_RTT。Make 工程使用
  `ARM_SEGGER_RTT/segger_rtt.mk`；CMake 工程提供并链接 `arm_segger_rtt` 目标。
- Make/CMake 工程都不需要在 `main.c`、中断文件、linker script 或工程 `rtt_cfg.h`
  中接入测试代码；测试入口、fixture、配置和链接参数全部由 runner overlay 注入。
- 硬件运行需要 J-Link、目标板供电以及可执行的 `JLinkExe`、`JLinkRTTClient`。
- CMake 目标需要安装 STM32 VS Code 插件，或通过 `CUBE_CMAKE` 显式指定插件提供的可执行文件。

## 本机配置

版本化的 `targets.json` 使用 PATH 中的 `arm-none-eabi-*` 命令，不保存某台机器的
`/opt/...` 工具链路径。需要指定本机工具链或工程目录时，以
[`config.example.json`](config.example.json) 为模板创建被 Git 忽略的
`tests/HW/config.local.json`；runner 会自动加载它。也可以用 `--local-config PATH`
指定其他覆盖文件。本地文件的 `defaults` 只允许覆盖 `toolchain_bin` 和 `nm`；`targets`
只允许按目标覆盖 `project_dir`、`toolchain_bin` 和 `nm`。MCU、协议、构建命令和验收阈值
不能由本地文件改变，未列出的目标保持 `targets.json` 配置。

工具链还可以通过环境变量或命令行统一覆盖全部目标：

```sh
HW_TOOLCHAIN_BIN=/path/to/arm-gnu-toolchain/bin \
HW_NM=arm-none-eabi-nm \
python3 tests/HW/run_hw.py --target h7b0_make --case HW01

python3 tests/HW/run_hw.py --target h7b0_make --case HW01 \
  --toolchain-bin /path/to/arm-gnu-toolchain/bin \
  --nm /path/to/arm-none-eabi-nm
```

优先级为命令行、`HW_TOOLCHAIN_BIN`/`HW_NM`、本地配置、`targets.json`。正式
`--release-suite` 仍固定使用版本化的目标和矩阵定义，但允许以上机器相关覆盖；实际值
会写入每项 evidence metadata。

本地配置结构如下：

| 位置 | 可配置字段 | 作用范围 |
|---|---|---|
| `defaults` | `toolchain_bin`、`nm` | 所有目标 |
| `targets.<name>` | `project_dir`、`toolchain_bin`、`nm` | 单个目标 |

`toolchain_bin` 留空时直接使用当前 PATH；非空时必须是 Arm GNU Toolchain 的 `bin`
目录，runner 会把它放到构建进程 PATH 最前面。`nm` 可以是 PATH 中的命令名，也可以是
绝对路径。`project_dir` 可以是绝对路径；相对路径以 ARM_SEGGER_RTT 主仓库为基准。
例如同时覆盖八个工程共用的工具链和一个工程的位置：

```json
{
  "schema_version": 1,
  "defaults": {
    "toolchain_bin": "/opt/arm-gnu-toolchain/bin",
    "nm": "arm-none-eabi-nm"
  },
  "targets": {
    "h7b0_make": {
      "project_dir": "/work/stm32h7b0vbmake"
    }
  }
}
```

`--config PATH` 的含义不同：它替换版本化的完整目标定义，只用于开发诊断，正式
`--release-suite` 会拒绝它。`--local-config PATH` 只覆盖上述机器路径，可以用于正式
矩阵。`cube-cmake` 继续通过 `CUBE_CMAKE` 或 PATH 指定；`JLinkExe` 和
`JLinkRTTClient` 通过 PATH 查找。探针序列号属于本次连接状态，不写入本地配置，使用
`--probe-serial SERIAL` 指定。

测试必须通过 `run_hw.py` 启动。Make 工程的测试宏、栈参数和链接 wrapper 已从工程
Makefile 迁到主库 overlay，直接执行 `make HW_TEST_CASE=...` 不再代表完整 HW 测试构建。

## 单工程步骤

以 H7 Make 工程为例，先进入工程根目录：

```sh
cd /path/to/stm32h7b0vbmake
```

先用 dry-run 检查目标、用例和实际构建命令；该操作不构建、不烧录、不访问硬件：

```sh
python3 ARM_SEGGER_RTT/tests/HW/run_hw.py \
  --local --target h7b0_make --case HW01 --dry-run
```

连接 H7 与 J-Link 后运行 HW01：

```sh
python3 ARM_SEGGER_RTT/tests/HW/run_hw.py \
  --local --target h7b0_make --case HW01 --build-type release
```

单工程命令用于开发诊断和预回归。可以把 `--case` 改为 `HW02` 到 `HW08` 定位问题；
遇到失败时保留证据并停止。它不代替主仓库中的完整 `--release-suite` 发布入口。

`--local` 使用当前目录作为唯一目标工程，但仍从 `targets.json` 读取 MCU、构建、
烧录和 RTT 参数。从其他目录启动时可改用：

```sh
python3 /path/to/ARM_SEGGER_RTT/tests/HW/run_hw.py \
  --project-dir /path/to/stm32h7b0vbmake \
  --target h7b0_make --case HW01
```

`--local` 和 `--project-dir` 都要求且只允许一个 `--target`。

## Make 测试 Overlay

对 Make 目标，runner 自动把原工程 Makefile 与
[support/hw_test.mk](support/hw_test.mk) 按顺序加载，等价于：

```sh
make \
  -f /path/to/project/Makefile \
  -f /path/to/ARM_SEGGER_RTT/tests/HW/support/hw_test.mk \
  -j8 DEBUG=0 OPT=-Os
```

overlay 统一提供：

- 中央 `support/hw_test_entry.c` 测试入口和当前用例 fixture，并把入口对象加入 ELF；
- 当前 runner 所属主库的 fixture/config include 路径；
- 根据工程的 `DEBUG` 值注入 `HW_TEST_BUILD_DEBUG` 或 `HW_TEST_BUILD_RELEASE`；
- MCU family、工程 IRQ handler/编译单元和 `--undefined=HW_TestEntry`；
- `HW_TEST_CASE`、profile、Up Buffer 和 HW08 gated/throughput 编译宏；
- `-fstack-protector-all` 与 `-fstack-usage`；
- 通过 `support/rtt_sections.ld` 把测试专用 `.rtt_cb`、`.rtt_buffer` 放入 `RAM`；
- HW fixture 使用的 `--wrap=SEGGER_RTT_Write`；
- 仅 HW05 使用的 `--wrap=SEGGER_RTT_printf`。
- HW07 只对配置的 IRQ 编译单元重命名工程 handler，由测试入口提供代理 handler，先执行
  fixture hook，再调用原工程 handler。

overlay 只参与本次 make 进程，不写入、不替换原工程 Makefile 或 linker script，异常退出也不需要恢复。
它也不要求工程的 `main.c`、`*_it.c` 或 `rtt_cfg.h` 保留测试钩子。普通工程 Makefile 只保留
RTT 库的正常源码和头文件集成。构建日志会记录两个 `-f`
参数；HW05 metadata 还会记录 `wrap_printf: true`，runner 会严格判定 legacy 与 typed
实际调用路径。

## CMake 测试 Overlay

对 CMake 目标，runner 直接调用 STM32 插件的 `cube-cmake`，并通过
[`support/hw_test_overlay.cmake`](support/hw_test_overlay.cmake) 注入测试配置。配置命令的
核心形式为：

```sh
cube-cmake --fresh \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PROJECT_INCLUDE=/path/to/ARM_SEGGER_RTT/tests/HW/support/hw_test_overlay.cmake \
  -DHW_TEST_REPO_ROOT=/path/to/ARM_SEGGER_RTT \
  -DHW_TEST_CASE=5 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
  -S . -B build/Release -G Ninja
```

overlay 在工程完成目标定义后执行，并仅修改当前 CMake configure/build 的目标属性：

- 向应用目标加入 `support/hw_test_entry.c` 和中央 fixture；
- 让应用目标与 `arm_segger_rtt` 同时优先使用测试专用 `rtt_cfg.h`；
- 注入用例、profile、MCU、构建类型、缓冲区和 HW08 参数；
- 注入栈保护、stack-usage、`--wrap=SEGGER_RTT_Write`，HW05 额外注入
  `--wrap=SEGGER_RTT_printf`；
- H7 通过 `support/rtt_sections.ld` 增加 `.rtt_cb` 和 `.rtt_buffer` 的 `NOLOAD` 布局；
- HW07 在单个编译单元内重命名工程 TIM1 handler，由测试入口提供代理 handler，先执行
  fixture hook，再调用原工程 handler。

Make/CMake 烧录后都先让原工程 `main()` 完成时钟、HAL 和外设初始化；runner 再从 ELF
定位 `HW_TestEntry`，通过 J-Link 设置 PC 进入 fixture。RTT client 在测试入口启动后才
连接，避免把 `NOLOAD` RAM 中上一用例的 RTT 控制块误认为本轮数据。

整个过程不会写入或临时替换工程的 `CMakeLists.txt`、`main.c`、`*_it.c`、linker script、
包装 Makefile 或 `rtt_cfg.h`。会变化的只有工程现有的 `build/<Config>` 生成目录和
`TEST_EVIDENCE` 证据目录；`--fresh` 会重新配置该 build 目录。

## 主仓库模式

在 ARM_SEGGER_RTT 主仓库中可以列出或调度配置的工程：

```sh
# 列出 8 个目标工程
python3 tests/HW/run_hw.py --list

# 验证目标配置和 overlay 命令
python3 tests/HW/run_hw.py --target f103_make --case HW02 --dry-run

# 只构建 F103 Make/CMake 的 HW07
python3 tests/HW/run_hw.py --mcu STM32F103C8 --case HW07 --build-only

# 运行一个指定目标和 profile
python3 tests/HW/run_hw.py --target f103_make --case HW04 --profile 1
```

`--all` 会遍历全部目标并在 MCU 改变时等待换板确认。`--yes` 只应用于已经连接正确
板卡或具备自动换板机制的场景。多个 J-Link 同时连接时可用 `--probe-serial SERIAL`
同时约束烧录、RTT 采集和结果读取使用的探针。指定序列号后，runner 绕过工程自身
无法选择探针的烧录目标，直接将本轮构建并归档的 ELF 下载到指定探针连接的目标板。

### 正式发布矩阵

正式发布不使用 `--all` 或人工循环。主仓库中的
[`release_matrix.json`](release_matrix.json) 固化测试计划要求的 178 个唯一组合；其中
F042 和 F411 的资格测试各含三次重复，因此总计执行 182 次：

```sh
python3 tests/HW/run_hw.py --release-suite \
  --mcu-order STM32H7B0VB --mcu-order STM32F042G6 \
  --evidence-dir TEST_EVIDENCE/HW_RELEASE_<candidate>
```

该入口要求主仓库处于干净的已提交候选，固定使用仓库内矩阵，并拒绝手工传入其他
矩阵/目标配置文件、case、profile、构建类型、build-only、repeat 或资格模式。runner 按 MCU/目标
排序以减少换板；任一项失败即退出，已经生成的失败批次证据应保留。

`--mcu-order` 可重复指定希望优先运行的 MCU，按参数出现顺序调度；未列出的 MCU 会按
目标配置中的顺序自动追加。例如上例先运行 H7，再运行 F0，随后完成 F1 和 F4。
该参数只调整执行顺序，不增删组合，也不影响最终证据验收。

全套运行成功后入口会自动做精确验收；也可以用同一矩阵独立复核已有证据：

```sh
python3 tests/HW/run_hw.py \
  --verify-evidence TEST_EVIDENCE/HW_RELEASE_<candidate>
```

验证器要求 182 个证据路径与矩阵完全相等，并检查每项 `metadata.json`、`result.txt`、
矩阵参数、每项均为硬件 `PASS`、候选 SHA，以及实际参与构建的库提交和 clean 状态。
缺失或多余的 metadata/result 都会失败。当前工作树不是该历史候选时，使用
`--candidate-sha <完整 SHA>` 指定证据对应的提交。

## 证据

未指定 `--evidence-dir` 时，证据写入
`TEST_EVIDENCE/HW_RUN_<timestamp>/<case>/<target>/...`。指定共同目录可把同一批次
HW01～HW08 集中保存，但 runner 拒绝覆盖已存在的用例/profile 目录；重跑前应移动旧
证据到批次内的 `ATTEMPTS/`，或者使用新的证据目录。

带独立矩阵参数的用例会在 `profile-<n>` 前增加参数目录，避免同一批次内的有效组合
相互覆盖：

```text
HW06/<target>/<build>/up-size-128/profile-0/
HW08/<target>/<build>/float-fast-1_skip-asm-0/profile-0/
```

`--build-only` 不要求连接开发板，只证明编译和链接成功，不等价于硬件 PASS。

## 用例参数

| 用例 | 参数 | 含义 |
|---|---|---|
| HW01 | 无 | 首次 RTT 采集、断开重连、心跳递增 |
| HW02 | 无 | 13 个公开 API 端到端输出 |
| HW03 | `--profile 0..6` | default、no-color、Lite、typed-only、typed-float-only、typed-combo、channel 1 |
| HW04 | `--profile 0..1` | Skip C、Skip ASM；F042 只允许 0 |
| HW05 | `--profile 0..3` | bit/compat、bit/fast、modff/compat、modff/fast |
| HW06 | `--up-size N` | Up Buffer 临界、突发、无读取窗口和重连恢复 |
| HW07 | 无 | 主循环与 TIM1 ISR 并发 |
| HW08 | `--float-fast 0/1`、`--skip-asm 0/1` | 板上 marker 性能、表征吞吐和运行时栈检查；F042 只允许 Skip C |
| HW08 | `--throughput-qualification --repeat 3` | 目标配置的正式门控吞吐资格测试 |

未定义 profile 的用例必须保持 `--profile 0`。每轮都会干净重建；runner 通过环境变量
和 overlay 向 Make/CMake 传递 `HW_TEST_CASE`、profile 和缓冲参数，不修改工程源码。

## 判定协议

HW02～HW08 的 marker 协议检查目标 MCU、构建类型、BEGIN/END、用例数量、PASS 结尾及
Fault/FAIL 禁止项。HW06 分两次建立 RTT 会话，中间保留无读取窗口，并要求恢复后的
`LIVE` 序号继续推进。HW01 同样不复位地重连，要求心跳严格递增。

H7 的 HW07 在六个并发 phase 前使用 RTT Down channel 单字节握手。目标输出
`HW-07|READY|WAIT_HOST` 后，runner 还会确认复位后的新 `RTT CB verified`，再发送配置的
gate；这样正式负载只会在 J-Link 已重新发现控制块并开始读取后启动，RAM 扫描时间不计入
零丢帧判定。

H7 的 HW08 保留 marker 性能和运行时栈检查，并在吞吐阶段使用 ELF 符号内存 gate。正式
吞吐为 16384 个带序号和 CRC 的 64 B 帧，按 H7B0 已冻结的 0.36 MiB/s 资格线发送；
runner 同时检查目标端全部接受、实际速率误差不超过 1%，以及主机端帧数、序号、CRC、
重复和乱序。内存 gate 避免 HW08 的隐藏计时 Up Buffer 被 J-Link 误识别为 Down Buffer；
该项是资格线回归，不是链路极限表征。

F042 Make 的 HW08 使用 gated-throughput 协议且只允许 Release：runner 生成非零
run ID，从 ELF 定位 `HW08_Result`，写入 gate，采集 8192 个 64 B 帧，回读 64 B
结果结构，并检查目标计数、速率、RTT 偏移、run ID、序号和 CRC。定向诊断命令为：

```sh
python3 tests/HW/run_hw.py --target f042_make --case HW08 \
  --build-type release --repeat 3
```

F042 CMake 和 F1/F4/H7 使用 HW08 marker 协议，要求 7168 条样本和完整 PASS 结尾；
其中 H7 还必须通过上述门控吞吐闭环。
F042 Make gated-throughput 不接受上述 marker 参数。F042/Cortex-M0 的 marker 组合固定使用
`--skip-asm 0`，runner 会拒绝 F042 的 `--skip-asm 1`。formatter、typed、legacy float、
Skip C/ASM 的 Flash/RAM/stack 和符号门禁全部由 NH10 执行；这些项目不访问硬件，已从
HW08 发布矩阵和 fixture 中移除。

F411 的普通 marker HW08 中，2048 帧突发写入仅为 `CHARACTERIZATION`，允许
`NO_BLOCK_SKIP` 按契约拒绝整帧，不能代替冻结保证线。正式资格测试独立使用历史 v2
协议：Make Release、0.10 MiB/s、16384 个 64 B 帧、随机非零 run ID、内存 gate、序号
和 CRC，并回读 64 B 目标结果结构。目标必须零拒绝，主机必须完整捕获，实际速率误差
不超过 1%，连续三次都须 PASS。以下命令只用于定向诊断；发布证据必须由包含该组合的
`--release-suite` 形成：

```sh
python3 tests/HW/run_hw.py --target f411_make --case HW08 \
  --throughput-qualification --build-type release --repeat 3
```

资格证据写入 `HW08/f411_make/release/qualification/run-1..3`，不会覆盖同批次的
`profile-0` marker 证据。该模式通过 Make overlay 引入专用 fixture，不修改 F411 工程。

## 历史 Overlay 验证记录

以下内容是 2026-08-31 的开发阶段验证记录，不是当前候选的正式发布证据，也不替代
`--release-suite`。当时使用 STM32Cube CMake build 插件 1.46.0 提供的 `cube-cmake` 和 GNU Tools
for STM32 14.3.1 验证了无需修改工程源码的 overlay：

- F042、F103、F411、H7B0 的 HW01 Release 均完成配置、编译和链接；
- 四个 CMake 目标的 HW05、HW07、HW08 Release 均完成 build-only；
- H7B0 的 HW01～HW08 Release 全部完成烧录、RTT 采集和硬件 PASS；
- H7B0 的 HW01 Debug 完成 build-only。

build-only 只证明配置、编译和链接成功；未列为硬件 PASS 的 MCU/用例仍需在对应开发板上
完成烧录和协议验收。

本轮详细证据索引及修复记录见
[`docs/TEST_RESULTS_CMAKE_OVERLAY_20260831.md`](docs/TEST_RESULTS_CMAKE_OVERLAY_20260831.md)。

### Make Overlay

同日使用 GNU Tools for STM32 14.3.1 验证了 Make 纯 overlay：

- F042、F103、F411、H7B0 的 HW01 Release 均完成编译和链接；
- H7B0 的 HW05、HW07、HW08 Release 均完成 build-only；
- H7B0 的 HW01、HW07 Release 完成烧录、RTT 采集和硬件 PASS，分别验证中央入口跳转、
  RTT 重连以及纯 overlay 的 TIM1 中断代理链；
- HW07 ELF 同时包含测试 ISR 入口、代理 handler、原工程重命名 handler 和 RTT wrapper，
  证明中断代理不依赖修改工程 `*_it.c`。

以上 build-only 只证明 overlay 的编译、链接和符号布局正确；真机协议结果以对应证据目录
中的 `result.txt` 为准。

## 发布约束

八个工程通过各自的 `ARM_SEGGER_RTT` 库工作树读取中央 fixture。必须先提交并发布本仓库，
再更新八个工程的库指针。Make/CMake 工程都不再提交测试专用的 `main.c`、中断钩子、
链接脚本或配置文件改动；中间状态不应发布。
