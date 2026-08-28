# API declaration

## API 总览

| API | 定位与概述 | 输出模式 | 详细说明 |
|---|---|---|---|
| `log_info(Format, ...)` | 信息级格式化日志 | Full/Lite 等级日志，自动换行 | [跳转](#api-log-info) |
| `log_debug(Format, ...)` | 调试级格式化日志 | Full/Lite 等级日志，自动换行 | [跳转](#api-log-debug) |
| `log_warn(Format, ...)` | 警告级格式化日志 | Full/Lite 等级日志，自动换行 | [跳转](#api-log-warn) |
| `log_err(Format, ...)` | 错误级格式化日志 | Full/Lite 等级日志，自动换行 | [跳转](#api-log-err) |
| `log_float(Value)` | 无标签单精度浮点日志 | 固定三位小数，自动换行 | [跳转](#api-log-float) |
| `log_float_label(Label, Value)` | 带标签单精度浮点日志 | `Label: Value`，固定三位小数并换行 | [跳转](#api-log-float-label) |
| `log_print(Format, ...)` | 无级别的精简格式化输出 | 原样格式化，不加前缀/颜色/换行 | [跳转](#api-log-print) |
| `log_string(Text)` | 纯字符串最短输出路径 | 原样字符串，不格式化、不换行 | [跳转](#api-log-string) |
| `log_i32(Label, Value)` | 32 位有符号十进制 typed 日志 | 单帧直写，固定换行 | [跳转](#api-log-i32) |
| `log_u32(Label, Value)` | 32 位无符号十进制 typed 日志 | 单帧直写，固定换行 | [跳转](#api-log-u32) |
| `log_hex32(Label, Value)` | 32 位固定宽度十六进制 typed 日志 | `0x` + 8 位大写十六进制，固定换行 | [跳转](#api-log-hex32) |
| `log_pointer(Label, Value)` | 按目标指针宽度输出地址 | `0x` + 指针宽度大写十六进制，固定换行 | [跳转](#api-log-pointer) |
| `log_f32(Label, Value)` | 绕过通用 formatter 的 typed 单精度浮点日志 | 固定三位小数，单帧直写并换行 | [跳转](#api-log-f32) |


## 配置入口

请查阅 [配置指南](Configuration.md)

## 格式化能力

`log_info`、`log_debug`、`log_warn`、`log_err` 和 `log_print` 使用项目的嵌入式精简 formatter：

| 支持项 | 说明 |
|---|---|
| `%c` | 参数按 `int` 读取 |
| `%d` | `int` 有符号十进制，安全处理 `INT_MIN` |
| `%u` | `unsigned int` 无符号十进制 |
| `%x`、`%X` | `unsigned int` 大写十六进制，不自动添加 `0x`；两者输出相同 |
| `%s` | `const char *`；空指针输出 `(NULL)` |
| `%p` | `void *`；固定为目标指针宽度的大写十六进制，不添加 `0x` |
| `%%` | 输出 `%` |
| 标志/宽度/精度 | 支持 `-`、`0`、`+`、数字字段宽度、数字精度，以及字符串动态精度 `%.*s` |

不支持 `#`、空格标志、动态字段宽度 `%*d`、`h/l/ll/z` 等长度修饰符，以及 `%f/%e/%g`。未知转换按原格式文本输出且不消费对应值参数；若其中已有 `.*`，动态精度参数会先被消费。长消息按 `SEGGER_RTT_PRINTF_BUFFER_SIZE`（默认 64 B）分块，该缓冲区大小不限制单条消息总长度。`Format` 必须是有效的 NUL 结尾字符串。

---

<a id="api-log-info"></a>
## `log_info(Format, ...)`

**用途与要求：** 输出一般运行状态。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_INFO=1`（两者默认均为 `1`）。

**配置与特性：** Full 模式输出绿色 `[INFO] ` 前缀；`RTT_LOG_USE_COLOR=0` 可保留前缀但关闭颜色；`LOG_ENABLE_LITE=1` 会同时去掉前缀和颜色。接口总会在正文后追加 `\n`，调用者通常不要再写换行。该接口需要 formatter 和等级装饰逻辑，Flash 占用高于 `log_print` 和各类直写接口；Lite 模式可略微降低占用，但不会裁掉 formatter。宏丢弃底层返回值；关闭后整个宏为空语句，参数不求值。

```c
log_info("system ready, count=%u", 3u);
```

```text
[INFO] system ready, count=3
```

<a id="api-log-debug"></a>
## `log_debug(Format, ...)`

**用途与要求：** 输出调试细节。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_DEBUG=1`（默认 `1`）。

**配置与特性：** Full 模式使用蓝色 `[DEBUG] `；`RTT_LOG_USE_COLOR` 和 `LOG_ENABLE_LITE` 的影响与 `log_info` 相同。自动追加 `\n`。该接口需要 formatter 和等级装饰逻辑，Flash 占用相对较高；Lite 模式可减少部分装饰代码与字符串。关闭后参数不求值。

```c
log_debug("rx=%u hex=%08X", 12u, 0x2Au);
```

```text
[DEBUG] rx=12 hex=0000002A
```

<a id="api-log-warn"></a>
## `log_warn(Format, ...)`

**用途与要求：** 输出需要关注但不一定导致操作失败的状态。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_WARN=1`（默认 `1`）。

**配置与特性：** Full 模式使用黄色 `[WARN] `；可通过 `RTT_LOG_USE_COLOR` 关闭颜色，通过 `LOG_ENABLE_LITE` 去掉所有装饰。自动追加 `\n`。该接口需要 formatter 和等级装饰逻辑，Flash 占用相对较高；Lite 模式可减少部分装饰代码与字符串。关闭后参数不求值。

```c
log_warn("voltage=%u mV", 3250u);
```

```text
[WARN] voltage=3250 mV
```

<a id="api-log-err"></a>
## `log_err(Format, ...)`

**用途与要求：** 输出错误状态。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_ERROR=1`（默认 `1`）。

**配置与特性：** Full 模式使用红色 `[ERROR] `；颜色和 Lite 行为同上。自动追加 `\n`。该接口需要 formatter 和等级装饰逻辑，Flash 占用相对较高；Lite 模式可减少部分装饰代码与字符串。接口名是 `log_err`，不是 `log_error`。关闭后参数不求值。

```c
log_err("status=%d", -1);
```

```text
[ERROR] status=-1
```

<a id="api-log-print"></a>
## `log_print(Format, ...)`

**用途与要求：** 需要格式化但不需要等级装饰时使用。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_PRINT=1`（默认 `1`）。

**配置与特性：** 不受 `LOG_ENABLE_LITE`、`RTT_LOG_USE_COLOR` 影响，也不自动换行。它比等级日志少一层帧装饰，但仍会链接 formatter。Flash 占用高于 `log_string` 和 typed 直写接口，低于功能完整的等级日志，是 Flash 受限但仍需格式化时的平衡选择。宏丢弃返回值；关闭后参数不求值。

```c
log_print("progress=%u%%\n", 75u);
```

```text
progress=75%
```

<a id="api-log-string"></a>
## `log_string(Text)`

**用途与要求：** 已有完整字符串且不需要格式化时的最短路径。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_STRING=1`（默认 `1`）。依赖 `rtt_log.c` 和 `SEGGER_RTT_WriteString()`，不依赖 formatter。

**配置与特性：** 文本完全原样输出，`%` 没有特殊含义，不自动换行。该接口不链接 formatter，是本日志层理论上 Flash 占用最低的输出选择。启用时返回 `SEGGER_RTT_WriteString()` 的结果；`Text==NULL` 返回 `-1` 且不写入。关闭后宏展开为常量 `0`，参数不求值。字符串必须以 NUL 结尾，不能用于包含内嵌 NUL 的二进制数据。

```c
int Count = log_string("rate=%d 100%\n");
```

```text
rate=%d 100%
```

<a id="api-log-i32"></a>
## `log_i32(Label, Value)`

**用途与要求：** 不链接通用 formatter 即可输出 `int32_t`。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_TYPED=1`（后者默认 `0`）。不使用堆内存或整数除法库。

**配置与特性：** `Value` 转换为 `int32_t`，支持完整范围（包括 `INT32_MIN`）。`Label` 为 `NULL` 或空串时只输出值；非空时输出 `Label: Value\n`。标签按字节处理，最多保留 46 B，超出部分静默截断且不添加省略号；数值永不因标签过长而截断。整帧在固定栈缓冲区组装后单次写入。不链接 formatter 或整数除法库，Flash 占用较低，是固定有符号整数日志的低占用选择。宏接口不向调用者暴露写入结果。

```c
log_i32("offset", INT32_MIN);
```

```text
offset: -2147483648
```

<a id="api-log-u32"></a>
## `log_u32(Label, Value)`

**用途与要求：** 不链接 formatter 输出 `uint32_t`。

**配置与特性：** `Value` 转换为 `uint32_t`，范围为 `0` 至 `4294967295`。标签、46 B 截断、单次写入和禁用行为均与 `log_i32` 相同。该接口不链接 formatter 或整数除法库，Flash 占用较低。

```c
log_u32("ticks", UINT32_MAX);
```

```text
ticks: 4294967295
```

<a id="api-log-hex32"></a>
## `log_hex32(Label, Value)`

**用途与要求：** 输出便于观察寄存器、掩码和状态字的固定宽度十六进制。要求 `RTT_LOG_ENABLE=1`、`LOG_ENABLE_TYPED=1`。

**配置与特性：** `Value` 转换为 `uint32_t`，固定输出 `0x` 和 8 位大写十六进制，不省略前导零。标签最多 46 B，整帧单次写入并固定换行。该接口不链接 formatter，Flash 占用较低，适合资源受限时输出寄存器和状态字。

```c
log_hex32("status", 0x2Au);
```

```text
status: 0x0000002A
```

<a id="api-log-pointer"></a>
## `log_pointer(Label, Value)`

**用途与要求：** 输出目标地址或指针。要求 `RTT_LOG_ENABLE=1`、`LOG_ENABLE_TYPED=1`。

**配置与特性：** 值转换为 `const void *`，再按 `sizeof(uintptr_t) * 2` 个大写十六进制数字输出，带 `0x` 和前导零；32 位目标为 8 位数字，64 位目标为 16 位数字。标签最多 46 B，整帧单次写入并固定换行。该接口不链接 formatter，Flash 占用较低。传入整数地址时应先显式转换为 `uintptr_t` 再转换为指针，以避免编译器诊断。

```c
log_pointer("buffer", Buffer);
```

```text
buffer: 0x20000000
```

<a id="api-log-f32"></a>
## `log_f32(Label, Value)`

**用途与要求：** 在不链接通用 formatter 的配置中输出单精度浮点数。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_TYPED_FLOAT=1`（默认 `0`）；它不依赖 `LOG_ENABLE_TYPED` 或 `LOG_ENABLE_FLOAT`。

**配置与特性：** `Value` 转换为 `float`，小数直接截断为三位而非四舍五入。标签规则与整数 typed API 相同：`NULL`/空串不输出分隔符，最多 46 B，超出静默截断。最大帧为 64 B，始终一次写入；不受 `RTT_LOG_FLOAT_FAST_PATH` 影响。它不链接通用 formatter，是 Flash 占用最低的浮点日志选择，但占用通常略高于固定整数 typed 接口。`RTT_FLOAT_USE_MODFF=1` 时改用 `modff()` 并可能需要 `-lm`。宏接口不向调用者暴露写入结果。

```c
log_f32("voltage", 3.3f);
```

```text
voltage: 3.299
```

`3.3f` 的 binary32 实际值略小于 3.3，结合截断语义可能得到 `3.299`，这不是四舍五入错误。

<a id="api-log-float"></a>
## `log_float(Value)`

**用途与要求：** 便捷输出无标签 `float`。要求 `RTT_LOG_ENABLE=1` 且 `LOG_ENABLE_FLOAT=1`（默认 `1`）。

**配置与特性：** 固定三位小数并截断，自动换行，无等级或颜色。`Value` 只求值一次并转换为 `float`。该接口需要浮点转换代码，默认兼容路径还会链接 formatter，因此 Flash 占用高于 `log_f32` 和固定类型接口。`RTT_LOG_FLOAT_FAST_PATH=1` 时，正常有限值在 64 B 栈缓冲区组帧并单次写入，减少 formatter 和多次写入开销，代价是进一步增加 Flash；默认 `0` 更偏向控制代码体积。特殊值仍走兼容格式化路径。关闭接口后参数不求值。
其中,`RTT_LOG_FLOAT_FAST_PATH = 1`在标签长度大于46B时效率反而会回退,因此应该理性判断并使用,详情请查阅 [配置指南](Configuration.md#rtt_log_float_fast_path)
```c
log_float(-2.5f);
```

```text
-2.500
```

<a id="api-log-float-label"></a>
## `log_float_label(Label, Value)`

**用途与要求：** 输出带说明文字的 `float`。

**配置与特性：** `Label==NULL` 时只输出数值；非空指针（包括 `""`）均输出 `Label: Value\n`，因此空字符串标签会产生前导 `": "`。与 typed 接口不同，该 API 保留完整标签，不执行 46 B 截断。该接口需要浮点转换和兼容 formatter，Flash 占用高于 `log_f32` 和固定类型接口。开启 `RTT_LOG_FLOAT_FAST_PATH` 后，正常有限值且标签不超过 46 B 时采用单次直写；标签更长、NaN、Inf 和 Overflow 自动回退兼容 formatter，输出内容不变。快速模式提高有限短标签路径效率，但会进一步增加 Flash；并且当标签长度大于46B时,效率反而会回退,详情请查阅[配置指南](Configuration.md#rtt_log_float_fast_path),宏接口不报告短写。

```c
log_float_label("temperature", 1.9996f);
```

```text
temperature: 1.999
```

### 浮点值的共同边界规则

| 输入 | 数值部分输出 |
|---|---|
| `1.2349f` | `1.234` |
| `-0.0009f`、`-0.0f` | `0.000` |
| NaN（不区分符号） | `NaN` |
| 正/负无穷 | `Inf` / `-Inf` |
| 绝对值大于等于 `2^32` | `Overflow` / `-Overflow` |
