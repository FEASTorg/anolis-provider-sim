# First-party compiler warning policy for anolis-provider-sim.
# Generated protobuf/gRPC sources are handled separately in CMakeLists.txt.

function(anolis_provider_sim_apply_warnings target_name)
    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "anolis_provider_sim_apply_warnings: unknown target '${target_name}'")
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
        if(ANOLIS_PROVIDER_SIM_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
        if(ANOLIS_PROVIDER_SIM_WARNINGS_AS_ERRORS)
            target_compile_options(${target_name} PRIVATE -Werror)
        endif()
    endif()
endfunction()
