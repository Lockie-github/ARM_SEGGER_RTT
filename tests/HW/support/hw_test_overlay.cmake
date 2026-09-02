if(NOT DEFINED HW_TEST_REPO_ROOT)
    message(FATAL_ERROR "HW_TEST_REPO_ROOT is required")
endif()
if(NOT DEFINED HW_TEST_MCU_FAMILY)
    message(FATAL_ERROR "HW_TEST_MCU_FAMILY is required")
endif()
if(NOT DEFINED HW_TEST_IRQ_HANDLER)
    message(FATAL_ERROR "HW_TEST_IRQ_HANDLER is required")
endif()
if(NOT DEFINED HW_TEST_LIBRARY_SHA)
    message(FATAL_ERROR "HW_TEST_LIBRARY_SHA is required")
endif()

function(hw_test_apply_overlay)
    set(app_target "${CMAKE_PROJECT_NAME}")
    if(NOT TARGET "${app_target}")
        message(FATAL_ERROR "HW test application target does not exist: ${app_target}")
    endif()
    if(NOT TARGET arm_segger_rtt)
        message(FATAL_ERROR "HW test requires the arm_segger_rtt target")
    endif()

    set(hw_support "${HW_TEST_REPO_ROOT}/tests/HW/support")
    set(hw_definitions
        HW_TEST_CASE=${HW_TEST_CASE}
        HW_TEST_PROFILE=${HW_TEST_PROFILE}
        HW_TEST_UP_SIZE=${HW_TEST_UP_SIZE}
        HW_TEST_MCU_FAMILY=${HW_TEST_MCU_FAMILY}
        HW_TEST_IRQ_HANDLER=${HW_TEST_IRQ_HANDLER}
        HW_TEST_LIBRARY_SHA="${HW_TEST_LIBRARY_SHA}"
        HW08_FLOAT_FAST=${HW08_FLOAT_FAST}
        HW08_SKIP_ASM=${HW08_SKIP_ASM}
        HW08_GATED_THROUGHPUT=${HW08_GATED_THROUGHPUT}
        HW08_TP_RUN_ID=${HW08_TP_RUN_ID}
        HW08_TP_BASE_TICKS=${HW08_TP_BASE_TICKS}
        HW08_TP_BASE_CYCLES=${HW08_TP_BASE_CYCLES}
        HW08_TP_REMAINDER_STEP=${HW08_TP_REMAINDER_STEP}
        HW08_TP_REMAINDER_DENOM=${HW08_TP_REMAINDER_DENOM}
        $<$<CONFIG:Debug>:HW_TEST_BUILD_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:HW_TEST_BUILD_RELEASE=1>
    )

    target_sources("${app_target}" PRIVATE "${hw_support}/hw_test_entry.c")
    target_include_directories("${app_target}" BEFORE PRIVATE "${hw_support}")
    target_include_directories("${app_target}" PRIVATE "${HW_TEST_REPO_ROOT}")
    target_include_directories(arm_segger_rtt BEFORE PRIVATE "${hw_support}")
    target_compile_definitions("${app_target}" PRIVATE ${hw_definitions})
    target_compile_definitions(arm_segger_rtt PRIVATE ${hw_definitions})
    target_compile_options("${app_target}" PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-fstack-protector-all>
        $<$<COMPILE_LANGUAGE:C>:-fstack-usage>
    )
    target_compile_options(arm_segger_rtt PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-fstack-protector-all>
        $<$<COMPILE_LANGUAGE:C>:-fstack-usage>
    )
    target_link_options("${app_target}" PRIVATE
        -Wl,--undefined=HW_TestEntry
        -Wl,--wrap=SEGGER_RTT_Write
    )
    if(HW_TEST_CASE EQUAL 5)
        target_link_options("${app_target}" PRIVATE -Wl,--wrap=SEGGER_RTT_printf)
    endif()
    if(HW_TEST_CASE EQUAL 7)
        get_target_property(hw_app_sources "${app_target}" SOURCES)
        set(hw_irq_source "")
        foreach(hw_source IN LISTS hw_app_sources)
            if(hw_source MATCHES "_it\\.c$")
                set(hw_irq_source "${hw_source}")
                break()
            endif()
        endforeach()
        if(NOT hw_irq_source)
            message(FATAL_ERROR "HW07 could not locate the project IRQ source")
        endif()
        set_source_files_properties("${hw_irq_source}"
            TARGET_DIRECTORY "${app_target}"
            PROPERTIES COMPILE_DEFINITIONS
                "${HW_TEST_IRQ_HANDLER}=HW_TestProjectIRQHandler"
        )
    endif()
    if(HW_TEST_MCU_FAMILY EQUAL 7)
        target_link_options("${app_target}" PRIVATE
            "-T${hw_support}/rtt_sections.ld"
        )
    endif()
endfunction()

cmake_language(DEFER CALL hw_test_apply_overlay)
