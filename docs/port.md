# 目录

- [目录](#目录)
- [移植](#移植)
  - [Make](#make)
    - [编译和烧录](#编译和烧录)
  - [CMake](#cmake)
    - [配置、编译和烧录](#配置编译和烧录)
      - [环境准备](#环境准备)
      - [Debug](#debug)
      - [Release](#release)
  - [RTT 主机连接方式](#rtt-主机连接方式)
    - [已验证方式：J-Link](#已验证方式j-link)
    - [其他 RTT 主机工具](#其他-rtt-主机工具)
  - [迁移验收](#迁移验收)
  - [常见问题](#常见问题)
- [修订记录](#修订记录)

---

# 移植

## Make
1. 拉取本仓库到STM32CubeMX生成的Makefile工程路径下
2. 在生成的工程的Makefile文件中的指定位置分别加入以下代码:

在 CubeMX 生成的 Makefile 中，找到 C_INCLUDES 定义块，在该定义块结束后、`# compile gcc flags` 之前插入以下内容。此位置必须位于 `OBJECTS` 根据 `C_SOURCES/ASMM_SOURCES` 生成之前。

```Makefile
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

```

可选的工具:

在Makefile的fr命令后加入:

```Makefile
# ============ Flash/RAM Analysis Targets ============
.PHONY: analyze analyze-printf analyze-symbols analyze-flash

# 主分析命令：显示 printf 相关符号 + 按大小排序的符号表 + 内存摘要
analyze: analyze-flash analyze-printf analyze-symbols

# 显示 Flash/RAM 使用摘要
analyze-flash:
	@echo
	@echo "Memory Usage Summary for $(TARGET).elf"
	@$(SZ) $(BUILD_DIR)/$(TARGET).elf
	@echo
	@$(PREFIX)size -A $(BUILD_DIR)/$(TARGET).elf | grep -E "\.(text|data|bss)" | \
 		awk '{printf "  \033[0;34m%-8s\033[0m %6d bytes (%.1f KB)\n", $$1, $$2, $$2/1024}'

# 分析 printf/vsnprintf 相关符号
analyze-printf:
	@echo
	@echo "Searching for printf/vsnprintf related symbols:"
	@$(PREFIX)objdump -t $(BUILD_DIR)/$(TARGET).elf 2>/dev/null | \
		grep -i "printf\|vsnprintf" | \
		sed 's/^/   /' || echo "   \033[0;32m✓ No printf/vsnprintf symbols found.\033[0m"

# 分析最大符号（按大小排序，显示最大的20个）
 analyze-symbols:
 	@echo
 	@echo "Top 20 Largest Symbols by Size:"
 	@$(PREFIX)nm --print-size -S $(BUILD_DIR)/$(TARGET).elf 2>/dev/null | \
 		sort -k2 -g | tail -20 | \
 		awk '{printf "   \033[0;35m%6s B\033[0m | %s\n", $$2, $$4}' || echo "   \033[0;31m✗ Failed to analyze symbols (check .elf exists)\033[0m"

```

### 编译和烧录

此处描述为使用GNU编译工具进行的操作
1.  在终端输入编译指令,等待编译成功
    ``` make -j```
2. 编译完成后,输入烧录指令
    ``` make flash ```

其他操作见[其他常用命令](##RTT%20主机连接方式)

## CMake
1. 拉取本仓库到STM32CubeMX生成的CMake工程路径下
2. 在工程根目录的`CMakeLists.txt`中添加
    1. 在 `# Add STM32CubeMX generated sources`后添加

    ```CMake
    add_subdirectory(ARM_SEGGER_RTT) 
    ```

    2. 在 `# Add user defined libraries` 后,将已有的 `target_link_libraries(${CMAKE_PROJECT_NAME} ...)` 中加入`arm_segger_rtt`

    ```cmake
    target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx
        arm_segger_rtt
        # Add user defined libraries
    )
    ```

    3. 在末尾添加

    ```CMake
    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary
                $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
                $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.bin
        COMMAND ${CMAKE_OBJCOPY} -O ihex
                $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
                $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/${CMAKE_PROJECT_NAME}.hex
        COMMENT "Generating binary and hex files"
        VERBATIM
    )
    ```

3. 将本仓库提供的 `Makefile` 复制到项目根目录。可以手动复制或在项目根目录的控制台中执行：
```shell
cp ARM_SEGGER_RTT/Makefile ./Makefile
```

### 配置、编译和烧录

此处描述为使用STM32Cube插件的cube-cmake进行的操作

#### 环境准备
首次使用新工程时，STM32Cube 插件可能尚未将其识别为 STM32Cube 工程。请先完成以下操作：

1. 使用 VS Code 打开 STM32CubeMX 生成的工程根目录。
2. 如果 VS Code 弹出“是否加载为 STM32Cube 工程”的提示，请确认加载。
3. 如果没有出现提示，按 `Cmd+Shift+P`（Windows/Linux 为 `Ctrl+Shift+P`）打开命令面板，执行 `STM32Cube: Set up STM32Cube projects`，然后选择当前工程并完成设置。
4. 设置完成后关闭已有终端，并新建一个 VS Code 集成终端，使插件提供的工具路径生效。
5. 在新终端中进入工程根目录，通过包装 Makefile 完成配置和编译：

    ```shell
    make preset_debug
    make d
    ```

    这两个目标会调用插件提供的 `cube-cmake`。无需在工程外直接执行
    `cube-cmake`，也不要用系统 `cmake` 代替。

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
其他操作见[其他常用命令](##RTT%20主机连接方式)

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

`preset_debug` 和 `preset_release` 会先删除对应的构建目录再重新生成，因此修改工具链文件、生成器或重要 CMake 配置后应重新执行相应的 preset 命令；仅修改 C/C++ 源文件时，直接执行 `make d` 或 `make r` 即可。

其他操作见[其他常用命令](##RTT%20主机连接方式)

## RTT 主机连接方式

RTT 数据保存在目标 RAM 中的控制块和 Up/Down Buffer。目标侧日志代码不直接依赖
J-Link；主机侧需要能够通过调试接口访问目标内存并识别 SEGGER RTT 控制块。

### 已验证方式：J-Link

本仓库提供以下辅助命令：

| 命令 | 作用 |
|---|---|
| `make run` | 使用 J-Link Commander 连接目标并开放 RTT Telnet 端口 9999 |
| `make rtt` | 连接 `127.0.0.1:9999` 并显示通道 0 日志 |
| `make rttlog` | 为日志添加时间戳并写入文件 |
| `make erase` | 使用 J-Link 擦除目标芯片 |
| `make flash` | 使用 J-Link 烧录 Make 工程固件 |
| `make debug` | 编译并烧录 CMake Debug 固件 |
| `make release` | 编译并烧录 CMake Release 固件 |

上述命令是辅助工具，不是编译或链接 RTT 库的必要条件。

### 其他 RTT 主机工具

OpenOCD、pyOCD、probe-rs 以及部分 IDE 也可能通过 ST-Link、CMSIS-DAP 等探针读取
RTT。使用这些方案时，目标侧仍可继续使用本库的日志 API，但需要按照对应工具的说明
配置 RTT 控制块搜索、通道选择和主机输出服务。

这些替代方案目前不属于本项目的实际验证范围，本仓库也暂未提供对应启动脚本。

## 迁移验收

迁移时应固定本库版本、主工程版本和工具链版本，并从干净构建目录执行验证。

1. `git submodule status` 显示预期的 RTT tag 或提交。
2. Debug 和 Release 均能从空构建目录完成编译、链接且无新增警告。
3. ELF 中存在 `_SEGGER_RTT` 控制块及实际调用的日志实现。
4. 固件能够烧录并正常启动，无 HardFault 或异常复位。
5. 调用 `log_info("RTT_READY value=%u", 42u)` 后，主机收到：
   `[INFO] RTT_READY value=42`。
6. 断开 RTT 客户端后目标程序继续运行；重新连接后能够收到新的日志。
7. 修改 `rtt_cfg.h` 后执行 clean rebuild，配置行为与预期一致。

## 常见问题

1.  `cube-cmake: No such file or directory`

如果执行 Makefile 时出现 `make: cube-cmake: No such file or directory`，通常表示当前工程尚未完成 STM32Cube 设置，或者终端是在插件加载前创建的。执行 `STM32Cube: Set up STM32Cube projects` 后重新新建集成终端即可。

回到工程根目录，重新通过包装 Makefile 验证：

```shell
make preset_debug
make d
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

9. `ts: command not found`

`make rttts` 和 `make rttlog` 使用 `ts` 为日志添加时间戳。没有安装 `ts` 时仍可使用不带时间戳的 `make rtt`；如需时间戳功能，请安装提供 `ts` 命令的 `moreutils` 工具包。

10. 没有 J-Link 是否可以使用本库
可以。编译和链接本库不要求 J-Link，可以继续使用工程原有工具烧录固件。

但查看 RTT 输出仍需要支持 RTT 的调试探针和主机软件。本仓库目前只提供并验证
J-Link 操作流程；使用 OpenOCD、pyOCD、probe-rs 或其他工具时，需要自行完成主机侧
RTT 配置。

11. 固件可以运行，但没有 RTT 日志
    
    依次检查：
    1. 应用是否实际执行了日志调用；
    2. ELF 中是否保留 `_SEGGER_RTT` 控制块；
    3. 主机工具是否连接了正确的 MCU 和调试接口；
    4. 主机工具是否成功找到 RTT 控制块；
    5. 读取的是否为 `RTT_LOG_BUFFER_INDEX` 对应的 Up Buffer；
    6. 非零通道是否已由应用调用 `SEGGER_RTT_ConfigUpBuffer()` 完成配置；
    7. 配置变更后是否执行了全量清理和重新构建

# 修订记录

| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|
|1.0.0|2026/08/13|从 README 拆分移植说明，并添加文档目录||
