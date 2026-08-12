add_library(arm_segger_rtt STATIC
    RTT/SEGGER_RTT.c
    rtt_printf.c
    rtt_log.c
    rtt_core.c
    RTT/SEGGER_RTT_ASM_ARMv7M.S
)

set(ARM_SEGGER_RTT_OPTIMIZATION "-Os" CACHE STRING
    "Optimization option used for ARM SEGGER RTT C sources")

if(ARM_SEGGER_RTT_OPTIMIZATION AND
   CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(arm_segger_rtt PRIVATE
        $<$<COMPILE_LANGUAGE:C>:${ARM_SEGGER_RTT_OPTIMIZATION}>
    )
endif()

set(ARM_SEGGER_RTT_CONFIG_DIR "${CMAKE_SOURCE_DIR}" CACHE PATH
    "Directory containing the application rtt_cfg.h")

target_include_directories(arm_segger_rtt BEFORE PUBLIC
    ${ARM_SEGGER_RTT_CONFIG_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/RTT
)
