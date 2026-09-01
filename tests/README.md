# 测试指南

本目录包含 ARM_SEGGER_RTT 的现行自动化测试、目标侧硬件测试 fixture，以及仅供追溯的历史测试资料。现行测试统一通过 Python runner 启动，测试实现、配置和共享辅助文件均保存在对应测试目录内。

[`docs/quality/test_plan.md`](../docs/quality/test_plan.md) 是现行发布测试定义，规定发布阻塞范围、固定执行矩阵和验收标准；本文件及 NH/HW README 负责说明具体配置和运行方式。

## 目录结构

```text
tests/
  README.md              测试总览和入口
  NH/                    NH01-NH12 非硬件测试
    run_nh.py            NH 统一运行器
    config.example.json  跨平台配置模板
    cases/               各 NH 用例的脚本、fixture 和资源
    support/             NH 共享辅助文件
  HW/                    HW01-HW08 硬件测试
    run_hw.py            HW 统一运行器
    targets.json         MCU、工程、构建和探针配置
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
| HW01-HW08 | MCU 端到端输出、重连、并发、吞吐、资源和栈测试 | 是，完整测试还需要 J-Link | `tests/HW/run_hw.py` | [HW 测试说明](HW/README.md) |

NH09 和 NH10 虽然不连接开发板，但会使用八个外部 STM32 工程进行交叉构建，因此比普通主机测试需要更多工具和工程配置。HW 的 `--build-only` 不需要连接开发板，但只证明编译和链接成功，不能作为硬件 PASS。

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

1. 根据 [NH 配置说明](NH/README.md#配置) 创建本机的 `tests/NH/config.local.json`。
2. 先运行 NH01-NH08、NH11 和 NH12，验证主机行为、API 和 Arm 构建结果。
3. 配置八个外部工程后运行 NH09 和 NH10，验证工程集成和资源预算。
4. 对目标 MCU 的 Make/CMake 工程先执行 HW `--dry-run`，再按需执行 `--build-only`。
5. 连接正确的开发板和 J-Link，按照 HW01 到 HW08 的顺序逐个运行；每个用例 PASS 后再运行下一个。

运行完整 NH 测试集：

```sh
python3 tests/NH/run_nh.py --all
```

运行单个 HW 用例：

```sh
python3 tests/HW/run_hw.py \
  --target h7b0_make --case HW01 --build-type release
```

不要直接执行带测试宏的工程 Makefile 来代替 HW runner。HW 所需 fixture、链接参数和 RTT 判定协议由 Make/CMake overlay 与 runner 共同提供。

## 测试证据

两个 runner 默认将证据写入仓库根目录的 `TEST_EVIDENCE/`：

```text
TEST_EVIDENCE/
  NH_RUN_<timestamp>/...
  HW_RUN_<timestamp>/...
```

可以使用 `--evidence-dir PATH` 为一次验收指定独立目录。runner 不会静默覆盖已有的证据目录；失败重跑应保留旧证据并使用新目录，或按 HW 文档约定移入批次的 `ATTEMPTS/`。

`TEST_EVIDENCE/` 默认被 Git 忽略。需要随版本发布的结论应整理成稳定的 Markdown 报告，记录环境、命令、结果和对应证据索引，而不是直接提交全部原始构建产物。

## 对工程的影响

- NH01-NH08、NH11 和 NH12 不修改外部工程。
- NH10 在临时目录构建外部工程，不修改其源码树。
- NH09 会删除并重新生成八个外部工程常规的 `build/` 目录，运行前应保留仍需使用的构建产物。
- HW 使用纯 Make/CMake overlay，不修改工程的 `main.c`、中断源码、Makefile、`CMakeLists.txt`、linker script 或 `rtt_cfg.h`。
- HW 构建会更新目标工程现有的构建目录；完整运行还会烧录目标并通过 RTT 采集结果。

更精确的依赖、参数、用例协议和平台配置以 [NH README](NH/README.md) 与 [HW README](HW/README.md) 为准。

## legacy 目录

`tests/legacy/` 保存历史测试计划、历史报告和已经退出现行流程的工具。它用于追溯过去的验证方法与结论，不是当前测试入口，也不应被 NH/HW runner 或活动测试源码依赖。

新增或修正测试时，应将 NH 内容放入 `tests/NH/`，将 HW 内容放入 `tests/HW/`；只有需要保留但不再维护的历史材料才放入 `tests/legacy/`。
