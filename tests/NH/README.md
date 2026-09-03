# NH 非硬件测试运行器

`run_nh.py` 是必需 NH01-NH12、可选 `NHO_*` 和临时 `NHT_*` 测试的统一入口。它保留各测试用例现有的脚本作为测试实现，并提供配置管理、依赖检查、统一日志和统一证据目录。运行这些测试不需要开发板或调试探针。

当前所有非硬件测试实现文件均位于本目录下：

```text
tests/NH/
  run_nh.py                 统一运行器
  config.example.json       可移植的本地配置模板
  cases/NH01/ ... NH12/     必需测试脚本、fixture、配置和资源
  cases/NHO_*/               可选测试
  cases/NHT_*/               临时测试，完成对应优化确认后退役
  support/                  多个测试用例共用的辅助文件
```

当前每个测试用例均通过 `tests/NH/cases/<CASE_ID>/run.sh` 执行；`tests/` 根目录下不再存放 NH 测试源码、fixture、配置或可执行脚本。

运行器原生支持 macOS。在 Windows 上，运行器使用 Python 和 Git Bash，以便两个平台执行相同的 POSIX 测试脚本和验收规则。

## 测试用例和依赖

| 用例 | 目的 | 额外依赖 |
|---|---|---|
| NH01 | 核心主机测试、sanitizer、静态分析、覆盖率和四架构 Arm ELF | Clang/LLVM、Cppcheck、Arm GNU Toolchain |
| NH02 | 非 Lite 彩色模式下的四级日志输出和成功返回值 | 主机 C 编译器 |
| NH03 | 打印和字符串 API 的常规输入、NULL、独立关闭及参数副作用 | 主机 C 编译器 |
| NH04 | 当前 formatter 格式、边界和分段 flush | 主机 C 编译器 |
| NH05 | 浮点日志和符号裁剪 | 主机 C 编译器、主机 `nm` |
| NH06 | 功能开关、Arm 符号与依赖裁剪、C/ASM 路径及 ASM/cache 冲突 | 主机 C 编译器、Arm GNU Toolchain |
| NH07 | 主机、Make 和 CMake 的 RTT 配置覆盖、依赖记录及干净重建 | NH06 所需工具、Make、Ninja、`cube-cmake` |
| NH08 | API 写入失败、短写与恢复，以及 C `NO_BLOCK_SKIP` 环形缓冲行为 | 主机 C 编译器 |
| NH09 | 八个 STM32 工程的集成矩阵 | Arm 工具、Make、Ninja、STM32 Cube 工具、工程工作区 |
| NH10 | 资源预算和可复现构建 | 与 NH09 相同的环境和工程 |
| NH11 | 带类型的整数和指针 API | 主机编译器/`nm`、Arm GNU Toolchain |
| NH12 | API 失败/短写/恢复、formatter flush 失败及浮点路径回归语料 | 主机 C 编译器 |
| `NHT_FMT_COMPAT`（临时） | 当前 formatter 与兼容旧布局基线的行为确认 | 主机 C 编译器、Git、tar |
| `NHO_LEGACY_CONFIG`（可选） | 旧浮点配置宏的迁移诊断 | 主机 C 编译器 |
| `NHT_FLASH_OPT`（临时） | M0 fast-path 及四架构 default/release Flash、RAM 差异 | Arm GNU Toolchain、Git、tar |

只列出测试用例及其依赖组，不实际运行：

```sh
python3 tests/NH/run_nh.py --list
```

## 配置

将 `tests/NH/config.example.json` 复制为已被 Git 忽略的本地文件 `tests/NH/config.local.json`，然后根据当前机器调整路径。运行器会自动加载该文件。使用 `--config PATH` 可以指定其他配置文件。

运行 NH09 和 NH10 时，`workspace` 目录中必须包含以下同级工程：

