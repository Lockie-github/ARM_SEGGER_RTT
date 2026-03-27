# 目录
- [目录](#目录)
- [描述](#描述)
- [移植](#移植)
  - [Make](#make)
  - [CMake](#cmake)
- [修订记录:](#修订记录)
- [更新记录](#更新记录)
  - [\[1.0.0\]](#100)
  - [\[1.0.1\]](#101)
  - [\[1.0.2\]](#102)
  - [\[2.0.0\] - 2026-03-27](#200---2026-03-27)
    - [Added](#added)
    - [Changed](#changed)

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
2. 在生成的工程的Makefile文件中加入以下代码:
```Makefile

include ARM_SEGGER_RTT/segger_rtt.mk
ALLINC := $(patsubst %,-I%,$(ALLINC))
C_SOURCES += $(ALLCSRC)
C_INCLUDES += $(ALLINC)
# compile gcc flags

# *** EOF ***

# 自动从ioc文件中提取MCU_ID,仅限STM32CubeMX(STM32CubeMX2不行)
IOC_FILE := $(wildcard *.ioc)
ifeq ($(IOC_FILE),)
  $(error 未找到 .ioc 文件)
endif

MCU_ID := $(shell awk -F'=' '/^Mcu\.CPN=/ {print substr($$2, 1, length($$2)-2); exit}' "$(IOC_FILE)")

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

MCU_ID := $(shell awk -F'=' '/^Mcu\.CPN=/ {print substr($$2, 1, length($$2)-2); exit}' "$(IOC_FILE)")

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

clean:
	-rm -fR build

```

# 修订记录:
| 文档版本 | 修订时间 | 修改内容 | 备注 |
|--|--|--|--|
|1.0.0|2026/03/37|更改了文档的结构||

---

# 更新记录
## [1.0.0]
  1. 源自于SEGGER_RTT_V864a
  2. 添加了分级日志功能,全部开启Flash占用约7.7K
     1. 添加了浮点打印支持
     2. 拥有超时机制
     3. 拥有颜色等级区分
  3. 添加了lite等级,Flash占用约3.9K
     1. 仅保留基础打印功能
     2. 该功能开启后除浮点外的分级日志将全部关闭

## [1.0.1]
  1. 添加有FPU的MCU浮点型处理逻辑,提高性能
  2. 增加浮点型NAN等特殊值的处理

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

## [2.0.0] - 2026-03-27 
### Added
  - 新增对Cmake的支持 

### Changed
  - 修改了下载脚本,兼容Cmake与Make,注:脚本已不再兼容V1.0.0

















```makefile
# *** EOF ***





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