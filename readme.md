# 目录
- [目录](#目录)
- [描述](#描述)
- [移植](#移植)
- [日志配置](#日志配置)
- [API](#api)
  - [API 介绍](#api-介绍)
    - [输出规则](#输出规则)
    - [使用示例](#使用示例)
    - [浮点日志](#浮点日志)
    - [底层接口](#底层接口)
  - [格式化支持](#格式化支持)
    - [格式控制](#格式控制)
    - [不支持的格式](#不支持的格式)
    - [长消息与错误格式](#长消息与错误格式)
- [修订记录](#修订记录)
- [更新记录](#更新记录)

---

# 描述
1. 本仓库基于 SEGGER RTT 8.64a，用于在 Cortex-M 目标上输出和查看调试日志。
2. 提供 J-Link 烧录、擦除脚本，位于 `ARM_SEGGER_RTT/jlinkscript`。
3. 支持 STM32CubeMX 生成的 Make 工程，移植方式见 [Make](port.md#make)。
4. 支持 STM32CubeMX 生成的 CMake 工程，以及 STM32 VS Code 插件提供的
   `cube-cmake`，移植方式见 [CMake](port.md#cmake)。
5. 建议将本仓库作为 Git submodule 引入主工程。

---

# 移植

Make、CMake 工程的接入方法，以及配置、编译、烧录和常见问题，请查看
[移植指南](port.md)。

---

# 日志配置

日志配置分为三级：

1. `RTT_LOG_ENABLE` 是总开关。设置为 `0` 时所有 `log_*` 宏均不输出，
   且宏参数不会被求值。可在发布固件中配置为 `0`
2. `LOG_ENABLE_LITE` 是轻量模式开关。设置为 `1` 时，已启用的日志只
   输出正文和换行，不输出颜色及等级前缀。建议在资源紧张的MCU配置此选项
3. `LOG_ENABLE_INFO`、`LOG_ENABLE_DEBUG`、`LOG_ENABLE_WARN`、
   `LOG_ENABLE_ERROR`、`LOG_ENABLE_PRINT` 和 `LOG_ENABLE_FLOAT` 分别控制
   各类日志，在完整模式和轻量模式下都独立生效。

默认配置为开启总开关、关闭轻量模式，并开启所有单项日志。默认使用
RTT Up Buffer 0 并启用 ANSI 颜色。`HARD_FPU_ENABLE` 默认为 `0`；仅当目标
和编译选项均启用硬件 FPU 时，才应在应用的 `rtt_cfg.h` 中将其设置为 `1`。

使用 submodule 集成时，推荐把配置模板复制到主工程根目录：

```shell
cp ARM_SEGGER_RTT/rtt_cfg.h ./rtt_cfg.h
```

然后在主工程的 `rtt_cfg.h` 中取消所需配置项的注释并修改。例如：

```c
#define RTT_LOG_ENABLE       1
#define LOG_ENABLE_LITE      1
#define LOG_ENABLE_DEBUG     0
#define HARD_FPU_ENABLE      1
#define RTT_LOG_BUFFER_INDEX 0
#define RTT_LOG_USE_COLOR    0

#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   3
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 3
#define BUFFER_SIZE_UP                  1024
#define BUFFER_SIZE_DOWN                16
#define SEGGER_RTT_PRINTF_BUFFER_SIZE   64u
```

CMake 和 `segger_rtt.mk` 都会优先搜索主工程根目录，因此该文件可以覆盖
日志配置以及 `SEGGER_RTT_Conf.h` 中的 RTT 通道、传输缓冲区和格式化栈缓冲
默认值，无需修改或提交子模块内容。`BUFFER_SIZE_UP` 和 `BUFFER_SIZE_DOWN`
占用静态 RAM；`SEGGER_RTT_PRINTF_BUFFER_SIZE` 是每次格式化调用的栈缓冲区。
以上数值与 SEGGER RTT 源码默认值一致。上行环形缓冲区会保留一个字节用于
区分空和满，因此默认 1024 B 缓冲区最多保存 1023 B 尚未被主机读取的数据。
缩小缓冲区会增加突发日志被丢弃的概率。

修改这些底层配置后必须清理全部 RTT 对象并重新编译，以保证应用代码和
`SEGGER_RTT.c` 看到一致的控制块布局：

Make:
```shell
make clean
make -j
```

CMake:
```shell
make preset_debug && make d
```

CMake 工程
如需把配置放在其他目录，可以在配置阶段指定：

```cmake
set(ARM_SEGGER_RTT_CONFIG_DIR "${CMAKE_SOURCE_DIR}/config" CACHE PATH "" FORCE)
add_subdirectory(ARM_SEGGER_RTT)
```

Make 工程可以在包含 `segger_rtt.mk` 前指定其他配置目录：

```make
RTT_CONFIG_DIR := config
include ARM_SEGGER_RTT/segger_rtt.mk
```

RTT 的 C 源码默认单独使用 `-Os`，主工程的优化等级不受影响。Make 工程可在
包含 `segger_rtt.mk` 前覆盖优化选项；设为空值表示继承主工程的 `CFLAGS`：

```make
ARM_SEGGER_RTT_OPTIMIZATION := -Og
include ARM_SEGGER_RTT/segger_rtt.mk
```

CMake 工程可在 `add_subdirectory` 前覆盖同名缓存变量：

```cmake
set(ARM_SEGGER_RTT_OPTIMIZATION "-Og" CACHE STRING "" FORCE)
add_subdirectory(ARM_SEGGER_RTT)
```

修改优化选项后必须清理旧对象再重新构建。GCC 和 Clang 同时收到项目优化
选项和 RTT 专用选项时，以编译命令中靠后的 RTT 专用选项为准。

应用源码与 `rtt_log.c` 必须使用同一个配置目录，仓库提供的 CMake 和 Make
集成已经保证这一点。若使用命令行 `-D` 临时配置，请不要在项目级
`rtt_cfg.h` 中重复定义同一个宏。

# API

## API 介绍

应用代码包含 `rtt_log.h` 后，推荐使用以下日志宏：

| API | 用途 | 完整模式输出 | 配置开关 |
|---|---|---|---|
| `log_info(Format, ...)` | 一般运行信息 | 亮绿色 `[INFO] ` + 正文 + 换行 | `LOG_ENABLE_INFO` |
| `log_debug(Format, ...)` | 调试信息 | 亮蓝色 `[DEBUG] ` + 正文 + 换行 | `LOG_ENABLE_DEBUG` |
| `log_warn(Format, ...)` | 警告信息 | 亮黄色 `[WARN] ` + 正文 + 换行 | `LOG_ENABLE_WARN` |
| `log_err(Format, ...)` | 错误信息 | 亮红色 `[ERROR] ` + 正文 + 换行 | `LOG_ENABLE_ERROR` |
| `log_print(Format, ...)` | 无等级的普通文本 | 无颜色，正文 + 换行 | `LOG_ENABLE_PRINT` |
| `log_float(Value)` | 输出单精度浮点数 | 无颜色，数值 + 换行 | `LOG_ENABLE_FLOAT` |
| `log_float_label(Label, Value)` | 输出带标签的单精度浮点数 | 无颜色，`Label: Value` + 换行 | `LOG_ENABLE_FLOAT` |

### 输出规则

- 表中的颜色仅在完整模式且 `RTT_LOG_USE_COLOR=1` 时生效。
- `LOG_ENABLE_LITE=1` 时，等级日志不输出颜色和等级前缀，只输出正文。
- 所有日志宏都会自动追加换行，格式字符串中通常不需要再写 `\n`。
- 各 API 的配置开关相互独立；关闭后，对应宏的参数也不会被求值。

### 使用示例

```c
#include "rtt_log.h"

log_info("system ready");
log_debug("counter=%u", 42u);
log_warn("voltage=%u mV", 3250u);
log_err("status=%d", -1);
log_print("plain text");
log_float(1.25f);
log_float_label("temperature", -2.5f);
```

忽略 ANSI 颜色控制字符后，默认完整模式的输出内容为：

```text
[INFO] system ready
[DEBUG] counter=42
[WARN] voltage=3250 mV
[ERROR] status=-1
plain text
1.250
temperature: -2.500
```

### 浮点日志

| 项目 | 说明 |
|---|---|
| `log_float(Value)` | 只输出数值 |
| `log_float_label(Label, Value)` | 先输出字符串标签，再输出数值 |
| `Label` | 类型为 `const char *`；传入 `NULL` 等同于 `log_float(Value)` |
| 数值精度 | 参数转换为 `float`，固定输出三位小数 |
| 舍入方式 | 直接截断，不四舍五入；`1.9996f` 输出 `1.999` |
| 特殊值 | 输出 `NaN`、`Inf`、`-Inf`、`Overflow` 或 `-Overflow` |
| 可表示范围 | 绝对值必须小于 `2^32`，否则输出溢出提示 |

默认使用不依赖 `modff` 或软浮点除法的 IEEE-754 binary32 解析路径。
设置 `HARD_FPU_ENABLE=1` 后改用 `modff`；GCC 工程最终链接时通常需要 `-lm`。

### 底层接口

一般应用代码应优先使用日志宏。只有需要返回值、指定 RTT 通道或动态选择
日志等级时，才需要直接调用底层接口：

```c
int Count;

Count = RTT_LogPrintf(RTT_LOG_LEVEL_INFO, "state=%u", 3u);
Count = SEGGER_RTT_printf(RTT_LOG_BUFFER_INDEX, "raw=%u", 3u);
RTT_LogFloat3(1.25f, "voltage");
```

| 接口 | 行为 | 返回值 |
|---|---|---|
| `RTT_LogPrintf(Level, Format, ...)` | 添加等级前缀和换行；`RTT_LOG_LEVEL_PRINT` 不添加等级前缀 | 成功时返回字符数；写入失败返回 `-1`；模块关闭返回 `0` |
| `SEGGER_RTT_printf(BufferIndex, Format, ...)` | 写入指定 Up Buffer，不添加前缀、颜色或换行 | 成功时返回字符数；写入失败返回 `-1` |
| `RTT_LogFloat3(Value, Description)` | `Description` 非空时输出 `Description: Value`，否则只输出数值 | 无返回值 |

## 格式化支持

`log_info`、`log_debug`、`log_warn`、`log_err`、`log_print` 和
`SEGGER_RTT_printf` 使用面向嵌入式场景的精简格式化器，不是完整的标准
`printf` 实现。当前支持范围如下：

| 格式 | 参数类型 | 输出行为 |
|---|---|---|
| `%c` | `int` | 输出单个字符 |
| `%d` | `int` | 输出有符号十进制整数 |
| `%u` | `unsigned int` | 输出无符号十进制整数 |
| `%x`、`%X` | `unsigned int` | 输出大写十六进制整数，不添加 `0x` 前缀 |
| `%s` | `const char *` | 输出字符串；空指针输出 `(NULL)` |
| `%p` | `void *` | 输出固定为指针位宽的大写十六进制值，不添加 `0x` 前缀 |
| `%%` | 无 | 输出 `%` |

### 格式控制

| 功能 | 支持范围 | 示例 |
|---|---|---|
| 左对齐 | `-` | `%-5d` |
| 数字补零 | `0` | `%08x` |
| 正数符号 | `+` | `%+d` |
| 字段宽度 | 仅支持格式串中的数字宽度 | `%8d` |
| 整数精度 | 仅支持格式串中的数字精度 | `%.4d` |
| 字符串精度 | 支持数字精度和动态精度 `.*` | `%.3s`、`%.*s` |

动态字符串精度为负数时视为未指定精度。

### 不支持的格式

| 类别 | 不支持内容 |
|---|---|
| 标志 | `#`、空格标志 |
| 动态字段宽度 | `*`，例如 `%*d` |
| 长度修饰符 | `h`、`l`、`ll`、`z` 等 |
| 浮点转换符 | `%f`、`%e`、`%g` 等 |

浮点值应使用 `log_float(Value)` 或 `log_float_label(Label, Value)`。

### 长消息与错误格式

- 长消息通过 `SEGGER_RTT_PRINTF_BUFFER_SIZE` 缓冲区分块写入，该配置不限制
  单条日志的总长度。
- 未支持或无法识别的转换会按原文本输出，不会消费对应的转换值参数。
- 如果错误格式中包含动态精度 `.*`，精度参数仍会先被消费。
- 编译器的 `printf` 格式检查只能发现参数类型问题，不代表本实现支持标准
  `printf` 的全部功能。

# 修订记录
| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|
|2.0.0|2026/08/13|重构 README 文档结构，将移植指南和更新记录拆分为独立文档；同步日志配置、API、精简格式化器及构建接入说明||
|1.1.0|2026/07/30|完善 CMake 构建、烧录和常见问题说明，修订记录与更新记录改为倒序排列||
|1.0.1|2026/04/07|修改了移植描述，[位于移植/Make/2.](port.md#make)||
|1.0.0|2026/03/27|更改了文档的结构||

# 更新记录

项目的版本更新内容和历史变更请查看 [CHANGELOG.md](CHANGELOG.md)。
