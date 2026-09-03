# ARM_SEGGER_RTT NH 全量测试报告

> 报告日期：2026-09-02
> 被测分支：`perf_flash`
> 被测提交：`b0b37cba62d6745368604029719c1ce810f59e44`
> 测试方案：`docs/quality/test_plan.md`（提交 `b0b37cb` 现行版）
> 执行范围：NH01-NH12
> 原始证据：`TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/`

## 1. 最终结论

**NH 综合判定：通过。NH01-NH12 在候选提交 `b0b37cb` 上全量运行，12/12 通过。**

本轮统一 runner 退出码为 0。主机质量门、API 与配置矩阵、RTT 写入路径、八个实际工程集成、资源预算、三次可复现构建以及 100000 个 binary32 随机模式全部满足测试方案的验收标准。未发现本轮 NH 范围内的未关闭产品缺陷。

本报告仅关闭非硬件回归范围。HW01-HW08 未在本轮执行，因此不能仅凭本报告判定完整发布测试计划已经关闭；完整发布仍需结合候选提交对应的板级回归证据。

## 2. 测试对象与环境

| 项目 | 内容 |
|---|---|
| 日志库 | ARM_SEGGER_RTT |
| 候选提交 | `b0b37cba62d6745368604029719c1ce810f59e44` |
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
python3 tests/NH/run_nh.py --all \
  --evidence-dir TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1