```text
workspace/
  ARM_SEGGER_RTT/
  stm32f042g6make/     stm32f042g6cmake/
  stm32f103c8make/     stm32f103c8cmake/
  stm32f411cemake/     stm32f411cecmake/
  stm32h7b0vbmake/     stm32h7b0vbcmake/
```

命令行选项的优先级高于 JSON 配置值。顶层 `environment` 对象中的环境变量应用于所有测试用例；`cases.<CASE_ID>.environment` 中的值只应用于对应的测试用例。

### macOS 配置示例

安装或配置主机编译器、Arm GNU Toolchain、Make、Ninja 和 STM32 Cube VS Code 扩展。典型的本地配置如下：

```json
{
  "schema_version": 1,
  "shell": "sh",
  "host_cc": "cc",
  "host_nm": "nm",
  "toolchain_prefix": "/opt/arm-gnu-toolchain-VERSION-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-",
  "cube_cmake": "/Users/USER/.vscode/extensions/stmicroelectronics.stm32cube-ide-build-cmake-VERSION-darwin-arm64/resources/cube-cmake/darwin/aarch64/cube-cmake",
  "cube": "/Users/USER/.local/stm32cube/bin/cube",
  "workspace": "/Users/USER/git/github",
  "jobs": 8,
  "environment": {},
  "cases": {}
}
```

### Windows 配置示例

安装 Python 3、Git for Windows、主机 GCC 或 Clang、Arm GNU Toolchain、GNU Make、Ninja 和 STM32 Cube VS Code 扩展。可以在 PowerShell 或命令提示符中运行命令；配置的 Git Bash 会负责执行各测试用例。JSON 中的反斜杠必须转义：

```json
{
  "schema_version": 1,
  "shell": "C:\\Program Files\\Git\\bin\\bash.exe",
  "host_cc": "gcc",
  "host_nm": "nm",
  "toolchain_prefix": "C:\\Program Files (x86)\\Arm GNU Toolchain arm-none-eabi\\bin\\arm-none-eabi-",
  "cube_cmake": "C:\\Users\\USER\\.vscode\\extensions\\stmicroelectronics.stm32cube-ide-build-cmake-VERSION-win32-x64\\resources\\cube-cmake\\win32\\x64\\cube-cmake.exe",
  "cube": "C:\\Users\\USER\\.local\\stm32cube\\bin\\cube.exe",
  "workspace": "C:\\Users\\USER\\git\\github",
  "jobs": 8,
  "environment": {},
  "cases": {}
}
```

Git Bash 必须提供脚本使用的标准 Unix 命令，包括 `awk`、`cmp`、`find`、`grep`、`sed`、`tar` 和 `sha256sum`。运行器使用 `cygpath` 转换配置中的绝对路径，不要求启用 Windows 开发者模式或符号链接权限。

## 运行测试

可以运行单个用例、按顺序运行多个用例，或运行完整测试集：

```sh
python3 tests/NH/run_nh.py --case NH01
python3 tests/NH/run_nh.py --case NH02 --case NH03 --case NH04
python3 tests/NH/run_nh.py --ci
python3 tests/NH/run_nh.py --all
python3 tests/NH/run_nh.py --case NHT_FMT_COMPAT
python3 tests/NH/run_nh.py --case NHO_LEGACY_CONFIG
python3 tests/NH/run_nh.py --case NHT_FLASH_OPT
```

`--ci` 是常规 CI 入口，固定运行不依赖外部工程的 NH01-NH08、NH11 和 NH12；NH09、
NH10 及 `NHO_*`、`NHT_*` 均不在该集合中。`--all` 是正式 NH01-NH12 发布入口。两个入口
都要求主仓库为已提交且干净的候选；dirty 工作树会在创建证据目录和执行任何用例之前
被拒绝。单独使用 `--case` 仍允许开发阶段预回归，但其
metadata 会明确记录 `source_dirty: true`，不能作为正式发布证据。runner 在整套测试开始时
冻结候选 SHA，并在每个用例开始和结束时确认候选身份未发生变化。

