# Optimize.cmake (clean & practical)

include_guard(GLOBAL)

function(apply_optimization_flags target)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")

        # ---- Base flags ----
        target_compile_options(${target} PRIVATE
            -fstrict-aliasing
        )

        # ---- Debug / Release configuration ----
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-Og -g3>
            $<$<CONFIG:Release>:-O3>
            $<$<CONFIG:RelWithDebInfo>:-O3 -g>
        )

        # ---- Link Time Optimization (LTO / IPO) ----
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported OUTPUT _)
        if(ipo_supported)
            set_target_properties(${target} PROPERTIES
                INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
                INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
            )
        endif()

        # ---- Architecture-specific optimization (safe) ----
        # Enable only for local x86 builds, avoid cross-build issues (e.g., cibuildwheel)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            if(ENABLE_NATIVE)
                target_compile_options(${target} PRIVATE -march=native)
            endif()
        endif()

    elseif(MSVC)

        # ---- MSVC optimization flags ----
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:/Od /Zi>
            $<$<CONFIG:Release>:/O2>
            $<$<CONFIG:RelWithDebInfo>:/O2 /Zi>
        )

        # ---- MSVC LTO ----
        set_target_properties(${target} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
        )

    endif()

endfunction()