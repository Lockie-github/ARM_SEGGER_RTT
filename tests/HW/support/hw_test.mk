HW_TEST_CASE ?= 8
HW_TEST_PROFILE ?= 0
HW_TEST_UP_SIZE ?= 256
HW08_FLOAT_FAST ?= 0
HW08_SKIP_ASM ?= 0
HW08_GATED_THROUGHPUT ?= 0
HW08_TP_RUN_ID ?= 1
HW08_TP_BASE_TICKS ?= 3
HW08_TP_BASE_CYCLES ?= 0
HW08_TP_REMAINDER_STEP ?= 53
HW08_TP_REMAINDER_DENOM ?= 1024
HW_TEST_MCU_FAMILY ?= -1
HW_TEST_IRQ_HANDLER ?= unused
HW_TEST_IRQ_SOURCE ?= unused.c

ifndef HW_TEST_LIBRARY_SHA
$(error HW_TEST_LIBRARY_SHA must be provided by the HW test runner)
endif

HW_TEST_REPO_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../..)
HW_TEST_SUPPORT := $(HW_TEST_REPO_ROOT)/tests/HW/support
HW_TEST_LINKER_FRAGMENT := $(HW_TEST_REPO_ROOT)/tests/HW/support/rtt_sections.ld

C_SOURCES += $(HW_TEST_SUPPORT)/hw_test_entry.c
vpath %.c $(HW_TEST_SUPPORT)

$(BUILD_DIR)/hw_test_entry.o: $(HW_TEST_SUPPORT)/hw_test_entry.c Makefile | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(BUILD_DIR)/hw_test_entry.o

# The runner's repository must win over a stale ARM_SEGGER_RTT copy in the
# target project when hw_test_entry.c includes the central fixture path.
# Keep CFLAGS recursive so automatic variables such as $@ remain intact.
C_INCLUDES := -I$(HW_TEST_REPO_ROOT) $(C_INCLUDES)
# Preloading the test header activates its include guard before a normal
# application rtt_cfg.h is reached, without rewriting the project's CFLAGS.
CFLAGS += -include $(HW_TEST_SUPPORT)/rtt_cfg.h
CFLAGS += -DHW_TEST_CASE=$(HW_TEST_CASE)
CFLAGS += -DHW_TEST_PROFILE=$(HW_TEST_PROFILE)
CFLAGS += -DHW_TEST_UP_SIZE=$(HW_TEST_UP_SIZE)
CFLAGS += -DHW_TEST_MCU_FAMILY=$(HW_TEST_MCU_FAMILY)
CFLAGS += -DHW_TEST_IRQ_HANDLER=$(HW_TEST_IRQ_HANDLER)
CFLAGS += -DHW_TEST_LIBRARY_SHA=\"$(HW_TEST_LIBRARY_SHA)\"
CFLAGS += -DHW_TEST_PROJECT_MAKE=1
ifeq ($(DEBUG),1)
CFLAGS += -DHW_TEST_BUILD_DEBUG=1
else
CFLAGS += -DHW_TEST_BUILD_RELEASE=1
endif
CFLAGS += -DHW08_FLOAT_FAST=$(HW08_FLOAT_FAST)
CFLAGS += -DHW08_SKIP_ASM=$(HW08_SKIP_ASM)
CFLAGS += -DHW08_GATED_THROUGHPUT=$(HW08_GATED_THROUGHPUT)
CFLAGS += -DHW08_TP_RUN_ID=$(HW08_TP_RUN_ID)
CFLAGS += -DHW08_TP_BASE_TICKS=$(HW08_TP_BASE_TICKS)
CFLAGS += -DHW08_TP_BASE_CYCLES=$(HW08_TP_BASE_CYCLES)
CFLAGS += -DHW08_TP_REMAINDER_STEP=$(HW08_TP_REMAINDER_STEP)
CFLAGS += -DHW08_TP_REMAINDER_DENOM=$(HW08_TP_REMAINDER_DENOM)
CFLAGS += -fstack-protector-all -fstack-usage

LDFLAGS += -Wl,--wrap=SEGGER_RTT_Write
LDFLAGS += -Wl,--undefined=HW_TestEntry
LDFLAGS += -T$(HW_TEST_LINKER_FRAGMENT)
ifeq ($(HW_TEST_CASE),5)
LDFLAGS += -Wl,--wrap=SEGGER_RTT_printf
endif

ifeq ($(HW_TEST_CASE),7)
HW_TEST_IRQ_OBJECT := $(BUILD_DIR)/$(basename $(notdir $(HW_TEST_IRQ_SOURCE))).o
$(HW_TEST_IRQ_OBJECT): CFLAGS += -D$(HW_TEST_IRQ_HANDLER)=HW_TestProjectIRQHandler
endif