`NHT_FMT_COMPAT` 不属于 `--all`，也不是发布或常规 CI 门禁。它用于 formatter 优化期间
按需确认历史兼容性，是为本次 Flash 优化特制的临时测试；优化代码完成合并后不再使用，
也不纳入后续版本的常规回归或 CI，届时退役并删除。必须显式使用
`--case NHT_FMT_COMPAT` 运行。默认基线为 Git `release` 分支。只有当其他基线可解析为
提交、包含旧路径 `RTT/SEGGER_RTT_printf.c`，并符合本用例既定兼容性差异时，才可通过
`NHT_FMT_COMPAT_BASELINE_REF` 指定；该参数不支持任意 Git ref。例如：

```sh
NHT_FMT_COMPAT_BASELINE_REF=release \
  python3 tests/NH/run_nh.py --case NHT_FMT_COMPAT
```

`NHT_FMT_COMPAT` 对共同支持的格式和 512 B 长消息检查预期输出，并确认两种实现都发生
分段写入；`%ld`、`%lu`、`%hd`、`%#x`、`%f` 按已知的基线/当前差异分别验收。返回值
差异只记录为兼容性观察，不作为失败条件。解析出的基线完整 SHA 保存到
`artifacts/baseline_commit.txt`。

`NHO_LEGACY_CONFIG` 只检查已废弃的 `HARD_FPU_ENABLE` 必须在编译期被拒绝，并给出迁移到
`RTT_FLOAT_USE_MODFF` 的明确诊断。它同样不属于 `--all`、发布门禁或常规 CI，必须显式运行：

```sh
python3 tests/NH/run_nh.py --case NHO_LEGACY_CONFIG
```

`NHT_FLASH_OPT` 集中执行本次 Flash 优化的两类临时资源比较：在 Cortex-M0、`-Os` 和
`--gc-sections` 的相同构建条件下生成 float fast-on/off 最终 ELF，并要求 fast-off Flash
严格小于 fast-on；在 M0、M3、M4F 和 M7 上各执行三次 current/default 与兼容 release/default
构建，要求 ELF 可复现，当前 Flash 不超过 release 加 1024 B且静态 RAM 不超过 release。
默认基线为 Git `release` 分支；可使用 `NHT_FLASH_OPT_BASELINE_REF` 指定包含旧版
`RTT/SEGGER_RTT_printf.c` 的兼容基线，解析后的完整 SHA 会写入证据。

该项是当前 Flash 优化的短期确认，完成对应优化验证后退役并删除；它不属于 `--all`、发布
门禁或常规 CI，必须显式运行：

```sh
python3 tests/NH/run_nh.py --case NHT_FLASH_OPT
```

默认在首个失败处停止。添加 `--keep-going` 可继续运行其余已选择的用例。使用 `--dry-run` 可以查看用例选择结果和最终生效的路径，而不检查工具、不创建证据目录，也不开始构建：

```sh
python3 tests/NH/run_nh.py --all --dry-run
python3 tests/NH/run_nh.py --ci --dry-run
```

常用的一次性覆盖选项包括：

```sh
python3 tests/NH/run_nh.py --case NH07 \
  --toolchain-prefix /absolute/path/to/arm-none-eabi- \
  --cube-cmake /absolute/path/to/cube-cmake
```

NH09 和 NH10 要求各工程处于正常集成状态。HW 测试使用纯 Make/CMake overlay，因此无论 HW 测试正常完成还是中断，都不会在 `rtt_cfg.h` 或应用源码中留下 fixture 引用。NH 预检仍会拒绝工程侧遗留的 `tests/HW/...` 注入，并兼容识别目录改名前的 `tests/hw/...` 旧注入，避免陈旧工作树产生误导性的集成测试结果。
NH09/NH10 构建前还会校验八个工程内实际使用的 `ARM_SEGGER_RTT`：每个目录都必须是
独立、干净的 Git 工作树，并与启动 runner 的主仓库 HEAD 完全一致。外层工程自身的提交或
脏状态不参与此项判定；实际库的路径、SHA 和脏状态会写入用例 metadata。

