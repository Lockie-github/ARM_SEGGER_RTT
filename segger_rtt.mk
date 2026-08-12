# List of all the board related files.
RTT_C_SOURCES = ARM_SEGGER_RTT/RTT/SEGGER_RTT.c \
				 ARM_SEGGER_RTT/rtt_printf.c \
				 ARM_SEGGER_RTT/rtt_log.c \
				 ARM_SEGGER_RTT/rtt_core.c

# Required include directories
RTT_CONFIG_DIR ?= .
RTT_INCLUDES = $(RTT_CONFIG_DIR) \
			ARM_SEGGER_RTT \
			ARM_SEGGER_RTT/RTT

EXTRA_C_SOURCES += $(RTT_C_SOURCES)
EXTRA_INCLUDES += $(RTT_INCLUDES)
