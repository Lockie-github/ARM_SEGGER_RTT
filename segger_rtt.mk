# List of all the board related files.
RTTSRC = ARM_SEGGER_RTT/SEGGER_RTT.c \
		 ARM_SEGGER_RTT/SEGGER_RTT_printf.c

# Required include directories
RTTINC = ARM_SEGGER_RTT

ALLCSRC += $(RTTSRC)
ALLINC += $(RTTINC)