```

NH09 完整执行了 M0、M3、M4F、M7 的 8 个 Make/CMake 工程，未缩减测试矩阵。

## 3. 结果总览

| 编号 | 测试项目 | 关键结果 | 状态 |
|---|---|---|---|
| NH01 | 核心质量门 | ASan、UBSan、Cppcheck、覆盖率、8 个 Arm ELF | PASS |
| NH02 | 四级日志 | 12 个等级用例、连续序列及返回路径 | PASS |
| NH03 | 通用输出 | print/string、关闭行为及副作用 | PASS |
| NH04 | formatter | 37 个当前用例、8 B 分段缓冲及 release 对比 | PASS |
| NH05 | 浮点日志 | 28 个输出用例、bit/modff 等价及依赖裁剪 | PASS |
| NH06 | 配置与裁剪 | 主机/Arm 配置矩阵、fast 路径及 C/ASM 选择 | PASS |
| NH07 | 自定义配置 | 主机、Make、Cube CMake、依赖记录、干净重建和非法配置 | PASS |
| NH08 | RTT 写入路径 | 失败、短写、恢复、C 环形缓冲边界 | PASS |
| NH09 | 实际工程集成 | 8 工程 Debug/Release、功能传播、ASM/C、可复现 | PASS |
| NH10 | 资源与可复现构建 | 144 次隔离资源构建、72 次实际工程构建、冻结预算 | PASS |
| NH11 | typed 整数/指针 | 4 个主机 profile、64 位主机值和 M0 ELF | PASS |
| NH12 | 回归语料 | 13 API 写入矩阵、100000 个 binary32、路径等价 | PASS |

统计：`12 PASS / 0 FAIL / 0 SKIP`。

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

全部工程通过初始清理、Debug/Release 完整构建、无变化增量构建、清理重建、Release 哈希复现和链接产物检查。外部工程的 `ARM_SEGGER_RTT` 均为干净工作树并解析到完整候选提交 `b0b37cba62d6745368604029719c1ce810f59e44`。

### 4.3 资源数据边界

本历史报告对应提交 `b0b37cb`。该候选的 NH10 证据记录了隔离 fixture 最终 ELF 和实际工程
完整 ELF，但没有使用相同 benchmark 对象生成配对控制链接，因此不能从该证据中严格分离
benchmark 与 RTT 库自身的 Flash 占用。原先列出的实际工程整体 Flash 包含启动代码、HAL、
应用和 RTT，不能作为 RTT 库大小，故不再作为库 Flash 展示。

从下一候选开始，NH10 通过同一 benchmark 对象的“RTT 完整链接 Flash - 配对控制链接 Flash”
生成 `artifacts/resource_usage/library_footprint.csv`；面向人工阅读的
`artifacts/resource_usage/flash_footprint.md`
分别以 `default` 和 `typed_combo` 两个表格列出四种架构的库链接 Flash 占用。完整 CSV 保留
所有配置，实际工程完整镜像大小只保留为链接容量和工程内差异证据。

以下两张表是后续候选的固定报告结构。本报告对应的旧证据无法按新口径计算库 Flash，
因此数值单元格保留为空；不得用实际工程完整 ELF 数据回填。

#### default RTT 库 Flash

| 架构 | 完整链接 Flash (B) | 配对控制 Flash (B) | RTT 库 Flash (B) |
|---|---:|---:|---:|
| Cortex-M0 |  |  |  |
| Cortex-M3 |  |  |  |
| Cortex-M4F |  |  |  |
| Cortex-M7 |  |  |  |

#### typed_combo RTT 库 Flash

| 架构 | 完整链接 Flash (B) | 配对控制 Flash (B) | RTT 库 Flash (B) |
|---|---:|---:|---:|
| Cortex-M0 |  |  |  |
| Cortex-M3 |  |  |  |
| Cortex-M4F |  |  |  |
| Cortex-M7 |  |  |  |

NH10 共执行 216 次新目录构建：隔离资源矩阵 144 次，8 个实际工程的 default、Skip ASM、forced C 矩阵 72 次。所有最小配置、默认配置、formatter、typed integer、legacy float fast-off/on、Skip C/ASM 和依赖裁剪预算均通过；这些无需开发板的原 HW08 资源检查现统一由 NH10 负责。矩阵内所有配置连续三次生成相同 ELF SHA-256；最大固定栈帧为 104 B，低于 256 B 门槛。

本报告不对 `b0b37cb` 给出无法由当轮证据严格支持的 RTT 库 Flash 数值。

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

本轮 runner 没有失败用例，NH09 和 NH10 的 `failures.txt` 均为空。NH04 输出的旧 `release@705ee8e` 返回值差异是版本对比用例的预期观察；当前工作树返回值无差异，公共输出和长消息写入一致，因此该用例判定为 PASS。

残余边界：

1. 本轮为非硬件测试，没有烧录开发板，也没有形成新的 HW01-HW08 板级运行证据。
2. HW09 长时间稳定性按测试计划仍属于后续扩展项，不是当前版本发布阻塞项，但不得宣称已完成 8 h/24 h 稳定性验证。
3. NH09/NH10 的集成和资源结论只适用于本报告记录的参考工程组合；工程配置或工具链变化后应重新验证相关组合。

## 7. 证据索引

| 范围 | 证据 |
|---|---|
| 汇总 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/SUMMARY.md` |
| 各用例结果 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/NHxx/result.txt` |
| 环境与退出码 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/NHxx/metadata.json` |
| 完整运行日志 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/NHxx/run.log` |
| 覆盖率与质量门 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/NH01/artifacts/quality/` |
| 实际工程集成 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/NH09/artifacts/` |
| 资源与可复现构建 | `TEST_EVIDENCE/NH_RELEASE_b0b37cb_20260902_RERUN1/NH10/artifacts/` |

## 8. 签署结论

| 结论项 | 状态 |
|---|---|
| NH01-NH12 | **12/12 PASS** |
| 本轮未关闭产品缺陷 | 无 |
| NH 发布门 | **通过** |
| HW01-HW08 | 本轮未执行 |
| HW09 | 不属于当前发布阻塞项，未执行 |
| 完整发布计划 | 需结合候选提交对应的 HW01-HW08 证据判定 |

本报告只汇总 `b0b37cb` 本轮统一 NH runner 产生的正式证据，不继承其他候选结果，不把外部参考工程限定为用户必须采用的配置，也不替代板级测试。
