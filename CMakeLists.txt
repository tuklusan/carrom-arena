# Carrom Arena - CMake Build Configuration
# C17, raylib 5.5, Box2D v3.1.0, PCG32 (vendored)

cmake_minimum_required(VERSION 3.25)
project(CarromArena LANGUAGES C VERSION 1.0.0)

# =============================================================================
# C17 Standard & Extensions
# =============================================================================
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =============================================================================
# Base Compiler Warnings (for all targets) - NONE for external deps
# =============================================================================
# We apply strict warnings only to our own targets in src/ and tests/
# External dependencies (raylib, box2d, glfw, unity) build with their own flags

# =============================================================================
# Build Configurations
# =============================================================================
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type: Debug, Release, RelWithDebInfo" FORCE)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")

# =============================================================================
# Dependency Management via FetchContent
# =============================================================================
include(FetchContent)

# raylib 5.5
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
# Box2D v3.1.0
FetchContent_Declare(
    box2d
    GIT_REPOSITORY https://github.com/erincatto/box2d
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)
# Unity test framework (for unit tests)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity
    GIT_TAG        v2.6.0
    GIT_SHALLOW    TRUE
)

# Disable raylib's -Werror flags by setting cache variables before MakeAvailable
# raylib's CompilerFlags.cmake adds -Werror=pointer-arith and -Werror=implicit-function-declaration
# These cause build failures with our codebase, so we disable them
set(RAYLIB_NO_WERROR ON CACHE BOOL "Disable -Werror in raylib" FORCE)

# Make all dependencies available
FetchContent_MakeAvailable(raylib box2d unity)

# Patch Box2D math_functions.h to add explicit math function declarations for all platforms
# Box2D v3.1.0 headers don't include math.h, causing implicit declaration warnings/errors
if(EXISTS "${box2d_SOURCE_DIR}/include/box2d/math_functions.h")
    file(READ "${box2d_SOURCE_DIR}/include/box2d/math_functions.h" BOX2D_MATH_H)
    if(BOX2D_MATH_H AND NOT BOX2D_MATH_H MATCHES "Math function declarations added")
        # Add explicit math function declarations for all platforms
        string(REPLACE "#pragma once" 
            "#pragma once\n\n// Math function declarations for Box2D (all platforms)\n// Box2D headers use math functions without including math.h\nextern float sqrtf(float);\nextern float atan2f(float, float);\nextern float cosf(float);\nextern float sinf(float);\nextern float fabsf(float);\n" 
            BOX2D_MATH_H "${BOX2D_MATH_H}")
        file(WRITE "${box2d_SOURCE_DIR}/include/box2d/math_functions.h" "${BOX2D_MATH_H}")
        message(STATUS "Patched Box2D math_functions.h with math function declarations for all platforms")
    endif()
endif()

# Disable -Werror for Box2D target (external dependency)
if(TARGET box2d)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(box2d PRIVATE -Wno-error=implicit-function-declaration)
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(box2d PRIVATE -Wno-error=builtin-declaration-mismatch)
        endif()
    endif()
endif()

# Enable Box2D fixed-point if available (for determinism)
add_compile_definitions(B2_USE_FIXED_POINT=1)

# PCG32 is vendored - single header in src/common/pcg32.h

# =============================================================================
# Build ID from git (for telemetry)
# =============================================================================
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --dirty --always --tags
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE BUILD_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        set(BUILD_ID "unknown")
    endif()
else()
    set(BUILD_ID "unknown")
endif()
add_compile_definitions(BUILD_ID="${BUILD_ID}")

# =============================================================================
# Platform-specific libraries
# =============================================================================
if(WIN32)
    set(PLATFORM_LIBS ${PLATFORM_LIBS} gdi32 winmm)
elseif(APPLE)
    find_library(COCOA Cocoa)
    find_library(IOKIT IOKit)
    find_library(COREVIDEO CoreVideo)
    set(PLATFORM_LIBS ${PLATFORM_LIBS} ${COCOA} ${IOKIT} ${COREVIDEO} "-framework OpenGL")
elseif(UNIX)
    find_package(Threads REQUIRED)
    find_library(RT rt)
    find_library(M m)
    find_library(DL dl)
    find_library(X11 X11)
    find_library(XRANDR Xrandr)
    find_library(XINERAMA Xinerama)
    find_library(XCURSOR Xcursor)
    find_library(XI Xi)
    set(PLATFORM_LIBS ${PLATFORM_LIBS} ${RT} ${M} ${DL} ${X11} ${XRANDR} ${XINERAMA} ${XCURSOR} ${XI} Threads::Threads)
else()
    # Fallback for other platforms
    set(PLATFORM_LIBS ${PLATFORM_LIBS})
endif()

# =============================================================================
# Main Source Directory
# =============================================================================
add_subdirectory(src)

# =============================================================================
# Tests
# =============================================================================
enable_testing()
add_subdirectory(tests)

# =============================================================================
# Custom Targets
# =============================================================================
add_custom_target(clean_build
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}
    COMMENT "Clean build directory"
    VERBATIM
)

# Print build info
message(STATUS "============================================")
message(STATUS "Carrom Arena Build Configuration")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build ID: ${BUILD_ID}")
message(STATUS "  C Standard: ${CMAKE_C_STANDARD}")
message(STATUS "  raylib: 5.5 (FetchContent)")
message(STATUS "  Box2D: v3.1.0 (FetchContent)")
message(STATUS "  PCG32: vendored")
message(STATUS "============================================")