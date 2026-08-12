# 目录
- [目录](#目录)
- [描述](#描述)
- [移植](#移植)
  - [Make](#make)
  - [CMake](#cmake)
    - [配置、编译和烧录](#配置编译和烧录)
      - [Debug](#debug)
      - [Release](#release)
      - [其他常用命令](#其他常用命令)
    - [常见问题](#常见问题)
- [日志配置](#日志配置)
  - [格式化支持](#格式化支持)
- [修订记录:](#修订记录)
- [更新记录](#更新记录)
  - [\[2.1.0\] - 2026-07-30](#210---2026-07-30)
    - [Added](#added)
    - [Changed](#changed)
  - [\[2.0.2\] - 2026-07-01](#202---2026-07-01)
    - [Fixed](#fixed)
  - [\[2.0.1\] - 2026-04-07](#201---2026-04-07)
    - [Changed](#changed-1)
  - [\[2.0.0\] - 2026-03-27](#200---2026-03-27)
    - [Added](#added-1)
    - [Changed](#changed-2)
  - [\[1.0.2\]](#102)
  - [\[1.0.1\]](#101)
  - [\[1.0.0\]](#100)

---

# 描述
1. 这是从SEGGER官网获取的8.64a版本的RTT文件,用于查看调试日志
2. 引入了jlink烧录、擦除的脚本,两个文件都位于`ARM_SEGGER_RTT/jlinkscript`
3. 适配了STM32CubeMX生成的make工程,移植请看[Make](#make)
4. 适配了STM32CubeMX生成的cmake工程(cube-Cmake STM32 for vscode插件内置的cmake),移植请看[CMake](#cmake)
5. 建议直接引用本文件为submodbule

---

# 移植
## Make
1. 拉取本仓库到STM32CubeMX生成的Makefile工程路径下
2. 在生成的工程的Makefile文件中的指定位置分别加入以下代码:
```Makefile

include ARM_SEGGER_RTT/segger_rtt.mk
EXTRA_INCLUDES := $(patsubst %,-I%,$(EXTRA_INCLUDES))
C_SOURCES += $(EXTRA_C_SOURCES)
C_INCLUDES += $(EXTRA_INCLUDES)
# compile gcc flags

# segger_rtt.mk 会把 RTT 的大写 .S 汇编源加入 ASMM_SOURCES，
# 并提供对应的源文件搜索路径。

# *** EOF ***

# 自动从ioc文件中提取MCU_ID,仅限STM32CubeMX(STM32CubeMX2不行)
IOC_FILE := $(wildcard *.ioc)
ifeq ($(IOC_FILE),)
  $(error 未找到 .ioc 文件)
endif

MCU_ID := $(shell awk -F'=' '/^ProjectManager\.DeviceId=/ { \
  v=$$2; \
  sub(/[A-Z]x$$/, "", v); \
  print v; \
  exit \
}' "$(IOC_FILE)")

ifeq ($(MCU_ID),)
  $(error 提取 MCU 型号失败)
endif

# 若自动不行就手动配置
# MCU_ID = 

# ifeq ($(MCU_ID),)
#     $(error 请配置 MCU 值)
# endif

info:
	@echo "MCU: $(MCU_ID)"
	@echo "TARGET: $(TARGET)"

erase:
	@echo "Erase chip..."
	-JLinkExe  -Device $(MCU_ID) -CommandFile ./ARM_SEGGER_RTT/jlinkscript/erase.jlink

run:
	@echo "Try to run MCU"
	-JLinkExe  -Device $(MCU_ID) -if SWD -Speed 24000 -RTTTelnetPort 9999 -autoconnect 1

rtt:
	@echo "rtt..."
	while true; do sleep 1; telnet 127.0.0.1 9999; done

# nc版本,仅限MacOS
# 	@echo "Starting RTT client (nc)..."
# 	@while true; do \
# 		echo "Connecting to RTT..."; \
# 		nc 127.0.0.1 9999 || echo "Connection lost. Retrying in 1s..."; \
# 		sleep 1; \
# 	done

rttts:
	@make rtt | ts '%H:%M:%S'

LOGDIR := logs

RTT_LOGFILE := $(LOGDIR)/$(shell date +%Y%m%d_%H%M%S).log

rttlog:
	@mkdir -p "$(LOGDIR)"
	@make rtt | ts '%Y-%m-%d %H:%M:%S' | tee "$(RTT_LOGFILE)"
.PHONY: rttlog

flash: all
	@echo "Uploading to firmware..."
	@sed -e "s|{{BUILD_DIR}}|$(BUILD_DIR)|g" \
	     -e "s|{{TARGET}}|$(TARGET)|g" \
	     ./ARM_SEGGER_RTT/jlinkscript/flash.jlink > $(BUILD_DIR)/flash.jlink
	JLinkExe -Device $(MCU_ID) -CommandFile $(BUILD_DIR)/flash.jlink

fr:
	@echo "flash & run"
	$(MAKE) flash
	$(MAKE) run
# ============ Flash/RAM Analysis Targets ============
# .PHONY: analyze analyze-printf analyze-symbols analyze-flash

# # 主分析命令：显示 printf 相关符号 + 按大小排序的符号表 + 内存摘要
# analyze: analyze-flash analyze-printf analyze-symbols

# # 显示 Flash/RAM 使用摘要
# analyze-flash:
# 	@echo
# 	@echo "Memory Usage Summary for $(TARGET).elf"
# 	@$(SZ) $(BUILD_DIR)/$(TARGET).elf
# 	@echo
# 	@$(PREFIX)size -A $(BUILD_DIR)/$(TARGET).elf | grep -E "\.(text|data|bss)" | \
# 		awk '{printf "  \033[0;34m%-8s\033[0m %6d bytes (%.1f KB)\n", $$1, $$2, $$2/1024}'

# # 分析 printf/vsnprintf 相关符号
# analyze-printf:
# 	@echo
# 	@echo "Searching for printf/vsnprintf related symbols:"
# 	@$(PREFIX)objdump -t $(BUILD_DIR)/$(TARGET).elf 2>/dev/null | \
# 		grep -i "printf\|vsnprintf" | \
# 		sed 's/^/   /' || echo "   \033[0;32m✓ No printf/vsnprintf symbols found.\033[0m"

# # 分析最大符号（按大小排序，显示最大的20个）
# analyze-symbols:
# 	@echo
# 	@echo "Top 20 Largest Symbols by Size:"
# 	@$(PREFIX)nm --print-size -S $(BUILD_DIR)/$(TARGET).elf 2>/dev/null | \
# 		sort -k2 -g | tail -20 | \
# 		awk '{printf "   \033[0;35m%6s B\033[0m | %s\n", $$2, $$4}' || echo "   \033[0;31m✗ Failed to analyze symbols (check .elf exists)\033[0m"
```

## CMake
1. 拉取本仓库到STM32CubeMX生成的CMake工程路径下
2. 在工程根目录的`CMakeLists.txt`中添加
    1. 在 `# Add STM32CubeMX generated sources`后添加
    ```CMake
    add_subdirectory(ARM_SEGGER_RTT) 
    ```
    2. 在 `# Add user defined libraries`后添加
    ```CMake
    arm_segger_rtt
    ``` 
    3. 在末尾添加
    ```CMake
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
        COMMENT "Generating binary and hex files"
        BYPRODUCTS ${CMAKE_PROJECT_NAME}.bin ${CMAKE_PROJECT_NAME}.hex
        VERBATIM
        message("Build type: " ${CMAKE_BUILD_TYPE})
    )
    ```
3. 复制文件夹内的Makefile文件到项目根目录下或在根目录touch一个Makefile文件并添加以下源码: 
```Makefile
BUILD_DIR = build

# 自动从ioc文件中提取MCU_ID,仅限STM32CubeMX(STM32CubeMX2不行)
IOC_FILE := $(wildcard *.ioc)
ifeq ($(IOC_FILE),)
  $(error 未找到 .ioc 文件)
endif

MCU_ID := $(shell awk -F'=' '/^ProjectManager\.DeviceId=/ { \
  v=$$2; \
  sub(/[A-Z]x$$/, "", v); \
  print v; \
  exit \
}' "$(IOC_FILE)")

ifeq ($(MCU_ID),)
  $(error 提取 MCU 型号失败)
endif

# 若自动不行就手动配置
# MCU_ID = 

# ifeq ($(MCU_ID),)
#     $(error 请配置 MCU 值)
# endif

# 从CMakeLists.txt中提取hex文件名
CMAKE_LISTS := CMakeLists.txt

TARGET := $(shell grep "set(CMAKE_PROJECT_NAME" $(CMAKE_LISTS) 2>/dev/null | \
         sed 's/set(CMAKE_PROJECT_NAME \([^)]*\))/\1/')

ifeq ($(TARGET),)
    $(warning CMAKE_PROJECT_NAME not found in CMakeLists.txt)
    TARGET := unknown
endif

# 若自动不行就手动配置
# TARGET = 

# ifeq ($(TARGET),)
#     $(error 请配置 TARGET 值)
# endif

info:
	@echo "MCU: $(MCU_ID)"
	@echo "TARGET: $(TARGET)"
erase:
	@echo "Erase chip..."
	-JLinkExe  -Device $(MCU_ID) -CommandFile ./ARM_SEGGER_RTT/jlinkscript/erase.jlink

run:
	@echo "Try to run MCU"
	-JLinkExe  -Device $(MCU_ID) -if SWD -Speed 2400 -RTTTelnetPort 9999 -autoconnect 1

rtt:
	@echo "rtt..."
	while true; do sleep 1; telnet 127.0.0.1 9999; done

# nc版本,仅限MacOS
# 	@echo "Starting RTT client (nc)..."
# 	@while true; do \
# 		echo "Connecting to RTT..."; \
# 		nc 127.0.0.1 9999 || echo "Connection lost. Retrying in 1s..."; \
# 		sleep 1; \
# 	done

rttts:
	@make rtt | ts '%H:%M:%S'

LOGDIR := logs

RTT_LOGFILE := $(LOGDIR)/$(shell date +%Y%m%d_%H%M%S).log

rttlog:
	@mkdir -p "$(LOGDIR)"
	@make rtt | ts '%Y-%m-%d %H:%M:%S' | tee "$(RTT_LOGFILE)"
.PHONY: rttlog

# 构建Debug配置
preset_debug:
	-rm -fR $(BUILD_DIR)/Debug 
	cube-cmake \
		  -DCMAKE_BUILD_TYPE=Debug \
	      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
	      -S . \
	      -B $(BUILD_DIR)/Debug \
	      -G Ninja

# 构建Release配置
preset_release:
	-rm -fR $(BUILD_DIR)/Release 
	cube-cmake \
		  -DCMAKE_BUILD_TYPE=Release \
	      -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake \
	      -S . \
	      -B $(BUILD_DIR)/Release \
	      -G Ninja
# 编译Debug配置
d:
	cube-cmake --build $(BUILD_DIR)/Debug --target clean --
	cube-cmake --build $(BUILD_DIR)/Debug --target all --

# 编译Release配置
r:
	cube-cmake --build $(BUILD_DIR)/Release --target clean --
	cube-cmake --build $(BUILD_DIR)/Release --target all --

debug:d
	@echo "Uploading to firmware..."
	@sed -e "s|{{BUILD_DIR}}|$(BUILD_DIR)/Debug|g" \
	     -e "s|{{TARGET}}|$(TARGET)|g" \
	     ./ARM_SEGGER_RTT/jlinkscript/flash.jlink > $(BUILD_DIR)/Debug/flash.jlink
	JLinkExe -Device $(MCU_ID) -CommandFile $(BUILD_DIR)/Debug/flash.jlink

release:r
	@echo "Uploading to firmware..."
	@sed -e "s|{{BUILD_DIR}}|$(BUILD_DIR)/Release|g" \
	     -e "s|{{TARGET}}|$(TARGET)|g" \
	     ./ARM_SEGGER_RTT/jlinkscript/flash.jlink > $(BUILD_DIR)/Release/flash.jlink
	JLinkExe -Device $(MCU_ID) -CommandFile $(BUILD_DIR)/Release/flash.jlink

dr:debug
	make run

rr:release
	make run

clean:
	-rm -fR build

```

### 配置、编译和烧录

首次使用新工程时，STM32Cube 插件可能尚未将其识别为 STM32Cube 工程。请先完成以下操作：

1. 使用 VS Code 打开 STM32CubeMX 生成的工程根目录。
2. 如果 VS Code 弹出“是否加载为 STM32Cube 工程”的提示，请确认加载。
3. 如果没有出现提示，按 `Cmd+Shift+P`（Windows/Linux 为 `Ctrl+Shift+P`）打开命令面板，执行 `STM32Cube: Set up STM32Cube projects`，然后选择当前工程并完成设置。
4. 设置完成后关闭已有终端，并新建一个 VS Code 集成终端，使插件提供的工具路径生效。
5. 在新终端中确认 `cube-cmake` 可用：

```shell
command -v cube-cmake
cube-cmake --version
```

以下命令均在工程根目录的 VS Code 集成终端中执行。

#### Debug

首次构建或 CMake 配置发生变化后，先生成 Debug 构建目录：

```shell
make preset_debug
```

然后编译 Debug 固件：

```shell
make d
```

也可以连续完成配置和编译：

```shell
make preset_debug && make d
```

编译并通过 J-Link 烧录 Debug 固件：

```shell
make debug
```

`make debug` 会先执行 Debug 编译，再生成 J-Link 下载脚本并烧录固件。它要求已经执行过 `make preset_debug`，并且系统中可以找到 `JLinkExe`。

#### Release

生成 Release 构建目录并编译：

```shell
make preset_release
make r
```

也可以连续执行：

```shell
make preset_release && make r
```

编译并通过 J-Link 烧录 Release 固件：

```shell
make release
```

#### 其他常用命令

```shell
make info       # 显示从 .ioc 和 CMakeLists.txt 中解析出的 MCU 与目标名称
make clean      # 删除整个 build 目录
make erase      # 使用 J-Link 擦除芯片
make run        # 启动 J-Link 并连接 RTT
make rtt        # 连接 RTT Telnet 端口
make rttlog     # 将带时间戳的 RTT 输出保存到 logs 目录
```

`preset_debug` 和 `preset_release` 会先删除对应的构建目录再重新生成，因此修改工具链文件、生成器或重要 CMake 配置后应重新执行相应的 preset 命令；仅修改 C/C++ 源文件时，直接执行 `make d` 或 `make r` 即可。

### 常见问题

1.  `cube-cmake: No such file or directory`

如果执行 Makefile 时出现 `make: cube-cmake: No such file or directory`，通常表示当前工程尚未完成 STM32Cube 设置，或者终端是在插件加载前创建的。执行 `STM32Cube: Set up STM32Cube projects` 后重新新建集成终端即可。

可以使用以下命令确认当前终端是否能够找到插件提供的 CMake：

```shell
command -v cube-cmake
cube-cmake --version
```

2. 执行 `make d` 或 `make r` 时提示构建目录不存在

`make d` 和 `make r` 只负责编译已经配置好的构建目录。新工程、执行过 `make clean`，或者相应构建目录被删除后，需要先生成构建目录：

```shell
make preset_debug    # 对应 make d
make preset_release  # 对应 make r
```

3. CMake 提示找不到 Ninja 或 ARM GCC

如果出现 `CMAKE_MAKE_PROGRAM is not set`、`Ninja not found` 或找不到 `arm-none-eabi-gcc`，请确认 STM32Cube 工程设置中已经安装并选择 Ninja 与 GNU Tools for STM32。完成设置后重新新建 VS Code 集成终端，再检查工具是否可用：

```shell
ninja --version
arm-none-eabi-gcc --version
```

如果更换过工具链版本，请重新执行 `make preset_debug` 或 `make preset_release`，不要继续使用旧的 CMake 缓存。

4. Makefile 提示未找到 `.ioc` 文件或提取 MCU 型号失败

Makefile 必须在包含 `.ioc` 文件的工程根目录执行，并通过 `.ioc` 文件中的 `ProjectManager.DeviceId` 自动获取 J-Link 设备名称。请先确认当前目录和解析结果：

```shell
pwd
ls *.ioc
make info
```

部分 STM32CubeMX 版本或芯片生成的 `.ioc` 文件可能没有可用的 `ProjectManager.DeviceId`。此时需要在工程根目录的 Makefile 中手动设置 `MCU_ID`，其值应使用 J-Link 支持的设备名称。

5. `make info` 显示 `TARGET: unknown`

Makefile 会从工程根目录的 `CMakeLists.txt` 中解析以下配置：

```cmake
set(CMAKE_PROJECT_NAME your_project_name)
```

如果工程使用了不同写法，自动解析可能失败。请保持上述格式，或者在 Makefile 中手动设置 `TARGET`。`TARGET` 必须与最终生成的 `.elf`、`.hex` 文件名一致。

6. `JLinkExe: command not found`

`make debug`、`make release`、`make erase` 和 `make run` 都依赖 SEGGER J-Link。请先安装 J-Link Software and Documentation Pack，并确保 `JLinkExe` 已加入 `PATH`：

```shell
command -v JLinkExe
JLinkExe -version
```

7. 烧录时提示找不到 HEX 文件

请确认编译已经成功，并且工程根目录的 `CMakeLists.txt` 已按本章节说明添加生成 `.hex` 文件的 `add_custom_command`。然后执行：

```shell
make info
find build -name '*.hex'
```

如果 `make info` 显示的 `TARGET` 与实际 HEX 文件名不同，请修正 Makefile 中的 `TARGET` 后重新烧录。

8. RTT 提示连接被拒绝或一直无法连接

`make rtt` 只连接本机的 RTT Telnet 端口，不会自行启动 J-Link。请先在一个终端执行 `make run` 并保持其运行，再在另一个终端执行 `make rtt`。同时确认开发板已连接、`MCU_ID` 正确，并且端口 `9999` 没有被其他程序占用。

8. `ts: command not found`

`make rttts` 和 `make rttlog` 使用 `ts` 为日志添加时间戳。没有安装 `ts` 时仍可使用不带时间戳的 `make rtt`；如需时间戳功能，请安装提供 `ts` 命令的 `moreutils` 工具包。

# 日志配置

日志配置分为三级：

1. `RTT_LOG_ENABLE` 是总开关。设置为 `0` 时所有 `log_*` 宏均不输出，
   且宏参数不会被求值。
2. `LOG_ENABLE_LITE` 是轻量模式开关。设置为 `1` 时，已启用的日志只
   输出正文和换行，不输出颜色及等级前缀。
3. `LOG_ENABLE_INFO`、`LOG_ENABLE_DEBUG`、`LOG_ENABLE_WARN`、
   `LOG_ENABLE_ERROR`、`LOG_ENABLE_PRINT` 和 `LOG_ENABLE_FLOAT` 分别控制
   各类日志，在完整模式和轻量模式下都独立生效。

默认配置为开启总开关、关闭轻量模式，并开启所有单项日志。默认使用
RTT Up Buffer 0 并启用 ANSI 颜色。

使用 submodule 集成时，推荐把配置模板复制到主工程根目录：

```shell
cp ARM_SEGGER_RTT/rtt_cfg.h ./rtt_cfg.h
```

然后在主工程的 `rtt_cfg.h` 中取消所需配置项的注释并修改。例如：

```c
#define RTT_LOG_ENABLE       1
#define LOG_ENABLE_LITE      1
#define LOG_ENABLE_DEBUG     0
#define RTT_LOG_BUFFER_INDEX 1u
#define RTT_LOG_USE_COLOR    0
```

CMake 和 `segger_rtt.mk` 都会优先搜索主工程根目录，因此该文件可以覆盖
子模块中 `rtt_log.h` 的默认配置，无需修改或提交子模块内容。CMake 工程
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

应用源码与 `rtt_log.c` 必须使用同一个配置目录，仓库提供的 CMake 和 Make
集成已经保证这一点。若使用命令行 `-D` 临时配置，请不要在项目级
`rtt_cfg.h` 中重复定义同一个宏。

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

整数格式支持 `-`、`0`、`+` 标志、数字字段宽度和数字精度，例如 `%+d`、
`%-5d`、`%08x` 和 `%.4d`。字符串支持数字精度和动态精度 `.*`，例如
`%.3s` 和 `%.*s`。字段宽度不支持 `*`，也不支持 `#`、空格标志、长度
修饰符（如 `h`、`l`、`ll`、`z`）以及浮点转换符（如 `%f`、`%e`、
`%g`）；浮点值请使用 `log_float` 或 `log_float_desc`。

未支持或无法识别的转换会按格式字符串中的原文本输出。调用方不应为这类
转换传入配套参数，因为精简格式化器不会消费该参数。编译器提供的
`printf` 格式检查只用于发现参数类型错误，并不表示本实现支持标准
`printf` 的全部格式。

# 修订记录:
| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|
|1.1.0|2026/07/30|完善 CMake 构建、烧录和常见问题说明，修订记录与更新记录改为倒序排列||
|1.0.1|2026/04/07|修改了移植描述,[位于移植/Make/2.](#make)||
|1.0.0|2026/03/27|更改了文档的结构||

---

# 更新记录
## [2.1.0] - 2026-07-30
### Added
  - 新增`dr`和`rr`目标,支持Debug/Release固件编译、烧录后直接运行
  - 新增STM32Cube CMake工程配置、编译、烧录说明和常见问题章节

### Changed
  - 修订记录和更新记录改为倒序排列,优先显示最新版本

## [2.0.2] - 2026-07-01
### Fixed
  - 修复当芯片型号带特殊版本后缀时自动获取MCU_ID错误的bug

## [2.0.1] - 2026-04-07
### Changed
  - 修改了segger_rtt.mk的变量命名,语义表达更清晰,风格与ST更相近

## [2.0.0] - 2026-03-27
### Added
  - 新增对Cmake的支持

### Changed
  - 修改了下载脚本,兼容Cmake与Make,注:脚本已不再兼容V1.0.0

## [1.0.2]
  1. 添加了带时间戳的日志,需要安装ts工具
  2. 若不方便安装ts 也可使用bash或gwk
   ```bash
    make rtt | while IFS= read -r line; do
    echo "$(date '+%Y-%m-%d %H:%M:%S') $line"
    done | tee  "$(RTT_LOGFILE)"
   ```
    ```
    make rtt | gawk '{ print strftime("%Y-%m-%d %H:%M:%S"), $0 }' | tee  "$(RTT_LOGFILE)"
    ```

## [1.0.1]
  1. 添加有FPU的MCU浮点型处理逻辑,提高性能
  2. 增加浮点型NAN等特殊值的处理

## [1.0.0]
  1. 源自于SEGGER_RTT_V864a
  2. 添加了分级日志功能,全部开启Flash占用约7.7K
     1. 添加了浮点打印支持
     2. 拥有超时机制
     3. 拥有颜色等级区分
  3. 添加了lite等级,Flash占用约3.9K
     1. 仅保留基础打印功能
     2. 该功能开启后除浮点外的分级日志将全部关闭
