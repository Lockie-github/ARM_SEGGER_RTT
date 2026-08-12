BUILD_DIR ?= build
AS ?= arm-none-eabi-gcc
CFLAGS ?= -mcpu=cortex-m0 -mthumb

include ARM_SEGGER_RTT/segger_rtt.mk

.PHONY: all
all: $(BUILD_DIR)/SEGGER_RTT_ASM_ARMv7M.o

$(BUILD_DIR):
	mkdir -p $@
