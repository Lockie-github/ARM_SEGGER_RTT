# ARM_SEGGER_RTT NH 全量及 HW 预发布测试报告

> 报告日期：2026-09-04
> 被测分支：`perf_flash`
> 被测提交：`29d61fc8d972b1436a13da993ef6f71f903dc86c`
> 测试方案：`docs/quality/test_plan.md`（候选提交 `29d61fc` 内现行版）
> 执行范围：NH01-NH12；临时/可选 NH 3 项；H7B0、F042、F103、F411 的 HW01-HW08
> 原始证据：`TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/` 及第 7 节板级证据

## 1. 最终结论

**预发布判定：NH 12/12 通过；H7B0、F042、F103、F411 板级发布矩阵 182/182 通过。**

本轮统一 runner 退出码为 0。主机质量门、API 与配置矩阵、RTT 写入路径、八个实际工程集成、资源预算、三次可复现构建以及 100000 个 binary32 随机模式全部满足测试方案的验收标准。未发现本轮 NH 范围内的未关闭产品缺陷。

本轮同时完成 H7B0、F042、F103 和 F411 的 HW01-HW08 板级子矩阵，覆盖 Make/CMake、
Debug/Release、全部 profile、Up Buffer 组合、C/ASM Skip 路径以及适用的吞吐协议。
四种 MCU 的完整 HW01-HW08 发布矩阵已经关闭；HW09 仍为非阻塞扩展项。

## 2. 测试对象与环境

| 项目 | 内容 |
|---|---|
| 日志库 | ARM_SEGGER_RTT |
| 候选提交 | `29d61fc8d972b1436a13da993ef6f71f903dc86c` |
| 主仓库状态 | 测试启动和 runner 完成时均为干净工作树 |
| 主机 | macOS 12.2.1（Darwin 21.3.0） |
| Python | 3.7.7rc1 |
| 主机编译器 | Apple Clang 13.1.6 |
| 静态分析 | Cppcheck 2.18.0 |
| Arm 工具链 | Arm GNU Toolchain 12.2.1（Build arm-12.24） |
| Cube CMake | STM32 扩展内置 `cube-cmake`，CMake 4.3.1 |
| 构建工具 | GNU Make 3.81、Ninja 1.13.1 |
| 并行度 | 8 |
| MCU/架构 | STM32F042G6/M0、STM32F103C8/M3、STM32F411CE/M4F、STM32H7B0VB/M7 |
| 工程矩阵 | 4 MCU x Make/CMake x Debug/Release |

执行命令：

```sh
python3 tests/NH/run_nh.py --all --keep-going \
  --evidence-dir TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904
```

NH09 完整执行了 M0、M3、M4F、M7 的 8 个 Make/CMake 工程，未缩减测试矩阵。

## 3. 结果总览

| 编号 | 测试项目 | 关键结果 | 状态 |
|---|---|---|---|
| NH01 | 核心质量门 | ASan、UBSan、Cppcheck、覆盖率、8 个 Arm ELF | PASS |
| NH02 | 四级日志 | 12 个等级用例、连续序列及返回路径 | PASS |
| NH03 | 通用输出 | print/string、关闭行为及副作用 | PASS |
| NH04 | formatter | 37 个当前用例及 8 B 分段缓冲 | PASS |
| NH05 | 浮点日志 | 28 个输出用例、bit/modff 等价及依赖裁剪 | PASS |
| NH06 | 配置与裁剪 | 主机/Arm 配置矩阵、fast 路径及 C/ASM 选择 | PASS |
| NH07 | 自定义配置 | 主机、Make、Cube CMake、依赖记录、干净重建和非法配置 | PASS |
| NH08 | RTT 写入路径 | 失败、短写、恢复、C 环形缓冲边界 | PASS |
| NH09 | 实际工程集成 | 8 工程 Debug/Release、功能传播、ASM/C、可复现 | PASS |
| NH10 | 资源与可复现构建 | 240 次隔离资源链接、72 次实际工程构建、冻结预算 | PASS |
| NH11 | typed 整数/指针 | 4 个主机 profile、64 位主机值和 M0 ELF | PASS |
| NH12 | 回归语料 | 13 API 写入矩阵、100000 个 binary32、路径等价 | PASS |

