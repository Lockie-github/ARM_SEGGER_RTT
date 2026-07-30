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