## 测试证据和工程影响

默认情况下，每次运行都会创建 `TEST_EVIDENCE/NH_RUN_YYYYMMDD_HHMMSS/`。使用 `--evidence-dir PATH` 可以指定一个新的证据目录。运行器绝不会覆盖已有证据。

```text
NH_RUN_YYYYMMDD_HHMMSS/
  SUMMARY.md
  NH01/
    metadata.json
    run.log
    result.txt
    artifacts/
```

`SUMMARY.md` 和每个用例的 `metadata.json` 都记录主仓库完整候选 SHA 与 dirty 状态。
即使依赖预检失败，用例目录仍会留下包含候选身份的 metadata、`FAIL` 结果和预检错误。

运行器、NH01-NH08、NH11-NH12、`NHO_*` 和 `NHT_*` 不会修改外部工程源码或构建文件。NH10 在临时目录中构建实际工程，因此也不会改动工程源码树。NH09 会有意验证各工程原生的 clean、增量构建和 preset 工作流；它会删除并重新生成八个工程常规的 `build/` 目录。运行 NH09 前，请提交或另行保留需要的构建目录产物。

NH09/NH10 的功能和资源构建会传入一个仅用于测试的 `SEGGER_RTT_WriteNoLock` 链接锚点。即使普通示例应用没有调用 RTT，该锚点也能防止 RTT 实现在启用 `--gc-sections` 时被裁剪，从而使测试可以观测它。锚点由测试命令或 overlay 提供，无需修改应用的 `main.c`、Makefile 或 CMake 目标定义。

NH10 在 metadata 中记录测试脚本、工具链、构建环境和证据文件的哈希，但不读取或依赖测试计划文档，也不会修改 `docs/quality/test_report.md`。正式发布时由候选提交 SHA 和最终测试报告关联本轮采用的测试计划。
formatter、typed integer、legacy float fast-off/on、Skip C/ASM 的 Flash、RAM、固定栈帧和
符号检查统一归 NH10；这些原 HW08 build-only 项目不需要开发板，NH10 会对其执行三次
干净构建、可复现性和冻结预算验收。具体配置映射和预算见
[NH10 resource usage](cases/NH10/resource_usage/README.md)。HW08 只保留必须在真实硬件上
完成的周期、吞吐和运行时栈检查。

NH10 报告库 Flash 时使用 `artifacts/resource_usage/library_footprint.csv`：每个配置使用完全相同的
benchmark 对象分别执行控制链接和 RTT 完整链接，两者 Flash 差值是库链接占用。该口径排除
benchmark 自身，同时包含 RTT 拉入的运行库支持。`actual/measurements.csv` 中的实际工程
Flash 是完整应用镜像占用，只用于链接容量和相同工程内的 C/ASM 差异证据，不作为 RTT 库
占用报告。面向人工阅读的 `artifacts/resource_usage/flash_footprint.md` 只输出两个表格：
`default` 和 `typed_combo`。`default` 开启四级日志、print、string 和 legacy float，并由
探针调用这些入口；`typed_combo` 只开启 typed integer/pointer 和 typed float，并调用五个
typed 入口。每张表均以字节列出完整链接、配对控制和两者之差。完整的所有 profile 数据仍
保留在 CSV 中。正式 NH/HW 证据验收完成后，以该 Markdown 摘要填写
`docs/quality/test_report.md` 中同名的两张表；runner 不自动更新最终报告。

各个 `tests/NH/cases/<CASE_ID>/run.sh` 脚本仍可用于针对性开发，但 Python 入口才是可复现测试和证据记录的统一接口。