统计：`12 PASS / 0 FAIL / 0 SKIP`。

临时/可选 NH 统计：`3 PASS / 0 FAIL`（`NHO_LEGACY_CONFIG`、`NHT_FMT_COMPAT`、
`NHT_FLASH_OPT`）。这三项按测试计划独立于 `--all` 执行，不计入 NH01-NH12 的 12 项发布
门禁统计。

板级统计：`182 PASS / 0 FAIL`（H7B0 46、F042 41、F103 46、F411 49）。

## 4. 关键量化结果

### 4.1 主机质量与覆盖率

| 文件 | 行覆盖率 | 分支覆盖率 |
|---|---:|---:|
| `rtt_printf.c` | 90.76% | 89.47% |
| `rtt_log.c` | 92.68% | 85.71% |
| `rtt_float.c` | 95.70% | 92.11% |
| **合计** | **91.99%** | **89.76%** |

总行覆盖率超过 90% 门槛，总分支覆盖率超过 80% 门槛。ASan、UBSan 和 Cppcheck 的诊断文件均为 0 B；M0、M3、M4F、M7 的 `-O0`/`-Os` 共 8 个最终 ELF 构建通过，M0 未引入整数除法辅助符号。

### 4.2 实际工程集成

| MCU | Make Debug/Release | CMake Debug/Release | 功能配置 | Skip 路径 | 判定 |
|---|---|---|---|---|---|
| F042/M0 | 通过 | 通过 | typed、typed-float、float-fast | C | PASS |
| F103/M3 | 通过 | 通过 | typed、typed-float、float-fast | ASM/C | PASS |
| F411/M4F | 通过 | 通过 | typed、typed-float、float-fast | ASM/C | PASS |
| H7B0/M7 | 通过 | 通过 | typed、typed-float、float-fast | ASM/C | PASS |

全部工程通过初始清理、Debug/Release 完整构建、无变化增量构建、清理重建、Release 哈希复现和链接产物检查。外部工程的 `ARM_SEGGER_RTT` 均为干净工作树并解析到完整候选提交 `29d61fc8d972b1436a13da993ef6f71f903dc86c`。

### 4.3 资源数据边界

本轮 NH10 使用相同 benchmark 对象分别生成 RTT 完整链接和配对控制链接，以两者 Flash
差值作为 RTT 库链接占用。该口径排除 benchmark 自身，同时包含当前配置实际拉入的 RTT
代码、数据初值和运行库支持。完整数据保存在 `library_footprint.csv`，以下两张表来自同目录
的 `flash_footprint.md`；实际工程完整镜像大小只作为链接容量和工程内差异证据。

#### default RTT 库 Flash

| 架构 | 完整链接 Flash (B) | 配对控制 Flash (B) | RTT 库 Flash (B) |
|---|---:|---:|---:|
| Cortex-M0 | 2632 | 193 | 2439 |
| Cortex-M3 | 2704 | 193 | 2511 |
| Cortex-M4F | 2948 | 197 | 2751 |
| Cortex-M7 | 2948 | 197 | 2751 |

#### typed_combo RTT 库 Flash

| 架构 | 完整链接 Flash (B) | 配对控制 Flash (B) | RTT 库 Flash (B) |
|---|---:|---:|---:|
| Cortex-M0 | 1794 | 108 | 1686 |
| Cortex-M3 | 1894 | 108 | 1786 |
| Cortex-M4F | 1998 | 112 | 1886 |
| Cortex-M7 | 2002 | 112 | 1890 |

