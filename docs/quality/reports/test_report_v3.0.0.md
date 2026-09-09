# ARM_SEGGER_RTT 3.0.0 发布候选测试报告

> 报告日期：2026-09-09
> 报告状态：NH01-NH12 与完整 HW01-HW08 发布矩阵均已通过
> 被测分支：`release`
> 被测提交：`d564672e6a0a5682d13ce1e9c5add66680b76dd6`
> 测试方案：`docs/quality/test_plan.md`（候选提交 `d564672` 内现行版）
> 已执行范围：NH01-NH12；F103、F042、F411、H7B0 完整 HW01-HW08
> 待执行范围：无；HW09 为非阻塞扩展项，本轮未执行
> 原始证据：`TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/`、`TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/`

## 1. 当前结论

**发布判定：NH01-NH12 为 12/12 PASS；STM32F103C8 为 46/46 PASS；STM32F042G6 为 41/41 PASS；STM32F411CE 为 49/49 PASS；STM32H7B0VB 为 46/46 PASS。完整 HW 矩阵为 182/182 PASS，精确证据验收通过，3.0.0 整体发布门通过。**

本轮 NH 统一 runner 退出码为 0。主机质量门、API 与配置矩阵、RTT 写入路径、八个实际
工程集成、资源预算、三次可复现构建以及 100000 个 binary32 随机模式全部满足现行测试
方案的验收标准。未发现本轮 NH 范围内的未关闭产品缺陷。

当前候选的四款 MCU、八个 Make/CMake 工程、Debug/Release、全部适用 profile、Up Buffer、
Skip 路径和 HW08 组合均已通过。独立验收器确认 182 个精确路径、结果、metadata、参数和
候选 SHA 完整匹配。本报告不继承历史候选 `29d61fc` 的任何测试结果。

## 2. 测试对象与环境

| 项目 | 内容 |
|---|---|
| 日志库 | ARM_SEGGER_RTT |
| 目标版本 | 3.0.0 |
| 候选提交 | `d564672e6a0a5682d13ce1e9c5add66680b76dd6` |
| 主仓库状态 | 测试启动和 runner 完成时均为干净工作树 |
| 主机 | macOS 12.2.1（Darwin 21.3.0） |
| Python | 3.7.7rc1 |
| Shell | GNU Bash 3.2.57 |
| 主机编译器 | Apple Clang 13.1.6 |
| Arm 工具链 | Arm GNU Toolchain 12.2.1（Build arm-12.24） |
| Cube CMake | STM32 扩展内置 `cube-cmake`，CMake 4.3.1 |
| 并行度 | 8 |
| MCU/架构 | STM32F042G6/M0、STM32F103C8/M3、STM32F411CE/M4F、STM32H7B0VB/M7 |
| 工程矩阵 | 4 MCU x Make/CMake x Debug/Release |

执行命令：

```sh
python3 tests/NH/run_nh.py --all --keep-going \
  --evidence-dir TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909
```

NH runner 在整套测试期间冻结候选 SHA。12 个用例的 metadata 均记录
`repository_sha=d564672e6a0a5682d13ce1e9c5add66680b76dd6` 和
`source_dirty=false`。NH09、NH10 记录的八个工程内 RTT 库也全部为同一干净候选。

## 3. NH 结果总览

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

`NHO_LEGACY_CONFIG`、`NHT_FMT_COMPAT` 和 `NHT_FLASH_OPT` 未包含在本批次中。它们是现行
测试计划定义的可选或临时测试，不属于 NH01-NH12 发布门禁，本报告不继承历史候选的执行结果。

## 4. 关键量化结果

### 4.1 主机质量与覆盖率

| 文件 | 行覆盖率 | 分支覆盖率 |
|---|---:|---:|
| `rtt_printf.c` | 90.76% | 89.47% |
| `rtt_log.c` | 92.68% | 85.71% |
| `rtt_float.c` | 95.70% | 92.11% |
| **合计** | **91.99%** | **89.76%** |

总行覆盖率超过 90% 门槛，总分支覆盖率超过 80% 门槛。ASan、UBSan 和 Cppcheck 均通过；
M0、M3、M4F、M7 的 `-O0`/`-Os` 共 8 个最终 ELF 构建通过，M0 未引入整数除法辅助符号。

