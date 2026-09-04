# RTT 日志模式

本文从使用者角度说明本库的四种主要日志配置方案：Full、Lite、Typed 和 Float，并给出可直接
用于 `rtt_cfg.h` 的配置示例。完整接口参数、格式化支持和边界行为请查看
[API 指南](API%20declaration.md)；Make、CMake 接入方式请查看[移植指南](port.md)。

# 目录

- [RTT 日志模式](#rtt-日志模式)
- [目录](#目录)
- [模式概览](#模式概览)
  - [Full 模式](#full-模式)
  - [Lite 模式](#lite-模式)
  - [Typed 模式](#typed-模式)
  - [Float 模式](#float-模式)
  - [四种模式的关系](#四种模式的关系)
- [模式选择](#模式选择)
  - [选择策略](#选择策略)
  - [模式对比](#模式对比)
  - [Float 子类如何选择](#float-子类如何选择)
- [配置前准备](#配置前准备)
  - [复制配置模板](#复制配置模板)
  - [启用自定义配置](#启用自定义配置)
  - [配置生效规则](#配置生效规则)
  - [修改配置后重新构建](#修改配置后重新构建)
- [Full 模式详细说明](#full-模式详细说明)
  - [Full 输出行为](#full-输出行为)
  - [Full 适用场景](#full-适用场景)
  - [Full 关闭颜色](#full-关闭颜色)
  - [Full 关闭未使用的日志接口](#full-关闭未使用的日志接口)
  - [Full Skip 汇编写入优化](#full-skip-汇编写入优化)
- [Lite 模式详细说明](#lite-模式详细说明)
  - [Lite 输出行为](#lite-输出行为)
  - [Lite 适用场景](#lite-适用场景)
  - [Lite 完整配置示例](#lite-完整配置示例)
  - [Lite 关闭未使用的日志接口](#lite-关闭未使用的日志接口)
  - [Lite Skip 汇编写入优化](#lite-skip-汇编写入优化)
- [Typed 模式详细说明](#typed-模式详细说明)
  - [Typed 输出行为](#typed-输出行为)
  - [支持的固定类型](#支持的固定类型)
  - [Typed 适用场景](#typed-适用场景)
  - [Typed-only 完整配置示例](#typed-only-完整配置示例)
  - [为什么不建议 Typed 与普通日志混合使用](#为什么不建议-typed-与普通日志混合使用)
  - [裁掉通用 formatter](#裁掉通用-formatter)
  - [Typed Skip 汇编写入优化](#typed-skip-汇编写入优化)
- [Float 模式详细说明](#float-模式详细说明)
  - [普通 Float](#普通-float)
  - [Typed Float](#typed-float)
  - [Typed Float 完整配置示例](#typed-float-完整配置示例)
  - [Float 的 Flash 与效率优化](#float-的-flash-与效率优化)
- [公共优化项](#公共优化项)
  - [关闭未使用的接口](#关闭未使用的接口)
  - [Skip 汇编写入的适用条件](#skip-汇编写入的适用条件)
  - [RTT 编译优化等级](#rtt-编译优化等级)
  - [缓冲区容量与丢失概率](#缓冲区容量与丢失概率)
  - [modff 备用实现](#modff-备用实现)
- [配置方案汇总](#配置方案汇总)
- [注意事项](#注意事项)
  - [宏参数求值规则](#宏参数求值规则)
  - [配置目录必须保持一致](#配置目录必须保持一致)
  - [命令行宏与配置文件不要重复定义](#命令行宏与配置文件不要重复定义)
  - [底层配置变更必须清理旧对象](#底层配置变更必须清理旧对象)
- [相关文档](#相关文档)

---

# 模式概览

## Full 模式

Full 是默认模式。`log_info`、`log_debug`、`log_warn` 和 `log_err` 会通过通用formatter 格式化正文，并添加等级前缀、可选 ANSI 颜色和换行。

Full 优先保证调试信息的辨识度。没有明确的 Flash 或实时性限制时，应首先使用该模式。它保留通用 formatter、等级前缀以及可选颜色，在三种非浮点方案中通常比 Lite 和Typed-only 占用更多 Flash；但应用已从其他位置链接 formatter 时，新增差值可能缩小。

## Lite 模式

Lite 继续使用与 Full 相同的四个等级日志 API 和通用 formatter，但去掉等级前缀及ANSI 颜色，只输出正文和换行。已有日志调用不需要修改。

Lite 适合希望保留 `printf` 风格格式化能力，同时减少输出装饰、传输字节和部分附加代码的工程。其 Flash 通常低于 Full，但由于通用 formatter 仍然存在，节省幅度通常小于改用Typed；Lite 的主要收益也包括减少 RTT 传输字节，而不只是缩小固件。

## Typed 模式

Typed 提供整数及指针的固定类型直写接口，不解析格式串，也不经过通用 formatter，并在固定缓冲区中完成转换后写入 RTT。

Typed 适合日志内容类型固定、MCU 资源紧张，或者希望明确控制格式化开销和写入路径的工程。当等级日志、`log_print` 和普通 Float 全部关闭时，通用 formatter 可从最终链接中移除；对于固定整数和指针日志，Typed-only 通常是 Flash 最小的方案。

## Float 模式

Float 是专门输出单精度浮点数的第四种方案，独立于上述三种模式,并分为两个等级：

- 普通 Float：使用 `log_float`、`log_float_label`，支持无标签或不受 46 B 限制的标签，并保留特殊值和超限情况的兼容路径。
- Typed Float：使用 `log_f32`，采用固定格式和固定 64 B 帧，标签最多 46 B，不经过通用formatter。

只需要浮点日志时，Typed Float 通常比普通 Float 更利于控制 Flash；普通 Float 的接口和兼容路径更灵活，通常有更高的固定代码成本。两者的实际差值仍受工具链和已有链接内容影响。

## 四种模式的关系

Full 和 Lite 是同一套等级日志 API 的两种互斥输出形式，由 `LOG_ENABLE_LITE` 选择。
Typed 由 `LOG_ENABLE_TYPED` 独立控制；从功能上可以与 Full 或 Lite 同时启用，但原则上不建议这样配置。

普通 Float 由 `LOG_ENABLE_FLOAT` 独立控制；Typed Float 由 `LOG_ENABLE_TYPED_FLOAT`独立控制，而且不要求同时开启 `LOG_ENABLE_TYPED`。因此“第四种 Float 方案”是使用角度的分类，不表示它与前三种配置在预处理阶段互斥；技术上可以组合，但不应把组合配置当作节省Flash 的方案。

为了让配置目标清晰，本文把 Typed 重点描述为 Typed-only 方案，即关闭等级日志、`log_print`、`log_string` 和普通 Float，只保留实际需要的固定类型接口。

本文给出的四套完整示例默认彼此独立。混合开启虽然能够增加功能，但会同时保留多套接口和实现路径；只要仍有任一通用格式化入口，formatter 就可能留在最终固件中。这样不仅无法获得 Typed 或 Typed Float 的主要空间收益，最终 Flash 还可能高于单独使用任一方案，因此原则上不建议混合配置。

# 模式选择

## 选择策略

Flash 预算是选择方案的首要条件，建议按以下顺序判断：

1. Flash 很紧：固定整数或指针选择 Typed-only；只输出浮点数时优先评估 Typed Float。
2. Flash 有限，但现有代码必须保留等级日志和格式字符串：选择 Lite，并关闭未使用的等级、`log_print`、`log_string` 和 Float 接口。Lite 仍保留 formatter，不能期待与 Typed 相同的Flash 降幅。
3. Flash 充足，优先考虑调试可读性和格式灵活性：选择 Full。
4. 浮点接口需要完整标签或兼容回退路径：选择普通 Float；能接受固定三位小数和 46 B 标签上限时，选择 Flash 更可控的 Typed Float。
5. 同时需要说明性日志和高频固定数据：优先重新评估接口需求并选定一种主方案。不应考虑混合配置；它通常不省 Flash，甚至可能因同时保留多套路径而占用更多 Flash。

上述顺序是定性趋势，不是固定字节数。最终 Flash 取决于目标内核、编译器版本、优化等级、调用数量、链接时优化，以及应用其他模块是否已经引用 formatter 或浮点转换代码。应使用同一工具链和同一业务调用集，比较最终链接后的固件，而不是只比较单个对象文件。

## 模式对比

| 项目 | Full | Lite | Typed-only |
|---|---|---|---|
| 默认启用 | 是 | 否 | 否 |
| 主要接口 | `log_info/debug/warn/err` | `log_info/debug/warn/err` | `log_i32/u32/hex32/pointer` |
| 通用 formatter | 使用 | 使用 | 不使用 |
| 格式灵活性 | 高 | 高 | 固定类型 |
| Flash 趋势 | 通常高于 Lite/Typed | 通常低于 Full、高于 Typed | 固定整数/指针通常最低 |
| 主要目标 | 调试可读性 | 低侵入精简 | 固定类型与空间可控 |

| 项目 | 普通 Float | Typed Float |
|---|---|---|
|默认启用| 是 | 否 |
|主要接口| `log_float/log_float_label` | `log_f32` |
| 通用 formatter |  保留兼容格式化路径 | 不使用 |
| 格式灵活性 |  固定三位小数、标签灵活 | 固定三位小数、标签限 46 B |
|Flash 趋势| 通常高于 Typed Float | 浮点方案中通常更低、更可控 |
|主要目标| 浮点兼容性 | 浮点空间与固定路径可控 |

表中的 Flash 趋势以各方案单独启用、其他入口关闭为前提。Full、Lite、Typed 之间以及两种
Float 之间可以分别比较，但不能据此断言等级日志方案与 Float 方案的固定交叉排序。

## Float 子类如何选择

普通 Float 和 Typed Float 是两套独立接口：

| 需求 | 推荐接口 | 配置开关 |
|---|---|---|
| 无标签或完整标签，且需要兼容回退路径 | `log_float`、`log_float_label` | `LOG_ENABLE_FLOAT` |
| 固定格式、固定帧、可接受 46 B 标签上限 | `log_f32` | `LOG_ENABLE_TYPED_FLOAT` |

`RTT_LOG_FLOAT_FAST_PATH`只影响普通 Float 的正常有限值；NaN、Inf、溢出及快速路径无法容纳的标签仍走兼容路径。

`RTT_LOG_FLOAT_FAST_PATH = 1`时,标签长度限制同样为46B。 
Typed Float 始终使用固定帧直写，不依赖 `RTT_LOG_FLOAT_FAST_PATH`，也不依赖`LOG_ENABLE_TYPED`。

# 配置前准备

## 复制配置模板

使用 submodule 集成时，在主工程根目录执行：

```shell
cp ARM_SEGGER_RTT/rtt_cfg.h ./rtt_cfg.h
```

Make 和 CMake 集成默认优先搜索主工程根目录，因此应用可以维护自己的配置，而不需要修改或提交子模块内容。

## 启用自定义配置

模板中的自定义配置块默认关闭。要让块内配置生效，首先设置：

```c
#define RTT_USER_CFG_ENABLE 1
```

`RTT_USER_CFG_ENABLE=0` 时，块内宏不会生效，各模块继续使用内置默认值。`RTT_USER_CFG_ENABLE=1` 时，块内宏覆盖内置默认值。

本文的“完整配置示例”指某个模式相关宏的完整取值，不是独立的完整头文件。请在复制后的 `rtt_cfg.h` 中修改已有同名定义，不要把代码块追加到文件末尾，否则会造成宏重复定义。通道数量和缓冲区等未在模式代码块中列出的底层项，继续使用模板默认值，除非按后文明确调整。

## 配置生效规则

所有日志功能都受总开关 `RTT_LOG_ENABLE` 控制。关闭总开关后，公开日志宏不输出，参数也不会被求值。

各 `LOG_ENABLE_*` 开关相互独立。模式配置代码应显式设置所有相关接口，避免依赖默认值后难以判断最终固件包含了哪些路径。

`RTT_LOG_BUFFER_INDEX` 必须小于 `SEGGER_RTT_MAX_NUM_UP_BUFFERS`。默认日志通道为Up Buffer 0。

## 修改配置后重新构建

修改日志开关、通道数量、缓冲区大小或汇编分发后，应清理旧 RTT 对象再重新构建，确保应用源码和 RTT 源码看到一致配置。

Make：

```shell
make clean
make -j
```

CMake：

```shell
make preset_debug && make d
make preset_release && make r
```

# Full 模式详细说明

## Full 输出行为

Full 模式下，四个等级接口输出等级前缀、正文和换行。启用颜色时，还会在前缀和末尾加入ANSI 控制序列。

```c
log_info("system ready");
log_debug("counter=%u", 42u);
log_warn("voltage=%u mV", 3250u);
log_err("status=%d", -1);
```

忽略 ANSI 控制字符后输出为：

```text
[INFO] system ready
[DEBUG] counter=42
[WARN] voltage=3250 mV
[ERROR] status=-1
```

`log_print` 和 `log_string` 是独立基础接口，不添加等级前缀或自动换行。

## Full 适用场景

- 常规开发、联调和故障定位。
- 需要通过颜色和等级快速扫描日志。
- 日志格式变化较多，需要 `printf` 风格格式化。
- Flash 和实时性预算允许保留通用 formatter。

## Full 关闭颜色

不需要终端颜色时设置：

```c
#define RTT_LOG_USE_COLOR 0
```

这会保留 `[INFO]`、`[DEBUG]`、`[WARN]` 和 `[ERROR]` 前缀，只移除 ANSI 控制序列。它主要减少输出字节并避免不支持 ANSI 的终端出现乱码，不应视为通用性能加速开关。

## Full 关闭未使用的日志接口

每个等级拥有独立开关,可根据实际需求选择,例如关闭info和debug：

```c
#define LOG_ENABLE_INFO  0
#define LOG_ENABLE_DEBUG 0
#define LOG_ENABLE_WARN  1
#define LOG_ENABLE_ERROR 1
```

关闭接口后，对应宏参数不会求值，相关实现也更容易被预处理和链接裁剪。

## Full Skip 汇编写入优化

高频日志且目标支持 ARMv7-M 汇编实现时，可以测试：

```c
#define RTT_WRITE_SKIP_USE_ASM 1
```

该开关优化底层 Skip 写入路径，并非 Full 专属。启用条件和限制见[Skip 汇编写入的适用条件](#skip-汇编写入的适用条件)。

# Lite 模式详细说明

## Lite 输出行为

Lite 保留四个等级日志的调用方式和格式化能力，但统一只输出正文与换行：

```c
log_info("system ready");
log_err("status=%d", -1);
```

输出为：

```text
system ready
status=-1
```

Lite 不输出等级前缀或 ANSI 颜色。`RTT_LOG_USE_COLOR` 的取值不会改变 Lite 输出。

## Lite 适用场景

- 现有代码已经大量使用等级日志，不希望修改调用点。
- 仍需要格式字符串和可变参数。
- 不需要从输出文本中区分 INFO、DEBUG、WARN 和 ERROR。
- 希望减少前缀、颜色、传输字节及相关处理代码。

Lite 仍使用通用 formatter。如果目标是完全移除 formatter，应使用 Typed-only。

## Lite 完整配置示例

`RTT_LOG_USE_COLOR=0` 用于表达 Lite 配置意图；Lite 本身不会输出颜色。

## Lite 关闭未使用的日志接口

Lite 仍保留每个等级的独立开关。例如只保留警告与错误：

```c
#define LOG_ENABLE_INFO  0
#define LOG_ENABLE_DEBUG 0
#define LOG_ENABLE_WARN  1
#define LOG_ENABLE_ERROR 1
```

关闭不需要的等级通常比仅切换到 Lite 更有效，因为对应调用和实现路径可以被裁剪。

## Lite Skip 汇编写入优化

Lite 的等级日志最终仍写入 RTT，因此也可以测试：

```c
#define RTT_WRITE_SKIP_USE_ASM 1
```

该开关只在目标、编译器和缓冲区模式满足条件时生效。

# Typed 模式详细说明

## Typed 输出行为

Typed 接口使用固定类型和固定格式，不解析格式字符串，不添加等级或颜色，并自动追加换行。标签为 `NULL` 或空字符串时只输出数值；非空标签与数值之间固定使用 `": "`。

```c
log_i32("offset", -12);
log_u32("counter", 4294967295u);
log_hex32("status", 0x89ABCDEFu);
log_pointer("buffer", Buffer);
```

典型输出：

```text
offset: -12
counter: 4294967295
status: 0x89ABCDEF
buffer: 0x20000000
```

Typed 标签最多输出前 46 B，超出部分直接截断且不添加省略号。

## 支持的固定类型

| API | 类型与输出 |
|---|---|
| `log_i32` | `int32_t`，有符号十进制 |
| `log_u32` | `uint32_t`，无符号十进制 |
| `log_hex32` | `uint32_t`，8 位大写十六进制并添加 `0x` |
| `log_pointer` | 指针宽度的大写十六进制并添加 `0x` |

## Typed 适用场景

- 高频输出的计数器、状态码、寄存器值或地址。
- Flash、栈空间和执行时间预算严格。
- 日志格式固定，不需要运行时解析格式串。
- 希望关闭普通格式化接口，从最终固件中裁掉通用 formatter。

## Typed-only 完整配置示例

以下方案只保留整数及指针 Typed API：

```c
#define RTT_USER_CFG_ENABLE 1

#define RTT_LOG_ENABLE       1
#define LOG_ENABLE_LITE      0
#define LOG_ENABLE_TYPED     1
#define LOG_ENABLE_TYPED_FLOAT 0

#define LOG_ENABLE_INFO      0
#define LOG_ENABLE_DEBUG     0
#define LOG_ENABLE_WARN      0
#define LOG_ENABLE_ERROR     0
#define LOG_ENABLE_PRINT     0
#define LOG_ENABLE_STRING    0
#define LOG_ENABLE_FLOAT     0

#define RTT_FLOAT_USE_MODFF  0
#define RTT_LOG_FLOAT_FAST_PATH 0
#define RTT_WRITE_SKIP_USE_ASM 0

#define RTT_LOG_BUFFER_INDEX 0u
#define RTT_LOG_USE_COLOR    0
```

`LOG_ENABLE_LITE` 和 `RTT_LOG_USE_COLOR` 不改变 Typed 输出，在这里设置为 `0` 是为了让配置意图明确。

## 为什么不建议 Typed 与普通日志混合使用

Typed 在功能上可以与 Full 或 Lite 同时开启：

```c
#define LOG_ENABLE_LITE  0
#define LOG_ENABLE_TYPED 1

#define LOG_ENABLE_INFO  1
#define LOG_ENABLE_WARN  1
```

但这不是一种 Flash 优化方案。只要应用仍使用等级日志或 `log_print`，通用 formatter 就可能保留；与此同时，Typed 的固定转换和写入实现也会进入固件。两套路径并存不仅抵消了Typed-only 的主要空间收益，最终 Flash 还可能高于单独使用 Full、Lite 或 Typed。

因此原则上应根据主要日志需求选择一种方案。

## 裁掉通用 formatter

要获得 Typed-only 的主要空间收益，必须同时关闭所有通用格式化入口：

```c
#define LOG_ENABLE_INFO   0
#define LOG_ENABLE_DEBUG  0
#define LOG_ENABLE_WARN   0
#define LOG_ENABLE_ERROR  0
#define LOG_ENABLE_PRINT  0
#define LOG_ENABLE_FLOAT  0
```

`log_string` 不使用 formatter，可按需要单独保留；但最严格的 Typed-only 配置通常也会关闭它，以便明确最终只保留 Typed 输出。

## Typed Skip 汇编写入优化

Typed 最终通过 RTT 写入固定帧，因此在支持的目标上也可以测试：

```c
#define RTT_WRITE_SKIP_USE_ASM 1
```

该优化不会改变 Typed 格式、标签上限或短写行为，只改变底层 Skip 写入分发。

# Float 模式详细说明

Float 模式专门处理单精度浮点日志。普通 Float 和 Typed Float 是两个可独立开启的子类，都输出固定三位小数，但接口边界、写入路径和Flash 成本不同。若工程只需要浮点日志，应从下面两套独立配置中选择，不必同时开启 Full、Lite 或整数 Typed。

## 普通 Float

普通 Float 由 `LOG_ENABLE_FLOAT` 控制，提供：

| API | 行为 |
|---|---|
| `log_float(value)` | 输出数值并自动换行 |
| `log_float_label(label, value)` | 输出 `label: value` 并自动换行 |

普通 Float 支持 NaN、正负 Inf、超出快速路径范围的数值以及较长标签。默认配置优先采用紧凑实现；启用快速路径后，正常有限值可走固定缓冲区单次提交，无法处理的输入仍回退到兼容路径。相应的灵活性和回退代码通常使其 Flash 高于 Typed Float-only。

普通 Float 适合以下情况：

- 标签不能接受 46 B 上限。
- 需要保留特殊值和超限输入的兼容路径。
- 已有代码使用 `log_float` 或 `log_float_label`，不希望修改调用点。
- Flash 预算允许保留比 Typed Float 更完整的处理逻辑。

## Typed Float

Typed Float 由 `LOG_ENABLE_TYPED_FLOAT` 独立控制，只提供 `log_f32(label, value)`。它不要求开启 `LOG_ENABLE_TYPED`，也不依赖普通 Float。输出固定三位小数；标签为 `NULL` 或空字符串时只输出数值，否则输出 `label: value`。

`log_f32` 在固定 64 B 帧中完成转换并单次写入 RTT。标签最多输出前 46 B，超出部分直接截断且不添加省略号。它不经过通用 formatter，也没有普通 Float 的兼容回退路径，因此在只保留浮点日志的构建中，Flash 通常更低且路径更可预测。

Typed Float 适合以下情况：

- Flash 是主要限制，希望移除通用 formatter 和普通 Float 回退路径。
- 固定三位小数满足需求。
- 可以接受标签最多 46 B。
- 高频浮点日志需要固定帧和稳定的写入路径。

## Typed Float 完整配置示例

以下配置只保留 Typed Float；注意整数 Typed 仍为关闭状态：

```c
#define RTT_USER_CFG_ENABLE 1

#define RTT_LOG_ENABLE       1
#define LOG_ENABLE_LITE      0
#define LOG_ENABLE_TYPED     0
#define LOG_ENABLE_TYPED_FLOAT 1

#define LOG_ENABLE_INFO      0
#define LOG_ENABLE_DEBUG     0
#define LOG_ENABLE_WARN      0
#define LOG_ENABLE_ERROR     0
#define LOG_ENABLE_PRINT     0
#define LOG_ENABLE_STRING    0
#define LOG_ENABLE_FLOAT     0

#define RTT_FLOAT_USE_MODFF  0
#define RTT_LOG_FLOAT_FAST_PATH 0
#define RTT_WRITE_SKIP_USE_ASM 0

#define RTT_LOG_BUFFER_INDEX 0u
#define RTT_LOG_USE_COLOR    0
```

## Float 的 Flash 与效率优化

Flash 紧张时，优先选择 Typed Float，并关闭所有等级日志、`log_print`、普通 Float 和未使用的整数 Typed 接口。只要其他块仍引用通用 formatter，改用 Typed Float 后看到的最终固件降幅就可能小于预期。

普通 Float 可以用额外 Flash 换取正常有限值的执行效率：

```c
#define LOG_ENABLE_FLOAT          1
#define RTT_LOG_FLOAT_FAST_PATH   1
```

开启后，正常有限值会尝试在栈上组装完整帧并通过一次 `SEGGER_RTT_Write()` 提交。NaN、正负 Inf、超出支持范围的数值以及固定缓冲区无法容纳的标签(长度46B)仍走兼容路径并且性能会下降。该选项通常增加Flash，因此不应在空间紧张时默认开启；应在目标工程中同时测量最终 Flash 和运行时间。

`RTT_LOG_FLOAT_FAST_PATH` 只影响普通 Float，对 `log_f32` 无效。
`RTT_FLOAT_USE_MODFF`是非 IEEE-754 平台的兼容选项，不是性能优化项，也不用于减小 Flash；通常应
保持为 `0`。

# 公共优化项

## 关闭未使用的接口

关闭未使用接口是所有方案中优先级最高、风险最低的优化。对应宏会展开为空语句，参数不会求值，相关代码也更容易在预处理和链接阶段被移除。

| 不需要的能力 | 关闭项 |
|---|---|
| 全部日志 | `RTT_LOG_ENABLE=0` |
| INFO/DEBUG/WARN/ERROR | 对应 `LOG_ENABLE_*` |
| 原始格式化输出 | `LOG_ENABLE_PRINT=0` |
| 纯字符串输出 | `LOG_ENABLE_STRING=0` |
| 普通 Float | `LOG_ENABLE_FLOAT=0` |
| 整数及指针 Typed | `LOG_ENABLE_TYPED=0` |
| Typed Float | `LOG_ENABLE_TYPED_FLOAT=0` |

优先关闭接口，再考虑以额外 Flash 换取速度的优化项。

## Skip 汇编写入的适用条件

`RTT_WRITE_SKIP_USE_ASM` 控制 `SEGGER_RTT_WriteNoLock()` 在`SEGGER_RTT_MODE_NO_BLOCK_SKIP` 分支中是否委派给 ARMv7-M 汇编实现：

```c
#define RTT_WRITE_SKIP_USE_ASM 1
```

只有以下条件同时满足时才会使用汇编路径：

1. 目标和编译器使 `RTT_USE_ASM=1`。
2. `RTT_WRITE_SKIP_USE_ASM=1`。
3. 当前 Up Buffer 使用 `SEGGER_RTT_MODE_NO_BLOCK_SKIP`。

默认终端通道使用 Skip 模式。Cortex-M0 等不支持该汇编实现的目标会自动保留 C 路径。该优化可能增加 Flash，但在极短包、特定缓冲状态和特定构建中可能缩短高频 Skip 写入路径。收益取决于帧长度、日志频率、主机读取速度和目标内核。
事实上不建议开启此功能。

使用 `rtt_cfg.h` 配置时，Make/CMake 集成会让 RTT C 与汇编源看到同一配置。若改用命令行`-D`，必须确保
C 与 `.S` 源使用一致定义。

## RTT 编译优化等级

RTT C 源默认单独使用 `-Os`，该配置不属于 `rtt_cfg.h`。

Make 工程在包含 `segger_rtt.mk` 前设置：

```make
ARM_SEGGER_RTT_OPTIMIZATION := -O2
include ARM_SEGGER_RTT/segger_rtt.mk
```

CMake 工程在 `add_subdirectory` 前设置：

```cmake
set(ARM_SEGGER_RTT_OPTIMIZATION "-O2" CACHE STRING "" FORCE)
add_subdirectory(ARM_SEGGER_RTT)
```

`-O2` 或 `-O3` 可能以更多 Flash 换取部分执行效率，但不保证每个内核或日志路径都更快。设为空值表示继承主工程优化选项。修改后必须清理旧 RTT 对象并重新构建。

## 缓冲区容量与丢失概率

以下配置使用静态 RAM，而不是 Flash：

```c
#define BUFFER_SIZE_UP   1024
#define BUFFER_SIZE_DOWN 16
```

Up 环形缓冲区保留一个字节区分空和满，因此 1024 B 缓冲区最多保存 1023 B 未读数据。增大 Up Buffer 可以降低突发日志被丢弃的概率，但会增加静态 RAM；它不会直接加快格式化。

`SEGGER_RTT_PRINTF_BUFFER_SIZE` 是每次格式化调用的栈缓冲区：

```c
#define SEGGER_RTT_PRINTF_BUFFER_SIZE 64u
```

长消息会分块写入，因此该值不限制单条日志总长度。增大它会增加栈占用，是否减少分块开销需要结合实际消息长度测量。

## modff 备用实现

`RTT_FLOAT_USE_MODFF` 不是性能优化开关：

```c
#define RTT_FLOAT_USE_MODFF 0
```

默认 `0` 使用紧凑的 IEEE-754 binary32 位解析实现，不依赖 `modff`。设置为 `1` 后改用`modff` 备用路径，主要用于非 IEEE-754 平台或未来更通用的浮点格式化需求；GCC 工程通常还需要链接 `libm`（`-lm`）。

当前固定三位小数 API 通常应保持默认值 `0`。

# 配置方案汇总

| 配置方案 | 核心配置 | formatter | Flash 趋势 | 优先目标 |
|---|---|---|---|---|
| Full | `LOG_ENABLE_LITE=0`，等级接口开启 | 保留 | 通常高于 Lite/Typed | 可读性与完整调试能力 |
| Lite | `LOG_ENABLE_LITE=1`，等级接口开启 | 保留 | 通常低于 Full、高于 Typed | 低侵入减少输出装饰 |
| Typed-only | 其他入口关闭，`LOG_ENABLE_TYPED=1` | 可移除 | 固定整数/指针通常最低 | 固定类型与空间可控 |
| 普通 Float-only | 其他入口关闭，`LOG_ENABLE_FLOAT=1` | 保留兼容路径 | 通常高于 Typed Float | 标签与回退路径更灵活 |
| Typed Float-only | 其他入口关闭，`LOG_ENABLE_TYPED_FLOAT=1` | 可移除 | 浮点方案中通常更低 | 固定浮点与空间可控 |
| 混合方案（不建议） | 组合上述开关 | 通常仍保留 | 通常不省，可能占用更多 | 仅用于兼容或迁移 |

Flash 是主要限制时，先关闭未使用接口，再优先评估 Typed-only 或 Typed Float-only；
Flash 充足且重视可读性时选择 Full。Lite 更适合保留现有格式化调用的渐进式精简，不能替代
Typed 的 formatter 裁剪效果。所有趋势都应通过同一目标、工具链和调用集的最终固件验证。

# 注意事项

## 宏参数求值规则

关闭总开关或单项接口后，对应日志宏的参数不会求值。例如：

```c
log_debug("value=%u", ReadExpensiveValue());
```

关闭 Debug 后，`ReadExpensiveValue()` 不会执行。不要依赖日志参数产生业务副作用。

## 配置目录必须保持一致

应用源码、`rtt_log.c`、`rtt_float.c`、SEGGER RTT C 源和汇编源必须看到同一个`rtt_cfg.h`。仓库提供的 Make/CMake 集成已统一配置目录。

若配置不在主工程根目录：

Make：

```make
RTT_CONFIG_DIR := config
include ARM_SEGGER_RTT/segger_rtt.mk
```

CMake：

```cmake
set(ARM_SEGGER_RTT_CONFIG_DIR "${CMAKE_SOURCE_DIR}/config" CACHE PATH "" FORCE)
add_subdirectory(ARM_SEGGER_RTT)
```

## 命令行宏与配置文件不要重复定义

若通过编译命令 `-D` 临时配置，应保持：

```c
#define RTT_USER_CFG_ENABLE 0
```

启用 `rtt_cfg.h` 自定义配置块后，不要再通过命令行为同名宏传入不同值，否则不同源文件可能看到不一致配置。

## 底层配置变更必须清理旧对象

尤其是以下配置会影响控制块布局、源文件选择或编译路径：

- RTT Up/Down 通道数量。
- Up/Down Buffer 大小。
- 日志通道索引。
- 汇编分发和编译优化选项。

修改后不要继续复用旧对象或旧 CMake 缓存。清理并完整重建后，再比较 Flash、RAM 和运行时间。

# 相关文档

- [API 指南](API%20declaration.md)：完整 API、输出格式、返回值、格式化支持和边界行为。
- [移植指南](port.md)：Make/CMake 接入、编译、烧录和常见问题。