NH10 共执行 312 次新目录构建：隔离资源矩阵包含 10 个配置、4 种架构、current/control
两类链接并各重复 3 次，共 240 次；8 个实际工程的 default、Skip ASM、forced C 矩阵共
72 次。所有最小配置、默认配置、formatter、typed integer、legacy float fast-off/on、
Skip C/ASM 和依赖裁剪预算均通过；这些无需开发板的原 HW08 资源检查现统一由 NH10 负责。
矩阵内所有配置连续三次生成相同 ELF SHA-256；最大固定栈帧为 104 B，低于 256 B 门槛。

### 4.4 板级预发布结果

| MCU | Make | CMake | 实际运行数 | HW01-HW08 | 判定 |
|---|---:|---:|---:|---|---|
| STM32H7B0VB | 23 | 23 | 46 | 全部计划组合通过 | PASS |
| STM32F042G6 | 21 | 20 | 41 | 全部计划组合通过 | PASS |
| STM32F103C8 | 23 | 23 | 46 | 全部计划组合通过 | PASS |
| STM32F411CE | 26 | 23 | 49 | 全部计划组合通过 | PASS |
| **合计** | **93** | **89** | **182** | 0 FAIL | **PASS** |

H7B0 的 8 个 HW08 marker 组合均完整捕获 16384 帧，实测速率
`0.360021973`～`0.360022738 MiB/s`，无 CRC 错误、缺帧、重复或乱序。F042 Make
资格协议独立运行 3 次，每次完整捕获 8192 帧，速率均为 `0.020000000 MiB/s`，目标拒绝数
为 0。F411 Make 资格协议独立运行 3 次，每次完整捕获 16384 帧，速率均为
`0.099999894 MiB/s`，目标接受 16384、拒绝 0，且无 CRC 错误、缺帧、重复、乱序或外来
run_id 帧。

F411 子矩阵使用 J-Link S/N `20721668`，`DBGMCU_IDCODE=0x10006431`（Device ID
`0x431`），确认目标为 STM32F411。最终证据经版本化 runner 按 F4 子矩阵复核，输出
`release evidence PASS: 49 exact runs`；所有 metadata 中 runner 和实际加载库均为干净的
`29d61fc8d972b1436a13da993ef6f71f903dc86c`。

F103 子矩阵使用同一 J-Link，目标内核为 Cortex-M3 r2p1，
`DBGMCU_IDCODE=0x20036410`（Device ID `0x410`），确认目标为 STM32F103 中密度器件。
Make/CMake 各完成 23 组，包含 8 个 HW08 功能 marker 组合；该 MCU 的矩阵未配置吞吐
资格协议。最终证据经版本化 runner 按 F1 子矩阵复核，输出
`release evidence PASS: 46 exact runs`；46 份 metadata 中 runner 和实际加载库均为干净的
`29d61fc8d972b1436a13da993ef6f71f903dc86c`。

## 5. 外部工程记录

| 工程 | 分支 | 工程提交 |
|---|---|---|
| `stm32f042g6make` | `master` | `8f16eec042a04b2f98e877fc0cbdc023e0bdd27b` |
| `stm32f042g6cmake` | `master` | `feb1fbe83dc9d5ce5d7f030ebcb32cbfb3d45188` |
| `stm32f103c8make` | `master` | `8e3586b18190f2f061bbc4f8ab98e2e75927f7a4` |
| `stm32f103c8cmake` | `master` | `c348ad32e38d9ce5417fe06e9c915dddcf6cc165` |
| `stm32f411cemake` | `master` | `12c04890155c1901f847ddd4a75872a96ce9d458` |
| `stm32f411cecmake` | `master` | `8b045637f96313b1abea9ef590a52dc0fe7f91b8` |
| `stm32h7b0vbmake` | `master` | `1bbcb320caa8c1184a8d6825280442edb1f86039` |
| `stm32h7b0vbcmake` | `master` | `53c8a7fc72ab2708984916d08655663a69072ba4` |

