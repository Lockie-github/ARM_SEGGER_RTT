add_library(arm_segger_rtt STATIC
    RTT/SEGGER_RTT.c
    RTT/rtt_printf.c
    RTT/rtt_log.c
    RTT/rtt_core.c
    RTT/SEGGER_RTT_ASM_ARMv7M.S
)

target_include_directories(arm_segger_rtt PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/RTT 
)
