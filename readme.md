# 目录
- [目录](#目录)
- [描述](#描述)
- [移植](#移植)
- [日志配置](#日志配置)
  - [推荐配置](#推荐配置)
  - [自定义配置](#自定义配置)
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

日志功能由以下配置项控制：

1. `RTT_LOG_ENABLE` 是总开关。设置为 `0` 时所有 `log_*` 宏均不输出，
   且宏参数不会被求值。可在发布固件中配置为 `0`
2. `LOG_ENABLE_LITE` 是等级日志的轻量模式开关。设置为 `1` 时，
   `log_info`、`log_debug`、`log_warn` 和 `log_err` 只输出正文和换行，
   不输出颜色及等级前缀；该开关不影响 `log_print`、`log_string` 和浮点日志。
3. `LOG_ENABLE_INFO`、`LOG_ENABLE_DEBUG`、`LOG_ENABLE_WARN`、
   `LOG_ENABLE_ERROR`、`LOG_ENABLE_PRINT`、`LOG_ENABLE_STRING` 和
   `LOG_ENABLE_FLOAT` 分别控制各类日志，在完整模式和轻量模式下都独立生效。

工程默认开启 `RTT_LOG_ENABLE`、关闭 `LOG_ENABLE_LITE`，并开启所有单项日志。
日志默认写入 RTT Up Buffer 0，完整模式下启用 ANSI 颜色。
`HARD_FPU_ENABLE` 默认为 `0`，浮点日志使用不依赖浮点运行库的 IEEE-754 位解析
实现。设置为 `1` 后改用 `modff` 实现；仅建议在目标和编译选项均启用硬件 FPU
时使用，GCC 工程通常还需链接 `libm`（`-lm`）。否则应保持为 `0`，以避免引入
软件浮点及数学库开销。

## 推荐配置

可以根据资源和输出需求选择以下配置方案：

| 配置方案 | 关键配置 | 行为和适用场景 |
|---|---|---|
| 完整日志 | `LOG_ENABLE_LITE=0`，所有单项日志开启 | 保留等级、颜色和全部日志 API，适合常规调试 |
| Lite 等级日志 | `LOG_ENABLE_LITE=1`，所有单项日志开启 | 等级日志只输出正文和换行，API 使用方式不变 |
| 极致精简模式 | 关闭等级及浮点日志，只开启 `LOG_ENABLE_PRINT` 和 `LOG_ENABLE_STRING` | 在保留格式化和字符串输出能力的前提下，以运行效率和 Flash 占用为最高优先级 |

资源极其紧张时，推荐使用极致精简模式：格式化内容使用 `log_print`，已有字符串
使用路径更短的 `log_string`。

```c
#define RTT_LOG_ENABLE    1

#define LOG_ENABLE_INFO   0
#define LOG_ENABLE_DEBUG  0
#define LOG_ENABLE_WARN   0
#define LOG_ENABLE_ERROR  0
#define LOG_ENABLE_FLOAT  0

#define LOG_ENABLE_PRINT  1
#define LOG_ENABLE_STRING 1
```

该配置不使用等级日志，因此 `LOG_ENABLE_LITE` 设置为 `0` 或 `1` 均不影响输出。
如果完全不需要格式化，可进一步只开启 `LOG_ENABLE_STRING`；如果只需要格式化，
则只开启 `LOG_ENABLE_PRINT`。

## 自定义配置

以上默认值由各模块内置，无需修改 `rtt_cfg.h` 即可生效。如果希望修改配置，
需要在应用工程中启用 `rtt_cfg.h` 的自定义配置块：

- `RTT_USER_CFG_ENABLE=0`：自定义配置块不生效，继续使用各模块的内置默认值。
- `RTT_USER_CFG_ENABLE=1`：自定义配置块生效，块内配置覆盖各模块的内置默认值。

使用 submodule 集成时，推荐把配置模板复制到主工程根目录：

```shell
cp ARM_SEGGER_RTT/rtt_cfg.h ./rtt_cfg.h
```

复制后，将主工程 `rtt_cfg.h` 中的 `RTT_USER_CFG_ENABLE` 设置为 `1`，再修改
需要调整的配置。例如：

```c
#define RTT_USER_CFG_ENABLE 1

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
集成已经保证这一点。若使用命令行 `-D` 临时配置，应保持
`RTT_USER_CFG_ENABLE=0`；启用自定义配置块后，请不要再通过命令行重复定义块内
的同名宏。

# API

## API 介绍

应用代码包含 `rtt_log.h` 后，推荐使用以下日志宏：

| API | 定位 | 完整模式输出 | 配置开关 |
|---|---|---|---|
| `log_string(Text)` | 纯字符串的最短输出路径 | 原样字符串，不格式化、不自动换行 | `LOG_ENABLE_STRING` |
| `log_print(Format, ...)` | 格式化日志的极致精简路径 | 原样格式化输出，不自动换行 | `LOG_ENABLE_PRINT` |
| `log_info(Format, ...)` | 一般运行信息 | 亮绿色 `[INFO] ` + 正文 + 换行 | `LOG_ENABLE_INFO` |
| `log_debug(Format, ...)` | 调试信息 | 亮蓝色 `[DEBUG] ` + 正文 + 换行 | `LOG_ENABLE_DEBUG` |
| `log_warn(Format, ...)` | 警告信息 | 亮黄色 `[WARN] ` + 正文 + 换行 | `LOG_ENABLE_WARN` |
| `log_err(Format, ...)` | 错误信息 | 亮红色 `[ERROR] ` + 正文 + 换行 | `LOG_ENABLE_ERROR` |
| `log_float(Value)` | 无标签浮点数 | 无颜色，数值 + 换行 | `LOG_ENABLE_FLOAT` |
| `log_float_label(Label, Value)` | 带标签浮点数 | 无颜色，`Label: Value` + 换行 | `LOG_ENABLE_FLOAT` |

资源极其紧张时，`log_string` 和 `log_print` 构成极致精简输出路径：
- `log_string` 不进入格式化器，直接输出已有字符串，是无需格式化时运行路径最短、
  Flash 附加占用最低的选择。
- `log_print` 直接调用 RTT 格式化器，不经过日志等级、颜色、前缀和自动换行处理，
  是仍需格式化参数时兼顾运行效率与 Flash 占用的极致精简选择。

两者分别覆盖纯字符串和格式化输出。只启用 `LOG_ENABLE_STRING` 与
`LOG_ENABLE_PRINT`，并关闭等级及浮点日志，即为前文所述的极致精简模式。需要
日志等级和颜色时使用普通日志，需要固定三位小数输出时使用浮点日志。


### 输出规则

- 表中的颜色仅在完整模式且 `RTT_LOG_USE_COLOR=1` 时生效。
- `LOG_ENABLE_LITE=1` 时，等级日志不输出颜色和等级前缀，只输出正文。
- 各 API 的配置开关相互独立；关闭后，对应宏的参数也不会被求值。
- 关闭 `LOG_ENABLE_STRING` 后，`log_string` 返回 0。

### 使用示例

```c
#include "rtt_log.h"

log_info("system ready");
log_debug("counter=%u", 42u);
log_warn("voltage=%u mV", 3250u);
log_err("status=%d", -1);
log_print("plain text\n");
log_string("raw text\n");
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
raw text
1.250
temperature: -2.500
```

### 浮点日志

| 项目 | 说明 |
|---|---|
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

官方 RTT 对部分长度修饰符的处理主要停留在格式串解析层面：解析器会跳过
`h`、`l` 等字符，但底层数值格式化仍按固定的基础整数类型处理，并未完整
实现对应的类型宽度，尤其不能视为对 `long long` 的真正支持。精简格式化器
因此不保留这类仅具表面兼容性的解析逻辑，以避免增加固件代码体积，同时使
实际支持范围与实现能力保持一致。

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
|2.1.0|2026/08/19|新增 `RTT_USER_CFG_ENABLE` 自定义配置启用方式；补充完整日志、Lite 等级日志和极致精简模式；强化 `log_print`、`log_string` 的效率与 Flash 定位，并修正 `HARD_FPU_ENABLE` 使用说明||
|2.0.0|2026/08/13|重构 README 文档结构，将移植指南和更新记录拆分为独立文档；同步日志配置、API、精简格式化器及构建接入说明||
|1.1.0|2026/07/30|完善 CMake 构建、烧录和常见问题说明，修订记录与更新记录改为倒序排列||
|1.0.1|2026/04/07|修改了移植描述，[位于移植/Make/2.](port.md#make)||
|1.0.0|2026/03/27|更改了文档的结构||

# 更新记录

项目的版本更新内容和历史变更请查看 [CHANGELOG.md](CHANGELOG.md)。
