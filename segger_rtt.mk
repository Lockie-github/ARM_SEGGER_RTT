# List of all the board related files.
RTT_C_SOURCES = ARM_SEGGER_RTT/RTT/SEGGER_RTT.c \
				 ARM_SEGGER_RTT/rtt_printf.c \
				 ARM_SEGGER_RTT/rtt_log.c \
				 ARM_SEGGER_RTT/rtt_float.c

RTT_ASM_SOURCES = ARM_SEGGER_RTT/RTT/SEGGER_RTT_ASM_ARMv7M.S

# Optimize only the RTT C objects for size. Applications may override this
# before including this file, or set it to an empty value to inherit CFLAGS.
ARM_SEGGER_RTT_OPTIMIZATION ?= -Os
RTT_C_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(RTT_C_SOURCES:.c=.o)))
$(RTT_C_OBJECTS): CFLAGS += $(ARM_SEGGER_RTT_OPTIMIZATION)

# Required include directories
RTT_CONFIG_DIR ?= .
RTT_INCLUDES = $(RTT_CONFIG_DIR) \
			ARM_SEGGER_RTT \
			ARM_SEGGER_RTT/RTT

EXTRA_C_SOURCES += $(RTT_C_SOURCES)
EXTRA_INCLUDES += $(RTT_INCLUDES)
ASMM_SOURCES += $(RTT_ASM_SOURCES)

$(BUILD_DIR)/%.o: ARM_SEGGER_RTT/RTT/%.S | $(BUILD_DIR)
	$(AS) -c $(CFLAGS) $< -o $@
