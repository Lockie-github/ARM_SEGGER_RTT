# 测试指南

本目录包含 ARM_SEGGER_RTT 的现行自动化测试、目标侧硬件测试 fixture，以及仅供追溯的历史测试资料。现行测试统一通过 Python runner 启动，测试实现、配置和共享辅助文件均保存在对应测试目录内。

[`docs/quality/test_plan.md`](../docs/quality/test_plan.md) 是现行发布测试定义，规定发布阻塞范围、固定执行矩阵和验收标准；本文件及 NH/HW README 负责说明具体配置和运行方式。

## 目录结构

```text
tests/
  README.md              测试总览和入口
  NH/                    NH01-NH12 必需测试、NHO 可选测试及 NHT 临时测试
    run_nh.py            NH 统一运行器
    config.example.json  跨平台配置模板
    cases/               各 NH 用例的脚本、fixture 和资源
    support/             NH 共享辅助文件
  HW/                    HW01-HW08 硬件测试
    run_hw.py            HW 统一运行器
    release_matrix.json  正式发布组合矩阵
    targets.json         可移植的 MCU、工程、构建和探针配置
    config.example.json  HW 本机路径覆盖模板
    config.local.json    自动加载的本机路径覆盖，不提交
    fixtures/            目标侧测试 fixture
    support/             Make/CMake overlay 和测试配置
    docs/                现行 HW 验证记录
  legacy/                历史计划、报告和旧工具，仅供追溯
```

目录名 `NH` 和 `HW` 区分大小写。脚本、配置和文档中的路径应始终使用 `tests/NH/...` 或 `tests/HW/...`。

## 选择测试类型

| 测试集 | 范围 | 是否需要开发板 | 统一入口 | 详细说明 |
|---|---|---:|---|---|
| NH01-NH12 | 主机行为、格式化、配置开关、代码裁剪、工程集成、资源和回归测试 | 否 | `tests/NH/run_nh.py` | [NH 测试说明](NH/README.md) |
| `NHT_FMT_COMPAT`（临时） | formatter 与兼容旧布局基线的行为确认，不属于发布门禁 | 否 | `tests/NH/run_nh.py --case NHT_FMT_COMPAT` | [NH 测试说明](NH/README.md) |
| `NHO_LEGACY_CONFIG`（可选） | 旧浮点配置迁移诊断，不属于发布门禁 | 否 | `tests/NH/run_nh.py --case NHO_LEGACY_CONFIG` | [NH 测试说明](NH/README.md) |
| `NHT_FLASH_OPT`（临时） | M0 fast-path 及四架构 default/release 资源对比，不属于发布门禁 | 否 | `tests/NH/run_nh.py --case NHT_FLASH_OPT` | [NH 测试说明](NH/README.md) |
| HW01-HW08 | MCU 端到端输出、重连、并发、吞吐、性能和运行时栈测试 | 是，完整测试还需要 J-Link | `tests/HW/run_hw.py` | [HW 测试说明](HW/README.md) |

NH09 和 NH10 虽然不连接开发板，但会使用八个外部 STM32 工程进行交叉构建，因此比普通主机测试需要更多工具和工程配置。HW 的 `--build-only` 不需要连接开发板，但只证明编译和链接成功，不能作为硬件 PASS。

NH 与 HW 的 `config.local.json` 都是机器相关输入并由 Git 忽略。版本化配置保存测试语义，
本地配置只提供工作区、工具链和工程路径；最终报告与 metadata 记录实际使用的环境。

## 快速检查

在仓库根目录列出当前支持的测试：

```sh
python3 tests/NH/run_nh.py --list
python3 tests/HW/run_hw.py --list
```

在不检查工具、不构建、不烧录且不创建证据目录的情况下检查 NH 调度：

```sh
python3 tests/NH/run_nh.py --all --dry-run
```

检查一个 HW 目标的配置和实际命令：

```sh
python3 tests/HW/run_hw.py \
  --target h7b0_make --case HW01 --dry-run
```

## 推荐执行顺序

