# RTT库配置指南

## 目录

- [RTT库配置指南](#rtt库配置指南)
  - [目录](#目录)
  - [`rtt_cfg.h` 简介](#rtt_cfgh-简介)
  - [开启配置方式](#开启配置方式)
  - [全部配置项速查](#全部配置项速查)
  - [配置块总开关](#配置块总开关)
    - [`RTT_USER_CFG_ENABLE`](#rtt_user_cfg_enable)
  - [日志功能和输出形式](#日志功能和输出形式)
    - [`RTT_LOG_ENABLE`](#rtt_log_enable)
    - [`LOG_ENABLE_LITE`](#log_enable_lite)
    - [`RTT_LOG_USE_COLOR`](#rtt_log_use_color)
  - [各日志 API 的独立裁剪开关](#各日志-api-的独立裁剪开关)
    - [`LOG_ENABLE_INFO`](#log_enable_info)
    - [`LOG_ENABLE_DEBUG`](#log_enable_debug)
    - [`LOG_ENABLE_WARN`](#log_enable_warn)
    - [`LOG_ENABLE_ERROR`](#log_enable_error)
    - [`LOG_ENABLE_PRINT`](#log_enable_print)
    - [`LOG_ENABLE_STRING`](#log_enable_string)
    - [`LOG_ENABLE_FLOAT`](#log_enable_float)
    - [`LOG_ENABLE_TYPED`](#log_enable_typed)
    - [`LOG_ENABLE_TYPED_FLOAT`](#log_enable_typed_float)
  - [浮点实现选择](#浮点实现选择)
    - [`RTT_FLOAT_USE_MODFF`](#rtt_float_use_modff)
    - [`RTT_LOG_FLOAT_FAST_PATH`](#rtt_log_float_fast_path)
      - [使用说明](#使用说明)
  - [RTT 写入实现选择](#rtt-写入实现选择)
    - [`RTT_WRITE_SKIP_USE_ASM`](#rtt_write_skip_use_asm)
  - [日志通道和 RTT 控制块](#日志通道和-rtt-控制块)
    - [`RTT_LOG_BUFFER_INDEX`](#rtt_log_buffer_index)
    - [`SEGGER_RTT_MAX_NUM_UP_BUFFERS`](#segger_rtt_max_num_up_buffers)
    - [`SEGGER_RTT_MAX_NUM_DOWN_BUFFERS`](#segger_rtt_max_num_down_buffers)
  - [数据缓冲区与 formatter 缓冲区](#数据缓冲区与-formatter-缓冲区)
    - [`BUFFER_SIZE_UP`](#buffer_size_up)
    - [`BUFFER_SIZE_DOWN`](#buffer_size_down)
    - [`SEGGER_RTT_PRINTF_BUFFER_SIZE`](#segger_rtt_printf_buffer_size)
  - [关键组合与连带影响](#关键组合与连带影响)


## `rtt_cfg.h` 简介

rtt_cfg.h 是 RTT 库的用户配置入口的用户配置模板；复制到应用工程的配置目录并启用后，可通过该文件实现统一控制日志功能、输出模式、日志通道、浮点实现及缓冲区大小、优化 MCU 资源占用等功能。并避免直接修改 RTT 子仓库代码。

本库无需用户配置即可使用。默认开启日志总开关;使用 Full 模式和普通Float并关闭 Typed 模式，数据流输出到 RTT Up Buffer 0。浮点转换默认使用不依赖 `modff()` 的 IEEE-754 binary32 解析实现，`RTT_LOG_FLOAT_FAST_PATH` 默认为 `0`，使用兼容格式化路径。

但建议启用自定义配置，按需裁剪日志等级、输出模式和缓冲区，以充分发挥本库的资源优化能力。

仓库中的模板`rtt_cfg.h`默认设置：

```c
#define RTT_USER_CFG_ENABLE 0
```
此时模板配置块不参与编译，各模块使用自己的内置默认值,此举旨在让用户将配置模板复制到本地工程并显式启用，在不修改 RTT 库源码及其 Git 状态的情况下完成项目级配置。


## 开启配置方式

如需自定义配置，可选择以下任一方式：

1. 将库内的 `rtt_cfg.h` 模板复制到用户工程的配置目录（通常是工程根目录），将 `RTT_USER_CFG_ENABLE` 设为 `1`，再修改所需配置项。配置头不在默认搜索目录时，Make 工程通过 `RTT_CONFIG_DIR` 指定目录，CMake 工程通过 `ARM_SEGGER_RTT_CONFIG_DIR` 指定目录。
2. 通过编译器 `-D` 选项直接覆盖需要调整的配置宏。应将相同宏应用到应用代码和所有 RTT 相关源文件，修改后清理并重新编译。

通常建议用户采用第一种方式,该方式更有利于将产品配置统一保留在主工程中，避免直接修改和频繁提交 RTT 子仓库，同时便于库的升级与多工程复用。

配置优先级和注意事项：

1. CMake 通过 `ARM_SEGGER_RTT_CONFIG_DIR`、Make 通过 `RTT_CONFIG_DIR` 指定应用配置目录；该目录应排在库目录之前。
2. `RTT_USER_CFG_ENABLE=1` 时，配置块中的宏会先于各模块的 `#ifndef` 默认值定义，因此覆盖模块默认值。
3. 使用编译命令的 `-D宏=值` 临时覆盖时，应保持 `RTT_USER_CFG_ENABLE=0`，否则同名宏会重复定义。
4. 应用源码、`rtt_log.c`、`rtt_float.c`、`SEGGER_RTT.c` 和 `SEGGER_RTT_printf.c` 必须看到同一份配置。修改后必须清理旧对象并全量重编译。
5. 日志宏被关闭后会展开为空语句，其参数不会求值。不要在日志参数中放置赋值、计数、读寄存器或函数调用等必需的业务副作用。

## 全部配置项速查

所有布尔开关均应使用 `0`（关闭）或 `1`（开启）。
这里的“副作用”不仅指Flash/RAM 变化，也包括输出格式、参数求值、实时性、链接依赖和配置一致性。

| 配置项 | 模板/内置默认值 | 直接控制内容 | 主要副作用 |
|---|---:|---|---|
| `RTT_USER_CFG_ENABLE` | `0` | 是否启用模板中的自定义配置块 | 设为 `0` 时，块内所有修改均无效 |
| `RTT_LOG_ENABLE` | `1` | 整个日志封装层 | 关闭后全部 `log_*` 宏不输出且参数不求值 |
| `LOG_ENABLE_LITE` | `0` | 等级日志的 Full/Lite 格式 | 开启后丢失等级前缀和颜色，但仍使用 formatter |
| `LOG_ENABLE_TYPED` | `0` | 整数、十六进制、指针 typed API | 开启会增加直写实现和固定栈帧，但可配合裁掉 formatter |
| `LOG_ENABLE_TYPED_FLOAT` | `0` | `log_f32` typed 浮点 API | 开启会增加浮点转换及 64 B 固定帧路径 |
| `LOG_ENABLE_INFO` | `1` | `log_info` | 关闭后调用及参数求值消失 |
| `LOG_ENABLE_DEBUG` | `1` | `log_debug` | 关闭后调用及参数求值消失 |
| `LOG_ENABLE_WARN` | `1` | `log_warn` | 关闭后调用及参数求值消失 |
| `LOG_ENABLE_ERROR` | `1` | `log_err` | 关闭后调用及参数求值消失 |
| `LOG_ENABLE_PRINT` | `1` | `log_print` | 关闭后原始格式化日志消失 |
| `LOG_ENABLE_STRING` | `1` | `log_string` | 关闭后宏固定返回 `0`，参数不求值 |
| `LOG_ENABLE_FLOAT` | `1` | `log_float`、`log_float_label` | 关闭可移除传统浮点日志路径 |
| `RTT_FLOAT_USE_MODFF` | `0` | 浮点拆分算法 | 开启通常引入 `modff`/`libm` 依赖 |
| `RTT_LOG_FLOAT_FAST_PATH` | `0` | 传统浮点日志有限数快速路径 | 开启以额外 Flash 和栈缓冲换取较短写入路径 |
| `RTT_WRITE_SKIP_USE_ASM` | `0` | Skip 模式是否分发到 RTT 汇编写入 | 只在 `RTT_USE_ASM=1` 时有效，会改变代码尺寸和时序 |
| `RTT_LOG_BUFFER_INDEX` | `0u` | 所有日志使用的 Up Buffer | 非 0 通道必须预先配置，且索引必须在范围内 |
| `RTT_LOG_USE_COLOR` | `1` | Full 等级日志的 ANSI 颜色 | 增加控制字节；终端不支持时会显示乱码 |
| `SEGGER_RTT_MAX_NUM_UP_BUFFERS` | `3` | Up Buffer 描述符数量 | 数量越大，RTT 控制块静态 RAM 越大 |
| `SEGGER_RTT_MAX_NUM_DOWN_BUFFERS` | `3` | Down Buffer 描述符数量 | 数量越大，RTT 控制块静态 RAM 越大 |
| `BUFFER_SIZE_UP` | `1024` | 默认 Up Buffer 0 的环形缓冲区 | 直接占静态 RAM；有效载荷容量为尺寸减 1 |
| `BUFFER_SIZE_DOWN` | `16` | 默认 Down Buffer 0 的环形缓冲区 | 直接占静态 RAM；太小会限制主机下发突发数据 |
| `SEGGER_RTT_PRINTF_BUFFER_SIZE` | `64u` | formatter 的分块临时缓冲区 | 直接增加每次格式化调用的栈占用 |

## 配置块总开关

### `RTT_USER_CFG_ENABLE`

- 用途：决定应用是否采用 `rtt_cfg.h` 中的自定义参数。
- `0`：忽略 `#if RTT_USER_CFG_ENABLE` 内的全部定义，使用 `rtt_log.h`、`rtt_float.h` 和 `SEGGER_RTT_Conf.h` 中的默认值。
- `1`：启用本文件中的整组应用配置。
- 副作用：它不是功能开关，而是配置来源开关。误设为 `0` 会造成“修改了配置但固件行为不变”；设为 `1` 后又通过 `-D` 定义同名宏，可能产生重定义诊断。

## 日志功能和输出形式

### `RTT_LOG_ENABLE`

- 用途：控制应用是否保留 RTT 日志能力。开发和联调阶段建议设为 `1`，通过实时日志观察程序状态并定位问题；产品发布阶段若不需要现场日志，通常设为 `0`，以减少Flash 占用、运行开销和无意泄露内部状态的风险。若发布产品仍需要故障追踪，可保持为 `1`，再关闭 INFO/DEBUG 等非必要等级，只保留 ERROR 或必要的诊断接口。
- `1`：编译日志实现，其他 `LOG_ENABLE_*` 开关继续决定具体 API。
- `0`：`log_info/debug/warn/err/print` 和 typed/float 宏均为空操作，`log_string()` 固定产生值 `0`；宏实参不会求值，也不会生成 RTT 输出调用。
- 副作用：关闭可显著减少日志相关 Flash 和运行时间，但不会关闭底层 SEGGER RTT本身；应用仍可直接调用 `SEGGER_RTT_*`。`RTT_LogPrintf()`、`RTT_LogString()` 和`RTT_LogFloat3()` 为兼容直接调用保留空实现/符号，因此不要用直接调用绕过宏来判断功能是否启用。
- 约束：只有该宏为 `1` 时才检查
  `RTT_LOG_BUFFER_INDEX < SEGGER_RTT_MAX_NUM_UP_BUFFERS`。

### `LOG_ENABLE_LITE`

- 用途：在保持现有 API 不变的情况下精简日志输出格式。适用于需要保留日志正文，同时希望减少 RTT 传输量和等级装饰代码开销的资源受限场景。
- `0`：已启用的 `log_info/debug/warn/err` 输出等级前缀。
- `1`：`log_info/debug/warn/err` 四类等级宏统一按 `RTT_LOG_LEVEL_PRINT` 输出，只保留正文和自动换行；此设置的优先级高于 `RTT_LOG_USE_COLOR`，覆盖 `RTT_LOG_USE_COLOR` 为 `0` 。
- 副作用：主机端无法再从文本区分 INFO、DEBUG、WARN、ERROR；`RTT_LOG_USE_COLOR` 在 Lite 模式下不生效。不影响 `log_print`、`log_string`、`log_float*` 和 typed API。

### `RTT_LOG_USE_COLOR`

- 用途：利用颜色快速区分不同等级的日志。适合支持 ANSI 转义序列的开发终端。
- `1`：Full 模式为四种等级分别添加 ANSI 颜色，并在换行前输出复位序列。
- `0`：仅输出纯文本 `[INFO] `、`[DEBUG] `、`[WARN] `、`[ERROR] ` 前缀。
- 副作用：开启会增加每条等级日志的 RTT 流量；不支持 ANSI 的查看器可能直接显示转义字符。该开关在 `LOG_ENABLE_LITE=1` 时无效，对 `PRINT`、字符串、浮点和typed 输出也无效。

## 各日志 API 的独立裁剪开关

以下开关仅在 `RTT_LOG_ENABLE=1` 时有效。关闭任一开关后，相应宏的所有实参都不会求值。只有关闭了所有会使用通用格式化器的路径并启用链接时的 section garbage collection，formatter 才一定有机会从最终固件中被裁掉。

### `LOG_ENABLE_INFO`

- 用途：输出启动流程、配置结果和正常状态变化等一般运行信息。开发和系统联调阶段通常开启；发布版本可根据现场可观测性需求决定是否保留。
- 控制 `log_info(Format, ...)`。关闭后信息日志完全消失。副作用是运行状态信息不可见，且原先写在参数表达式中的代码不再执行；好处是对应调用点、格式串及独占代码可被裁剪。

### `LOG_ENABLE_DEBUG`

- 用途：输出函数路径、中间变量和协议细节等高频调试信息。开发定位问题时开启，正常发布构建通常关闭。
- 控制 `log_debug(Format, ...)`。发布构建常设为 `0` 以减少 Flash、RTT 带宽和运行时间。副作用是现场调试细节丢失，并同样取消参数求值。

### `LOG_ENABLE_WARN`

- 用途：记录可恢复异常、资源接近阈值、重试和降级运行等需要关注但未导致功能失败的状态。需要现场健康监测时建议保留。
- 控制 `log_warn(Format, ...)`。关闭可减少代码和流量，但会失去非致命异常、降级路径等告警信息，参数不再求值。

### `LOG_ENABLE_ERROR`

- 用途：记录初始化失败、通信错误和无法恢复的业务异常。发布固件若需要保留最小现场诊断能力，应优先保留该等级，再关闭 DEBUG、INFO 等低优先级日志。
- 控制 `log_err(Format, ...)`。关闭可减少代码和流量，但会失去错误诊断信息，参数不再求值。

### `LOG_ENABLE_PRINT`

- 用途：输出由应用自行组织格式、且不需要等级标签和自动换行的内容，例如命令行回显、简单数据表或兼容已有 `printf` 风格代码。
- 控制 `log_print(Format, ...)`，内部直接使用 `SEGGER_RTT_printf()`。
- 输出不带等级前缀、颜色和自动换行，换行必须由调用者在格式串中提供。
- 副作用：开启会保留通用 formatter；关闭后所有 `log_print` 调用和参数求值消失，但只要等级日志或传统浮点兼容路径仍启用，formatter 仍可能保留。

### `LOG_ENABLE_STRING`

- 用途：以最直接的路径发送已经准备好的字符串。适合固定文本、调用者自行拼接的帧，或只需最小字符串输出能力的精简固件。
- 控制 `log_string(Text)`，内部使用 `SEGGER_RTT_WriteString()` 原样写字符串，不格式化、不自动换行。
- 关闭后宏展开为常量表达式 `0`，`Text` 不求值；
- 副作用：如果调用方把返回值当作实际写入字节数，关闭后 `0` 同时表示“功能关闭”而不是一次真实写入成功。开启本项本身不需要 formatter。

### `LOG_ENABLE_FLOAT`

- 用途：为常规业务代码提供带可选标签的单精度浮点日志，适合查看传感器值、计算结果和控制参数，而无需在调用点手工拆分整数与小数。
- 控制 `log_float(Value)` 和 `log_float_label(Label, Value)`，固定输出三位小数并自动换行。
- 小数直接截断而非四舍五入；支持 `NaN`、`Inf`、`-Inf` 和 `Overflow` 文本。
- 关闭后宏参数不求值，浮点日志主体可被裁剪。
- 副作用：开启会引入浮点拆分代码；默认兼容路径还会使用通用 formatter。`LOG_ENABLE_TYPED_FLOAT` 与本项相互独立。

### `LOG_ENABLE_TYPED`

- 用途：在资源和执行时间更敏感的 MCU 上输出固定类型数据。适合日志内容主要为32 位整数、十六进制值和指针，并希望避免通用格式串解析的场景。
- 控制 `log_i32`、`log_u32`、`log_hex32` 和 `log_pointer`。
- typed API 不解析格式串，直接在栈上组帧并一次写入 RTT；标签最多输出前 46 B，超出部分静默截断，固定自动换行。
- 副作用：开启会增加专用转换代码，并在调用时使用至少 64 B 的局部帧缓冲区；标签按字节截断，UTF-8 中文可能恰好截在多字节字符中。若同时关闭等级日志、`PRINT` 和传统`FLOAT`，typed 模式可以避免保留通用 formatter。
- 注意：它与 `LOG_ENABLE_TYPED_FLOAT` 独立；开启 typed 整数不会自动开启 `log_f32`。

### `LOG_ENABLE_TYPED_FLOAT`

- 用途：为资源受限目标提供格式固定、开销可预测的单精度浮点输出。适合只需要标签和三位小数、不需要通用浮点格式控制的场景。
- 控制 `log_f32(Label, Value)`。它固定三位小数、标签最多 46 B、使用 64 B 栈上固定帧，并通过一次 `SEGGER_RTT_Write()` 提交。
- 副作用：开启会引入浮点拆分和直写实现，增加 Flash 与固定栈需求；同样存在 UTF-8标签按字节截断的问题。
- 它不依赖 `LOG_ENABLE_TYPED`、`LOG_ENABLE_FLOAT` 或`RTT_LOG_FLOAT_FAST_PATH`，可单独用于不链接通用 formatter 的浮点输出方案。

## 浮点实现选择

### `RTT_FLOAT_USE_MODFF`

- 用途：选择浮点数整数部分与小数部分的拆分方法。常规 IEEE-754 MCU 应保持为 `0`；只有移植到非 IEEE-754 平台，或验证未来通用浮点格式需求时才考虑设为 `1`。
- `0`：使用工程内置的 IEEE-754 binary32 位解析实现，不依赖浮点数学库。
- `1`：使用 `modff()` 拆分整数和小数部分。
- 副作用：GCC 等工具链通常需要额外链接 `libm`（`-lm`），并可能增加 Flash、调用开销以及软浮点运行库代码。该路径主要用于非 IEEE-754 平台或未来更通用的格式化需求；当前固定三位小数 API 下通常没有必要开启。
- 影响范围：只要 `LOG_ENABLE_FLOAT` 或 `LOG_ENABLE_TYPED_FLOAT` 开启，该选择就会影响共用的浮点拆分实现。

### `RTT_LOG_FLOAT_FAST_PATH`

- 用途：缩短 legacy 普通有限浮点日志的组帧和写入路径。
- `0`：使用兼容 formatter，优先控制 Flash 占用。
- `1`：可容纳的日志帧通过一次 `SEGGER_RTT_Write()` 直接写入。
- 副作用：增加 Flash 和固定栈占用；兼容 formatter 仍需保留，长标签可能产生额外的容量检查开销。

#### 使用说明

`RTT_LOG_FLOAT_FAST_PATH` 仅优化 legacy 普通有限浮点日志，即 `log_float()` 和`log_float_label()`。其有效范围按标签的**编码字节数**计算，不按字符数计算，字符串末尾的 `\0` 不计入标签长度。

- 标签长度 0～46 B：完整日志帧能够放入 64 B 快速缓冲区，使用一次直接写入，性能提升。
- 标签长度 ≥47 B：无法使用快速路径，将安全回退到 formatter。输出内容保持不变，但由于增加了容量检查，相比 `RTT_LOG_FLOAT_FAST_PATH=0` 可能变慢。
- NaN、Inf、Overflow 等特殊值不使用有限浮点快速路径。
- `log_f32()` 始终使用固定帧直接写入，不受该开关影响。

若工作负载主要使用短标签，建议启用；若频繁使用 47 B 及以上标签且对性能敏感，建议关闭。

ASCII 字符各占 1 B：

```c
/* 46 B：使用快速路径。 */
log_float_label("1234567890123456789012345678901234567890123456", Value);

/* 47 B：回退兼容 formatter。 */
log_float_label("12345678901234567890123456789012345678901234567", Value);
```

普通汉字在源文件和编译器使用 UTF-8 编码格式中通常占 3 B：

```c
/* 15 个汉字 x 3 B + 1 个 ASCII 字符 = 46 B：使用快速路径。 */
log_float_label("一二三四五六七八九十一二三四五1", Value);

/* 15 个汉字 x 3 B + 2 个 ASCII 字符 = 47 B：回退兼容 formatter。 */
log_float_label("一二三四五六七八九十一二三四五12", Value);
```

常用汉字在源文件和编译器使用  GBK 编码格式中通常占 2 B，ASCII 字符占 1 B：

```c
/* 23 个汉字 x 2 B = 46 B：使用快速路径。 */
log_float_label("一二三四五六七八九十"
                "一二三四五六七八九十"
                "一二三", Value);

/* 23 个汉字 x 2 B + 1 个 ASCII 字符 = 47 B：回退兼容 formatter。 */
log_float_label("一二三四五六七八九十"
                "一二三四五六七八九十"
                "一二三1", Value);
```

上述 UTF-8 和 GBK 示例分别以对应编码保存并编译时才具有标注的字节数。源文件编码、编译器执行字符集和 RTT 查看端的解码方式必须保持一致；可使用 `strlen(Label)` 核对运行时实际标签字节数。

## RTT 写入实现选择

### `RTT_WRITE_SKIP_USE_ASM`

- 用途：优化 RTT Up Buffer 处于非阻塞 Skip 模式时的底层写入效率。适合支持相应汇编实现且日志写入频繁、对执行周期敏感的 ARM 目标；优先考虑可移植性和代码尺寸时保持为 `0`。
- `0`：`SEGGER_RTT_WriteNoLock()` 的 `SEGGER_RTT_MODE_NO_BLOCK_SKIP` 分支使用 C 实现。
- `1`：当 SEGGER 自动判定 `RTT_USE_ASM=1` 时，把 Skip 分支分发到架构相关汇编实现。
- 副作用：支持的 ARMv7-M 等目标上在极短包、特定缓冲状态和特定构建中可能缩短高频写入路径，但会改变 Flash 尺寸和精确执行时序；不支持汇编的 Cortex-M0 等目标因 `RTT_USE_ASM=0` 仍使用 C 路径。
- 该开关不改变 Skip 语义：空间不足时整次写入返回 `0`，不会部分写入；也不影响 Trim或 Block 模式。若工程完全禁用 RTT 汇编，必须让所有 RTT C 与 `.S` 文件一致地看到`RTT_USE_ASM=0`。

事实上不建议开启此功能。

## 日志通道和 RTT 控制块

### `RTT_LOG_BUFFER_INDEX`

- 用途：把本日志模块与其他 RTT 数据流分配到不同 Up 通道。例如通道 0 用于终端日志，通道 1 用于二进制采样数据；只有单一日志流时保持默认通道 0 最简单。
- 指定所有日志封装 API 写入的 RTT Up Buffer 索引，默认 `0u`。
- 约束：日志开启时必须满足`RTT_LOG_BUFFER_INDEX < SEGGER_RTT_MAX_NUM_UP_BUFFERS`，否则编译报错。
- 副作用：通道 0 在 RTT 初始化时自动绑定 `BUFFER_SIZE_UP`；通道 1 及以上只有描述符，应用必须在首次日志前调用 `SEGGER_RTT_ConfigUpBuffer()` 提供名称、存储区、尺寸和模式。仅增大 `SEGGER_RTT_MAX_NUM_UP_BUFFERS` 不会为额外通道自动分配数据缓冲区。
- 修改通道会影响主机端采集配置。若主机仍只监听通道 0，看起来会像“日志丢失”。

### `SEGGER_RTT_MAX_NUM_UP_BUFFERS`

- 用途：预留 Target 向 Host 发送数据所需的通道描述符数量。只有需要将日志、采样数据或不同协议分离到多个上行通道时才增加；单通道应用可设为 `1` 以节省控制块 RAM。
- 设置 Target 到 Host 的 Up Buffer 描述符数量；必须至少保留通道 0，并大于日志索引。
- 副作用：该值直接改变 RTT 控制块结构布局和静态 RAM。32 位目标上每增加一个 Up描述符通常增加 24 B 控制块 RAM，但不会自动增加相应的数据缓冲区 RAM。
- 所有编译单元必须使用相同值。混用旧对象会导致控制块布局不一致，可能表现为越界、通道不可见或数据损坏。

### `SEGGER_RTT_MAX_NUM_DOWN_BUFFERS`

- 用途：预留 Host 向 Target 发送命令或数据所需的通道描述符数量。只使用默认终端输入时保留通道 0；需要独立命令、配置或测试数据通道时再增加。
- 设置 Host 到 Target 的 Down Buffer 描述符数量；即使应用不读取主机数据，也应至少保留默认通道 0。
- 副作用：32 位目标上每增加一个 Down 描述符通常增加 24 B 控制块静态 RAM；额外通道仍需应用调用 `SEGGER_RTT_ConfigDownBuffer()` 并自行提供数据缓冲区。
- 与 Up 数量一样，它改变公共控制块 ABI，修改后必须全量重编译。

## 数据缓冲区与 formatter 缓冲区

### `BUFFER_SIZE_UP`

- 用途：为默认上行通道吸收目标短时间内产生、但主机尚未来得及读取的突发数据。日志频率高、单条消息长或主机轮询间隔大时应适当增大；RAM 紧张且日志稀疏时可以缩小。
- 设置自动创建的 Up Buffer 0 的静态环形缓冲区大小，默认 1024 B。
- 实际可保存的未读数据最多为 `BUFFER_SIZE_UP - 1` B，环形缓冲区保留 1 B 用于区分空和满。
- 副作用：增大值会等量增加静态 RAM，并降低突发日志因主机来不及读取而丢失的概率；缩小会节省 RAM，但更容易触发当前通道模式的 Skip、Trim 或 Block 行为。Block 模式下缓冲区满可能长时间阻塞目标，Skip 模式下则会丢弃整次写入。
- 在启用 D-Cache 且配置了 cache-line 对齐的目标上，实际分配量可能向 cache line 向上取整。该宏不为 Up 1、Up 2 等额外通道分配内存。

### `BUFFER_SIZE_DOWN`

- 用途：缓存主机发送给目标的终端输入、命令或控制数据。主机可能连续发送较长命令时应增大；目标只接收少量单字符控制且 RAM 紧张时可以缩小。
- 设置自动创建的 Down Buffer 0 静态环形缓冲区大小，默认 16 B。
- 副作用：增大值会等量增加静态 RAM，并提高主机连续下发数据的容纳能力；缩小可节省RAM，但主机输入更容易因目标读取不及时而等待或丢失。启用 cache-line 对齐时实际分配量也可能向上取整。
- 该宏不为额外 Down 通道分配内存。

### `SEGGER_RTT_PRINTF_BUFFER_SIZE`

- 用途：在 formatter 的栈占用与分块写入效率之间取舍。栈空间充足且格式化长消息较多时可适当增大；小栈 MCU 应保持较小值，并结合实际最大栈深度验证。
- 设置 `SEGGER_RTT_printf()` 及工程 formatter 每次分块输出所用的局部临时缓冲区，默认 64 B，必须至少为 1。
- 它不限制单条日志总长度；长日志会分成多个不超过该尺寸的块依次写入。
- 副作用：增大会直接提高每层格式化调用的栈占用，并减少长消息的分块写入次数；缩小会降低栈峰值，但增加写调用次数、锁持有/处理开销，并可能让长帧更容易在中途遇到写失败。它不改变 `BUFFER_SIZE_UP`，也不改善 RTT 环形缓冲区的总容量。

## 关键组合与连带影响

| 目标 | 推荐关键配置 | 必须接受的影响 |
|---|---|---|
| 开发期完整诊断 | `RTT_LOG_ENABLE=1`、`LOG_ENABLE_LITE=0`、`RTT_LOG_USE_COLOR=1` | 输出字节和 Flash 较多，终端需支持 ANSI |
| 发布版保留错误日志 | 关闭 INFO/DEBUG/WARN，保留 ERROR | 被关闭日志的参数不执行；仍保留 formatter |
| 保留等级 API 但减少装饰 | `LOG_ENABLE_LITE=1` | 文本中无法区分日志等级，formatter 仍存在 |
| 只需原始字符串 | 仅开启 `LOG_ENABLE_STRING` | 无格式化、无自动换行，资源路径最短,效率高,Flash占用低 |
| 只需固定整数/指针 | 关闭等级、PRINT、FLOAT，开启 `LOG_ENABLE_TYPED` | 标签最长 46 B，调用栈需要固定帧 |
| 只需固定格式浮点 | 开启 `LOG_ENABLE_TYPED_FLOAT`，关闭其他格式化接口 | 固定三位且截断，64 B 帧，标签最长 46 B |
| 完全关闭日志封装 | `RTT_LOG_ENABLE=0` | 全部日志参数不执行 |
