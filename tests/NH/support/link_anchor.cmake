function(nh_apply_link_anchor)
    set(app_target "${CMAKE_PROJECT_NAME}")
    if(NOT TARGET "${app_target}")
        message(FATAL_ERROR "NH link anchor target does not exist: ${app_target}")
    endif()
    target_link_options("${app_target}" PRIVATE
        -Wl,--undefined=SEGGER_RTT_WriteNoLock
    )
endfunction()

cmake_language(DEFER CALL nh_apply_link_anchor)
