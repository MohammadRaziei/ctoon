# Optimize.cmake (simple & safe)

function(apply_optimization target)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")

        # Debug vs Release
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE -Og -g)
        else()
            target_compile_options(${target} PRIVATE -O3)
        endif()

        # فقط برای x86 لوکال (نه CI / نه cross build)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            target_compile_options(${target} PRIVATE -march=native)
        endif()

    elseif(MSVC)

        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE /Od /Zi)
        else()
            target_compile_options(${target} PRIVATE /O2)
        endif()

    endif()

endfunction()