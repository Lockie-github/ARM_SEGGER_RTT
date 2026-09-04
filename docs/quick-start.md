# 快速开始指南

> 本文基于仓库当前源码整理，覆盖从环境准备到看到第一条 RTT 日志的完整流程。
> 更多细节请查阅：[移植指南](port.md)、[输出模式指南](Mode%20declaration.md)、
> [日志配置指南](Configuration.md)、[API 指南](API%20declaration.md)。

## 1. 这个库是什么

本仓库基于 SEGGER RTT 8.64a 封装了一层轻量日志接口：

| 文件 | 职责 |
|---|---|
| `RTT/` | SEGGER RTT 官方源码（含少量已记录的修复，见 readme「对源码的修改」） |
| `rtt_log.h/c` | 日志 API：等级宏 `log_info` 等、typed 类型化接口 |
| `rtt_printf.c` | 精简 printf 格式化器（无浮点、无长度修饰符） |
| `rtt_float.c` | 定点三位小数浮点输出（IEEE-754 位解析实现） |
| `rtt_cfg.h` | 配置模板：复制到应用工程后取消注释即可覆盖默认值 |
| `segger_rtt.mk` / `CMakeLists.txt` | Make / CMake 构建接入 |
| `Makefile` / `jlinkscript/` | 烧录与 RTT 查看的封装命令 |

所有日志通过 RTT Up Buffer 写入内存环形缓冲，由调试器侧读取，不占用 UART，
即使目标全速运行也不阻塞应用（非阻塞模式下缓冲满时丢弃而非等待）。

## 2. 准备环境
本库的编译和链接不依赖 J-Link。若只需要将 RTT 日志功能集成到固件，可以继续使用
主工程原有的编译、烧录和调试流程。
本文为了完成“编译、烧录、运行并看到第一条 RTT 日志”的端到端验证，使用本项目已经
实际验证的 J-Link Commander + RTT Telnet 方案。

### 编译和集成所需环境

- 一个由 STM32CubeMX 生成的 Make 或 CMake 工程；
- ARM GNU Toolchain 及工程原有构建工具；
- 目标 Cortex-M 开发板。

### 使用本文 J-Link 验证流程所需环境

- SEGGER J-Link Software and Documentation Pack；
- 命令行能够运行 `JLinkExe`；
- 一只通过 SWD 连接目标板的 J-Link 调试探针；
- `telnet` 客户端；macOS 也可使用 `nc` 连接 RTT Telnet 端口。

## 3. 引入仓库

建议作为 Git submodule 引入 STM32 工程根目录：

```shell
git submodule add https://github.com/Lockie-github/ARM_SEGGER_RTT.git ARM_SEGGER_RTT
git submodule update --init --recursive
```

## 4. 接入构建系统

按主工程类型二选一；详细说明见 [移植指南](port.md)。

### Make 工程

在 CubeMX 生成的 Makefile 中，找到 C_INCLUDES 定义块，在该定义块结束后、`# compile gcc flags` 之前插入以下内容。此位置必须位于 `OBJECTS` 根据 `C_SOURCES/ASMM_SOURCES` 生成之前。

```makefile
# C includes
#C_INCLUDES = \
#-ICore/Inc \
# ...

# RTT BEGIN Includes 
include ARM_SEGGER_RTT/segger_rtt.mk
EXTRA_INCLUDES := $(patsubst %,-I%,$(EXTRA_INCLUDES))
C_SOURCES += $(EXTRA_C_SOURCES)
C_INCLUDES += $(EXTRA_INCLUDES)
# RTT END Includes 
# compile gcc flags
CFLAGS = ...
```