### 4.2 实际工程集成

| MCU | Make Debug/Release | CMake Debug/Release | 功能配置 | Skip 路径 | 判定 |
|---|---|---|---|---|---|
| F042/M0 | 通过 | 通过 | typed、typed-float、float-fast | C | PASS |
| F103/M3 | 通过 | 通过 | typed、typed-float、float-fast | ASM/C | PASS |
| F411/M4F | 通过 | 通过 | typed、typed-float、float-fast | ASM/C | PASS |
| H7B0/M7 | 通过 | 通过 | typed、typed-float、float-fast | ASM/C | PASS |

全部工程通过初始清理、Debug/Release 完整构建、无变化增量构建、清理重建、Release 哈希
复现和链接产物检查。八个工程实际参与构建的 `ARM_SEGGER_RTT` 均为干净工作树，并解析到
候选提交 `d564672e6a0a5682d13ce1e9c5add66680b76dd6`。

### 4.3 资源数据边界

本轮 NH10 使用相同 benchmark 对象分别生成 RTT 完整链接和配对控制链接，以两者 Flash
差值作为 RTT 库链接占用。该口径排除 benchmark 自身，同时包含当前配置实际拉入的 RTT
代码、数据初值和运行库支持。

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

NH10 共执行 312 次新目录构建：240 次隔离资源矩阵构建和 72 次实际工程构建。所有当前
配置连续三次生成相同 ELF SHA-256；RAM、相对资源、依赖裁剪和固定栈预算全部通过，最大
固定栈帧为 104 B，低于 256 B 门槛。链接库 Flash 按测试计划记录，不设置统一绝对上限。

### 4.4 HW 发布矩阵状态

| MCU | 计划运行数 | 当前结果 |
|---|---:|---|
| STM32H7B0VB | 46 | **46/46 PASS** |
| STM32F042G6 | 41 | **41/41 PASS** |
| STM32F103C8 | 46 | **46/46 PASS** |
| STM32F411CE | 49 | **49/49 PASS** |
| **合计** | **182** | **182/182 PASS** |

历史候选 `29d61fc` 的板级结果不适用于当前候选。当前候选必须使用
`tests/HW/run_hw.py --release-suite` 完成全部 182 次真机运行，并通过
`--verify-evidence` 精确验收。

F103 子矩阵使用 J-Link S/N `20721668`，VTref `3.285 V`，目标内核识别为
Cortex-M3 r2p1。Make 与 CMake 各完成 23 项，全部 `result.txt` 为 `PASS`；46 份 metadata
中的 `library_sha` 和 `actual_library_sha` 均为候选完整 SHA，且 `source_dirty=false`。

F042 子矩阵的目标内核识别为 Cortex-M0 r0p0。Make 完成 21 项，CMake 完成 20 项，全部
`result.txt` 为 `PASS`；41 份 metadata 中的 `library_sha` 和 `actual_library_sha` 均为
候选完整 SHA，且 `source_dirty=false`。其中 F042 Make 的 HW08 吞吐资格测试连续执行三轮，
每轮均接收 `8192/8192` 帧，吞吐率为 `0.020000000 MiB/s`，`accepted=8192`、
`rejected=0`，且没有 CRC、缺帧、重复、乱序或外来帧错误。

F411 子矩阵使用 J-Link S/N `20721668`，VTref `3.288 V`，目标内核识别为
Cortex-M4 r0p1。Make 完成 26 项，CMake 完成 23 项，全部 `result.txt` 为 `PASS`；49 份
metadata 中的 `library_sha` 和 `actual_library_sha` 均为候选完整 SHA，且
`source_dirty=false`。其中 F411 Make 的 HW08 正式吞吐资格测试连续执行三轮，每轮均接收
`16384/16384` 个 64 B 帧，测得 `0.099999894 MiB/s`，`accepted=16384`、`rejected=0`，
且没有 CRC、缺帧、重复、乱序或外来帧错误。

