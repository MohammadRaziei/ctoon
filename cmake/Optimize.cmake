# Optimize.cmake
# Cross-platform optimization flags for OS-independent builds

include_guard(GLOBAL)

# Function to apply optimization flags based on platform and compiler
function(apply_optimization_flags target)
    # Get compiler information
    get_target_property(target_type ${target} TYPE)
    
    # Determine the compiler being used
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        set(IS_GCC_OR_CLANG TRUE)
    else()
        set(IS_GCC_OR_CLANG FALSE)
    endif()

    # Optimization flags for GCC/Clang
    if(IS_GCC_OR_CLANG)
        # Performance optimization flags
        target_compile_options(${target} PRIVATE
            -O3                     # Maximum optimization
            -ffast-math             # Aggressive floating-point optimizations
            -fomit-frame-pointer    # Omit frame pointer when not needed
            -fstrict-aliasing       # Enable strict aliasing
            -ftree-vectorize        # Enable loop vectorization
        )

        # Link-time optimization (LTO) for Release builds
        if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
            include(CheckIPOSupported)
            check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
            if(ipo_supported)
                set_target_properties(${target} PROPERTIES
                    INTERPROCEDURAL_OPTIMIZATION TRUE
                )
                message(STATUS "LTO/IPO enabled for target: ${target}")
            else()
                message(WARNING "LTO/IPO not supported: ${ipo_error}")
            endif()
        endif()

        # Architecture-specific optimizations
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686|i386")
            # x86/x64 specific optimizations
            target_compile_options(${target} PRIVATE
                -march=native       # Optimize for current CPU architecture
                -mtune=native       # Tune for current CPU
            )
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
            # ARM64 specific optimizations
            target_compile_options(${target} PRIVATE
                -march=armv8-a+simd
            )
        endif()

    elseif(MSVC)
        # MSVC optimization flags
        target_compile_options(${target} PRIVATE
            /O2                     # Maximum optimization for speed
            /GL                     # Whole program optimization
            /RTC1-
        )

        target_link_options(${target} PRIVATE
            /LTCG                   # Link-time code generation
        )

        if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
            target_compile_options(${target} PRIVATE
                /arch:AVX2          # Enable AVX2 instructions
            )
        endif()
    endif()

    # # Platform-specific definitions
    # if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #     target_compile_definitions(${target} PRIVATE
    #         CTOON_PLATFORM_LINUX=1
    #     )
    # elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #     target_compile_definitions(${target} PRIVATE
    #         CTOON_PLATFORM_WINDOWS=1
    #     )
    # elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    #     target_compile_definitions(${target} PRIVATE
    #         CTOON_PLATFORM_MACOS=1
    #     )
    # endif()

    # # Cross-platform thread optimization
    # if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    #     target_compile_options(${target} PRIVATE
    #         -pthread
    #     )
    #     target_link_options(${target} PRIVATE
    #         -pthread
    #     )
    # endif()

endfunction()

# Function to apply debug-friendly optimization flags
function(apply_debug_optimization_flags target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE
            -Og                     # Optimize for debugging experience
            -g3                     # Maximum debug information
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE
            /Od                     # Disable optimization for debugging
            /Zi                     # Generate debug information
        )
    endif()
endfunction()

# Convenience function to apply flags to multiple targets
function(optimize_all_targets)
    foreach(target IN LISTS ARGV)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            apply_debug_optimization_flags(${target})
        else()
            apply_optimization_flags(${target})
        endif()
    endforeach()
endfunction()

# Print optimization summary
macro(print_optimization_summary)
    message(STATUS "=== Optimization Configuration ===")
    message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
    message(STATUS "==================================")
endmacro()