再把 [移植指南](port.md#make) 中的 `info`、`flash`、`run`、`rtt` 等目标复制进去。

### CMake 工程

在根目录 `CMakeLists.txt` 中添加子目录并链接库，再附加生成 hex 的
POST_BUILD 步骤（完整片段见 [移植指南](port.md#cmake)）：

```cmake
add_subdirectory(ARM_SEGGER_RTT)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    arm_segger_rtt
)
```

然后将本仓库的 `Makefile` 复制到工程根目录，用于驱动 cube-cmake 与烧录：

```shell
cp ARM_SEGGER_RTT/Makefile ./Makefile
make preset_debug
make d
```

以上命令应在 STM32 VS Code 插件完成工程设置后，从工程根目录的集成终端执行。
由包装 Makefile 调用插件提供的 `cube-cmake`，不要改用系统 `cmake`。

## 5. 输出第一条日志

RTT 无需初始化函数，控制权到达即可调用。在 `main()` 中调用一次：

```c
#include "rtt_log.h"

int main(void)
{
  /* ... HAL_Init()、时钟和外设初始化 ... */
  log_info("hello world");
  log_debug("counter=%u", 42u);
  log_warn("voltage=%u mV", 3250u);
  log_err("status=%d", -1);
  log_float_label("temp", 1.25f);   /* 需要浮点时 */

  while (1) { }
}
```

默认配置下忽略颜色序列后的输出为：

```text
[INFO] hello world
[DEBUG] counter=42
[WARN] voltage=3250 mV
[ERROR] status=-1
temp: 1.250
```

注意格式化器是精简版：支持 `%c %d %u %x %X %s %p %%`，不支持 `%f/%e/%g`
和长度修饰符。浮点必须走 `log_float` / `log_float_label` / `log_f32`。
关闭某一级别的开关后，对应宏展开为空语句，参数不会求值。

## 6. 编译、烧录

### 编译

**Make 工程**：

```shell
make -j          # 编译
make flash       # J-Link 烧录 build/<TARGET>.hex
```

**CMake 工程**（首次或改动工具链配置后需要先执行 preset）：

```shell
make preset_debug && make d      # Debug：配置 + 编译
make debug                       # Debug：编译 + 烧录
# Release 对应 preset_release / make r / make release
```

烧录前可用 `make info` 核对自动解析出的 `MCU` 和 `TARGET`。若 `.ioc` 缺少
`ProjectManager.DeviceId` 或 CMakeLists.txt 不是标准
`set(CMAKE_PROJECT_NAME ...)` 写法，请在 Makefile 中手动设置 `MCU_ID`/`TARGET`。

### 烧录 

可使用原有的构建方式进行烧录;也可使用Jlink进行烧录:
**Make 工程**：

```shell
make flash       # J-Link 烧录 build/<TARGET>.hex
```

**CMake 工程**（首次或改动工具链配置后需要先执行 preset）：

```shell
make debug                       # Debug：编译 + 烧录
# Release 对应 make release
```

## 7. 查看 RTT 日志

本节使用 J-Link Commander 提供 RTT Telnet 服务。这是本仓库当前提供并完成实际
验证的主机连接方式，不代表 RTT 只能通过 J-Link 使用。

开两个终端，在工程根目录执行：

```shell
# 终端一：启动 J-Link，SWD 连接目标并在本机 9999 端口提供 RTT Telnet 服务
make run

# 终端二：循环连接 RTT Telnet 端口并打印日志（断线每秒重试）
make rtt
```

顺序不限：`make rtt` 会一直重试直到服务出现。也可以 `make fr`（Make 工程）
一步完成烧录并启动 J-Link。需要带时间戳落盘日志时使用 `make rttlog`
（依赖 moreutils 的 `ts`）。

## 8. 裁剪输出模式以节省 Flash

三种常用形态，全部通过编译期配置裁剪，详见 [输出模式指南](Mode%20declaration.md)：

| 模式 | 开关 | 适用场景 |
|---|---|---|
| Full（默认） | — | 开发期，带 `[LEVEL]` 前缀和 ANSI 颜色 |
| Lite | `LOG_ENABLE_LITE=1` | 省略前缀和颜色，保留全部格式化能力 |
| Typed | `LOG_ENABLE_TYPED=1` 等 | 绕过格式化器直写数值，Flash 最省 |

启用方式二选一：

1. 把 `rtt_cfg.h` 复制到应用工程（Make 默认搜索工程根目录
   `RTT_CONFIG_DIR ?=. `，CMake 默认 `${CMAKE_SOURCE_DIR}`），把
   `RTT_USER_CFG_ENABLE` 改为 1 并修改需要的项；
2. 或直接在编译选项中定义宏（如 `ARM_SEGGER_RTT_OPTIMIZATION` 同级的
   `-DLOG_ENABLE_LITE=1`）。

修改任一配置宏后需要 clean 重编 RTT 相关对象才能生效。

## 9. 常见问题速查

| 现象 | 排查 |
|---|---|
| 看不到日志 | `make run` 是否仍在运行；9999 端口是否被占用；`make info` 的 MCU/TARGET 是否正确；代码路径确实执行；相关 `LOG_ENABLE_*` 未被关闭；改过配置后是否 clean 重编 |
| `cube-cmake: No such file or directory` | 完成 STM32Cube 工程设置后新建 VS Code 集成终端，回到工程根目录重新执行 `make preset_debug` 和 `make d` |
| 提示找不到 HEX | 先成功编译，确认 POST_BUILD 生成了 hex；`find build -name '*.hex'` |
| `JLinkExe: command not found` | 安装 J-Link 软件包并把其 bin 目录加入 PATH |
| 日志中断/丢帧 | 缓冲满即丢弃是预期行为；增大 `BUFFER_SIZE_UP`（默认 1024）或降低发送频率 |

更完整的排查见 [移植指南常见问题](port.md#常见问题)。
