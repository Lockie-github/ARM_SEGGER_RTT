BUILD_DIR ?= build
PREFIX ?= /opt/arm-gnu-toolchain-12.2.rel1-darwin-arm64-arm-none-eabi/bin/arm-none-eabi-
CC := $(PREFIX)gcc
AS := $(PREFIX)gcc
CFLAGS := -mcpu=cortex-m0 -mthumb -std=c11 -Wall -Wextra -Werror \
          -ffunction-sections -fdata-sections -MMD -MP

RTT_CONFIG_DIR ?= .
include ARM_SEGGER_RTT/segger_rtt.mk

CFLAGS += $(patsubst %,-I%,$(EXTRA_INCLUDES)) \
          -DEXPECT_USER_CFG=$(EXPECT_USER_CFG) \
          -DEXPECT_UP=$(EXPECT_UP) -DEXPECT_DOWN=$(EXPECT_DOWN) \
          -DEXPECT_UP_SIZE=$(EXPECT_UP_SIZE) \
          -DEXPECT_DOWN_SIZE=$(EXPECT_DOWN_SIZE) \
          -DEXPECT_PRINTF_SIZE=$(EXPECT_PRINTF_SIZE) \
          -DEXPECT_LOG_INDEX=$(EXPECT_LOG_INDEX) \
          -DEXPECT_CB_SIZE=$(EXPECT_CB_SIZE)

OBJECTS := $(BUILD_DIR)/SEGGER_RTT.o \
           $(BUILD_DIR)/rtt_printf.o \
           $(BUILD_DIR)/rtt_log.o \
           $(BUILD_DIR)/rtt_float.o \
           $(BUILD_DIR)/SEGGER_RTT_ASM_ARMv7M.o \
           $(BUILD_DIR)/config_probe.o

.PHONY: all clean
all: $(BUILD_DIR)/libnh07.a

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/SEGGER_RTT.o: ARM_SEGGER_RTT/RTT/SEGGER_RTT.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rtt_printf.o: ARM_SEGGER_RTT/rtt_printf.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rtt_log.o: ARM_SEGGER_RTT/rtt_log.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/rtt_float.o: ARM_SEGGER_RTT/rtt_float.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/config_probe.o: config_probe.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libnh07.a: $(OBJECTS)
	$(PREFIX)ar rcs $@ $(OBJECTS)

clean:
	rm -rf $(BUILD_DIR)

-include $(OBJECTS:.o=.d)