这些工程是为 RTT 库提供真实 MCU、Make/CMake 和工具链环境的本地参考工程，不属于本仓库发布物。它们包含本地 RTT 指向或配置，因此工作树状态只作为诊断信息，不参与 PASS/FAIL；本轮判定以实际加载的 RTT 候选 SHA、有效编译参数、链接产物和测试闭环为依据。

## 6. 失败与残余风险

最终 NH 和板级证据没有失败用例，NH09 和 NH10 的 `failures.txt` 均为空。统一 NH runner 使用
`--keep-going` 完成全矩阵收集；该选项不改变任何单项验收标准。不属于发布门禁的
`NHO_LEGACY_CONFIG`、`NHT_FMT_COMPAT` 和 `NHT_FLASH_OPT` 也已另行执行并全部通过。该辅助
批次的 metadata 记录候选 SHA 为 `29d61fc8d972b1436a13da993ef6f71f903dc86c`，同时记录
`source_dirty: true`，因此其结果作为额外验证记录，不改变干净候选上的正式发布门判定。


残余边界：

1. HW09 长时间稳定性按测试计划仍属于后续扩展项，不是当前版本发布阻塞项，但不得宣称已完成 8 h/24 h 稳定性验证。
2. NH09/NH10 的集成和资源结论只适用于本报告记录的参考工程组合；工程配置或工具链变化后应重新验证相关组合。

## 7. 证据索引

| 范围 | 证据 |
|---|---|
| 汇总 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/SUMMARY.md` |
| 临时/可选 NH 汇总 | `TEST_EVIDENCE/NH_AUX_29d61fc_20260904/SUMMARY.md` |
| 临时/可选 NH 结果 | `TEST_EVIDENCE/NH_AUX_29d61fc_20260904/{NHO_LEGACY_CONFIG,NHT_FMT_COMPAT,NHT_FLASH_OPT}/` |
| 各用例结果 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NHxx/result.txt` |
| 环境与退出码 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NHxx/metadata.json` |
| 完整运行日志 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NHxx/run.log` |
| 覆盖率与质量门 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NH01/artifacts/quality/` |
| 实际工程集成 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NH09/artifacts/` |
| 资源与可复现构建 | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NH10/artifacts/` |
| RTT 库 Flash | `TEST_EVIDENCE/NH_PRERELEASE_29d61fc_20260904/NH10/artifacts/resource_usage/flash_footprint.md` |
| H7B0 HW01-HW08 | `TEST_EVIDENCE/HW_H7_PRERELEASE_29d61fc_20260904/` |
| F042 HW01-HW08 | `TEST_EVIDENCE/HW_F0_PRERELEASE_29d61fc_20260904/` |
| F103 HW01-HW08 | `TEST_EVIDENCE/HW_F1_PRERELEASE_29d61fc_20260904/` |
| F411 HW01-HW08 | `TEST_EVIDENCE/HW_F4_PRERELEASE_29d61fc_20260904_ATTEMPT2/` |
| F411 首次失败现场 | `TEST_EVIDENCE/HW_F4_PRERELEASE_29d61fc_20260904/HW01/f411_make/debug/profile-0/` |

## 8. 签署结论

| 结论项 | 状态 |
|---|---|
| NH01-NH12 | **12/12 PASS** |
| 临时/可选 NH | **3/3 PASS** |
| H7B0 HW01-HW08 | **46/46 PASS** |
| F042 HW01-HW08 | **41/41 PASS** |
| F103 HW01-HW08 | **46/46 PASS** |
| F411 HW01-HW08 | **49/49 PASS** |
| 本轮未关闭产品缺陷 | 无 |
| NH 发布门 | **通过** |
| HW01-HW08 完整发布矩阵 | **182/182 PASS** |
| HW09 | 不属于当前发布阻塞项，未执行 |
| 当前预发布门 | **通过** |

本报告只汇总 `29d61fc` 本轮 NH 及 H7B0/F042/F103/F411 板级正式证据，不继承其他候选
结果，不把外部参考工程限定为用户必须采用的配置。HW09 扩展项不因当前预发布门通过而被视为已执行。
