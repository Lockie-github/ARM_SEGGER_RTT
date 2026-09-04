cmake_minimum_required(VERSION 3.20)
project(nh07_config_test C ASM)

if(NOT DEFINED ARM_SEGGER_RTT_SOURCE_DIR)
    message(FATAL_ERROR "ARM_SEGGER_RTT_SOURCE_DIR is required")
endif()

add_subdirectory(${ARM_SEGGER_RTT_SOURCE_DIR} arm_segger_rtt)

add_library(nh07_config_probe OBJECT config_probe.c)
target_link_libraries(nh07_config_probe PRIVATE arm_segger_rtt)
target_compile_definitions(nh07_config_probe PRIVATE
    EXPECT_USER_CFG=${EXPECT_USER_CFG}
    EXPECT_UP=${EXPECT_UP}
    EXPECT_DOWN=${EXPECT_DOWN}
    EXPECT_UP_SIZE=${EXPECT_UP_SIZE}
    EXPECT_DOWN_SIZE=${EXPECT_DOWN_SIZE}
    EXPECT_PRINTF_SIZE=${EXPECT_PRINTF_SIZE}
    EXPECT_LOG_INDEX=${EXPECT_LOG_INDEX}
    EXPECT_CB_SIZE=${EXPECT_CB_SIZE}
)