H7 使用 J-Link S/N `20721668`，VTref `3.288 V`，目标内核识别为 Cortex-M7 r1p1。通过
正式 `--release-suite` 入口优先执行 H7，Make 与 CMake 各完成 23 项，
全部 `result.txt` 为 `PASS`；46 份 metadata 中的候选 SHA 完整匹配且
`source_dirty=false`。8 个 HW08 组合均完整捕获 `16384/16384` 个 64 B 帧，吞吐率为
`0.360021973` 至 `0.360022737 MiB/s`，CRC、缺帧、重复、意外序号和乱序均为 0；最大运行
时栈使用 720/1024 B，保护区通过。

H7 完成后 runner 停在下一块 MCU 的人工换板提示，未重复执行已经通过的 F042/F103/F411。
随后使用版本化矩阵独立执行 `--verify-evidence`，合并证据通过全部 182 项精确验收。

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

这些工程是 RTT 库的本地参考测试环境，不属于本仓库发布物。本轮结论以实际加载的 RTT
候选 SHA、有效编译参数、链接产物和测试闭环为依据，不要求用户采用相同工程提交或配置。

## 6. 失败与残余风险

本轮 NH01-NH12 没有失败用例，NH09 和 NH10 的 `failures.txt` 均为空。统一 runner 使用
`--keep-going` 完成全矩阵收集；该选项不改变任何单项验收标准。

验证边界：

1. 可选或临时测试 `NHO_LEGACY_CONFIG`、`NHT_FMT_COMPAT`、`NHT_FLASH_OPT` 未在本批次执行。
2. HW09 长时间稳定性不属于当前发布阻塞项，但不得宣称已完成 8 h/24 h 稳定性验证。
3. NH09/NH10 结论只适用于本报告记录的参考工程、工具链和配置组合。

## 7. 证据索引

| 范围 | 证据 |
|---|---|
| 汇总 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/SUMMARY.md` |
| 各用例结果 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NHxx/result.txt` |
| 环境与退出码 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NHxx/metadata.json` |
| 完整运行日志 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NHxx/run.log` |
| 覆盖率与质量门 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NH01/artifacts/quality/` |
| 实际工程集成 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NH09/artifacts/` |
| 资源与可复现构建 | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NH10/artifacts/` |
| RTT 库 Flash | `TEST_EVIDENCE/NH_RELEASE_3.0.0_d564672_20260909/NH10/artifacts/resource_usage/flash_footprint.md` |
| F103 HW01-HW08 有效结果 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/{HW01..HW08}/f103_*/` |
| F042 HW01-HW08 有效结果 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/{HW01..HW08}/f042_*/` |
| F042 HW08 三轮吞吐资格测试 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/HW08/f042_make/release/profile-0/run-{1..3}/` |
| F411 HW01-HW08 有效结果 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/{HW01..HW08}/f411_*/` |
| F411 HW08 三轮吞吐资格测试 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/HW08/f411_make/release/qualification/run-{1..3}/` |
| H7 最新正式执行 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/{HW01..HW08}/h7b0_*/` |
| 完整 HW01-HW08 | `TEST_EVIDENCE/HW_RELEASE_3.0.0_d564672_20260909_ATTEMPT2/` |
| 182 项精确证据验收 | **PASS：182 exact runs，候选 `d564672e6a0a5682d13ce1e9c5add66680b76dd6`** |

## 8. 签署状态

| 结论项 | 状态 |
|---|---|
| NH01-NH12 | **12/12 PASS** |
| 临时/可选 NH | 未执行，不属于发布门禁 |
| H7B0 HW01-HW08 | **46/46 PASS** |
| F042 HW01-HW08 | **41/41 PASS** |
| F103 HW01-HW08 | **46/46 PASS** |
| F411 HW01-HW08 | **49/49 PASS** |
| 本轮未关闭产品缺陷 | 无 |
| NH 发布门 | **通过** |
| HW01-HW08 完整发布矩阵 | **182/182 PASS** |
| HW09 | 不属于当前发布阻塞项，未执行 |
| 3.0.0 整体发布门 | **通过** |

本报告汇总候选 `d564672` 的 NH01-NH12 与完整 HW01-HW08 正式证据，不继承其他候选结果。
现行测试计划定义的 3.0.0 发布阻塞项已全部通过，候选可以进入发布
报告提交、版本标记与发布阶段；发布说明必须继续保留 HW09 未执行的验证边界。
