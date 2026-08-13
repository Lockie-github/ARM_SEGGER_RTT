# 目录

- [目录](#目录)
- [移植](#移植)
  - [Make](#make)
  - [CMake](#cmake)
    - [配置、编译和烧录](#配置编译和烧录)
      - [Debug](#debug)
      - [Release](#release)
      - [其他常用命令](#其他常用命令)
    - [常见问题](#常见问题)
- [修订记录](#修订记录)

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

1. 将本仓库提供的 `Makefile` 复制到项目根目录。可以手动复制或在项目根目录的控制台中执行：
```shell
cp ARM_SEGGER_RTT/Makefile ./Makefile
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

9. `ts: command not found`

`make rttts` 和 `make rttlog` 使用 `ts` 为日志添加时间戳。没有安装 `ts` 时仍可使用不带时间戳的 `make rtt`；如需时间戳功能，请安装提供 `ts` 命令的 `moreutils` 工具包。

# 修订记录

| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|
|1.0.0|2026/08/13|从 README 拆分移植说明，并添加文档目录||