1. 根据 [NH 配置说明](NH/README.md#配置) 和 [HW 配置说明](HW/README.md#本机配置) 创建本机配置。
2. 使用 `python3 tests/NH/run_nh.py --ci` 运行 NH01-NH08、NH11 和 NH12，
   验证主机行为、API 和 Arm 构建结果。
3. 配置八个外部工程后运行 NH09 和 NH10，验证工程集成和资源预算。
4. 对目标 MCU 的 Make/CMake 工程先执行 HW `--dry-run`；`--build-only` 仅用于定位构建问题。
5. 连接正确的开发板和 J-Link，使用 `--release-suite` 执行版本化 HW 发布矩阵。
6. 验证 NH/HW 证据均属于同一候选提交后，根据证据更新
   `docs/quality/test_report.md`；其中 NH10 的 RTT 库 Flash 数据取自
   `NH10/artifacts/resource_usage/flash_footprint.md`。runner 不会自动修改正式报告。

运行完整 NH 测试集：

```sh
python3 tests/NH/run_nh.py --ci
python3 tests/NH/run_nh.py --all
```

常规 CI 使用 `--ci`，不包含需要八个外部工程的 NH09、NH10，也不包含 `NHO_*`、`NHT_*`。
`--ci` 和正式发布使用的 `--all` 都要求主仓库为干净的已提交候选。统一 summary 和所有
用例 metadata 都会记录完整候选 SHA 与 dirty 状态；单用例 dirty 运行只用于开发阶段预回归。
可选 `NHO_*` 和临时 `NHT_*` 必须显式运行，不会被 `--all` 或常规 CI 选中。

运行单个 HW 用例：

```sh
python3 tests/HW/run_hw.py \
  --target h7b0_make --case HW01 --build-type release
```

单用例命令仅用于诊断和预回归。正式 HW 发布执行及精确证据验收为：

```sh
python3 tests/HW/run_hw.py --release-suite \
  --mcu-order STM32H7B0VB --mcu-order STM32F042G6 \
  --evidence-dir TEST_EVIDENCE/HW_RELEASE_<candidate>
python3 tests/HW/run_hw.py \
  --verify-evidence TEST_EVIDENCE/HW_RELEASE_<candidate>
```

版本化的 `tests/HW/release_matrix.json` 固化 178 个唯一组合、182 次实际运行，全部要求
真机 `PASS`，不含 build-only 资源组合；验证器会拒绝任何缺失/多余组合、缺失/多余结果、
参数或候选 SHA 不一致。无需开发板的长期发布资源、符号和可复现性门禁统一由 NH10 执行；
依赖历史 release 或本次特定优化目标的比较由临时 `NHT_FLASH_OPT` 执行。

不要直接执行带测试宏的工程 Makefile 来代替 HW runner。HW 所需 fixture、链接参数和 RTT 判定协议由 Make/CMake overlay 与 runner 共同提供。

## 测试证据

两个 runner 默认将证据写入仓库根目录的 `TEST_EVIDENCE/`：

```text
TEST_EVIDENCE/
  NH_RUN_<timestamp>/...
  HW_RUN_<timestamp>/...
```

可以使用 `--evidence-dir PATH` 为一次验收指定独立目录。runner 不会静默覆盖已有的证据目录；失败重跑应保留旧证据并使用新目录，或按 HW 文档约定移入批次的 `ATTEMPTS/`。

正式发布证据只对 metadata 和报告记录的完整候选 Git SHA 有效。主仓库产生新的候选
提交后，必须重新执行完整 NH01-NH12 和 HW01-HW08 发布矩阵；旧证据只能保留为原候选
的历史记录，不能通过影响范围分析拼接进新候选的发布结论。定向重跑只用于诊断和预回归。

`TEST_EVIDENCE/` 默认被 Git 忽略。需要随版本发布的结论应在全部 NH/HW 证据验收完成后，
整理到稳定的 `docs/quality/test_report.md` 中，记录环境、命令、结果和对应证据索引，而不是
直接提交全部原始构建产物。NH10 自动生成的 `flash_footprint.md` 是填写正式报告中
`default`、`typed_combo` 两张 RTT 库 Flash 表的数据源，不取代正式报告。

## 对工程的影响

- NH01-NH08、NH11-NH12、`NHO_*` 和 `NHT_*` 不修改外部工程。
- NH10 在临时目录构建外部工程，不修改其源码树。
- NH09 会删除并重新生成八个外部工程常规的 `build/` 目录，运行前应保留仍需使用的构建产物。
- HW 使用纯 Make/CMake overlay，不修改工程的 `main.c`、中断源码、Makefile、`CMakeLists.txt`、linker script 或 `rtt_cfg.h`。
- HW 构建会更新目标工程现有的构建目录；完整运行还会烧录目标并通过 RTT 采集结果。

更精确的依赖、参数、用例协议和平台配置以 [NH README](NH/README.md) 与 [HW README](HW/README.md) 为准。

## legacy 目录

`tests/legacy/` 保存历史测试计划、历史报告和已经退出现行流程的工具。它用于追溯过去的验证方法与结论，不是当前测试入口，也不应被 NH/HW runner 或活动测试源码依赖。

新增或修正测试时，应将 NH 内容放入 `tests/NH/`，将 HW 内容放入 `tests/HW/`；只有需要保留但不再维护的历史材料才放入 `tests/legacy/`。
