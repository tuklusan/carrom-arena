# Carrom Arena - Tests CMakeLists.txt

# Unity test framework (already fetched)
include_directories(${unity_SOURCE_DIR}/src)

# Strict warnings for test targets
function(apply_test_warnings target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(TEST_WARNINGS
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion
            -Wnull-dereference -Wdouble-promotion
            -Wformat=2 -Wmisleading-indentation
            -Wno-unused-parameter
            -Wno-unused-function
        )
        # GCC-specific warnings not supported by Clang
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            list(APPEND TEST_WARNINGS -Wduplicated-cond -Wlogical-op)
        else()
            # Clang-specific: disable warnings not applicable or too strict for our codebase
            list(APPEND TEST_WARNINGS -Wno-newline-eof)
        endif()
        target_compile_options(${target} PRIVATE ${TEST_WARNINGS})
        # Force include system math.h before Box2D headers to fix missing sqrtf/atan2f declarations (Linux only)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_compile_options(${target} PRIVATE -include math.h -Wno-double-promotion)
        else()
            target_compile_options(${target} PRIVATE -Wno-double-promotion)
        endif()
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE -Werror)
            # On Windows/Clang, Box2D headers trigger -Wimplicit-function-declaration
            # Disable -Werror for this specific warning on Windows
            if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
                target_compile_options(${target} PRIVATE -Wno-error=implicit-function-declaration)
            endif()
            # Sanitizers only on Linux/GNU - not supported on Windows/Clang
            if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
                target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${target} PRIVATE -fsanitize=address,undefined)
            endif()
        endif()
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /std:c17)
    endif()
endfunction()

# Test executable for rules engine
add_executable(test_rules
    test_rules.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_rules)

target_link_libraries(test_rules carrom_core)
target_include_directories(test_rules PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

add_test(NAME rules_test COMMAND test_rules)

# Test executable for physics settling
add_executable(test_pocketing_deterministic
    test_pocketing_deterministic.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_pocketing_deterministic)
target_link_libraries(test_pocketing_deterministic carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_pocketing_deterministic PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME pocketing_deterministic_test COMMAND test_pocketing_deterministic)

add_executable(test_physics
    test_physics.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_physics)

target_link_libraries(test_physics carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_physics PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

add_test(NAME physics_test COMMAND test_physics)

# Test executable for AI isolation
add_executable(test_ai
    test_ai.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_ai)

target_link_libraries(test_ai carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_ai PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

add_test(NAME ai_test COMMAND test_ai)

# Test executable for circular trace buffer
add_executable(test_trace_circular
    test_trace_circular.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_trace_circular)

# Disable format-truncation warning for this test (false positive, GNU only)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_trace_circular PRIVATE -Wno-format-truncation)
endif()

target_link_libraries(test_trace_circular carrom_core ${PLATFORM_LIBS})
target_include_directories(test_trace_circular PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

add_test(NAME trace_circular_test COMMAND test_trace_circular)

# Integration test (full game simulation)
add_executable(test_integration
    test_integration.c
    ${unity_SOURCE_DIR}/src/unity.c
)

add_executable(test_pocketing_integration
    test_pocketing_integration.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_pocketing_integration)
target_link_libraries(test_pocketing_integration carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_pocketing_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME pocketing_integration_test COMMAND test_pocketing_integration)

apply_test_warnings(test_integration)

# Disable -Werror for integration test due to app.c linking complexity (app.c uses raylib)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_integration PRIVATE -Wno-error)
endif()

target_link_libraries(test_integration carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_integration PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

add_test(NAME integration_test COMMAND test_integration)

# Regression test (headless full match with bounded time assertions)
# Catches: "jerky / no real game" - shots not settling, match not terminating
add_executable(test_regression
    test_regression.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_regression)

# Link against core + box2d (headless - no raylib)
target_link_libraries(test_regression carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

# Reduce MAX_CANDIDATES for faster regression testing (default is 320)
# Reduce MAX_SIM_TIME for candidate evaluation (default is 4.0f seconds)
target_compile_definitions(test_regression PRIVATE MAX_CANDIDATES=5 MAX_SIM_TIME_VAL=0.5f)

# Default run with fixed seed and full bounds (1200s timeout with ASAN)
add_test(NAME regression_test COMMAND test_regression)

# Test executable for ICF layout
add_executable(test_icf_layout
    test_icf_layout.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_icf_layout)
target_link_libraries(test_icf_layout carrom_core ${PLATFORM_LIBS})
target_include_directories(test_icf_layout PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME icf_layout_test COMMAND test_icf_layout)

# Test executable for snapshot rotation
add_executable(test_snapshot_rotation
    test_snapshot_rotation.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_snapshot_rotation)
target_link_libraries(test_snapshot_rotation carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_snapshot_rotation PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME snapshot_rotation_test COMMAND test_snapshot_rotation)

# Set test labels
set_tests_properties(icf_layout_test PROPERTIES LABELS "unit")
set_tests_properties(snapshot_rotation_test PROPERTIES LABELS "unit")

# R14d Physics Tests
add_executable(test_physics_r14d
    test_physics_r14d.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_physics_r14d)
target_link_libraries(test_physics_r14d carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_physics_r14d PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME physics_r14d_test COMMAND test_physics_r14d)

# Locked Dimensions Test
add_executable(test_locked_dimensions
    test_locked_dimensions.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_locked_dimensions)
target_link_libraries(test_locked_dimensions carrom_core ${PLATFORM_LIBS})
target_include_directories(test_locked_dimensions PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME locked_dimensions_test COMMAND test_locked_dimensions)

# Telemetry Extensions Test
add_executable(test_telemetry_extensions
    test_telemetry_extensions.c
    ${unity_SOURCE_DIR}/src/unity.c
)
apply_test_warnings(test_telemetry_extensions)
target_link_libraries(test_telemetry_extensions carrom_core ${PLATFORM_LIBS})
target_include_directories(test_telemetry_extensions PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)
add_test(NAME telemetry_extensions_test COMMAND test_telemetry_extensions)

# Capture regression test (spawns carrom_arena binary in capture mode)

# Catches: capture-mode hang (Gap-2 renderer rewrite regression)
add_executable(test_capture
    test_capture.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_capture)

# Disable -Werror for this test due to POSIX popen/pclose with -std=c17 and format-truncation
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_capture PRIVATE -Wno-error=implicit-function-declaration -Wno-error=int-conversion)
endif()
# format-truncation is GCC-specific; disable as error only for GNU
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(test_capture PRIVATE -Wno-error=format-truncation)
endif()

# No carrom_core link needed - this test spawns the full binary
target_include_directories(test_capture PRIVATE
    ${unity_SOURCE_DIR}/src
)

# capture_test needs to find carrom_arena at cwd, so run from build/ dir:
add_test(NAME capture_test COMMAND test_capture)
set_tests_properties(capture_test PROPERTIES LABELS "regression" TIMEOUT 120)
set_tests_properties(capture_test PROPERTIES WORKING_DIRECTORY ${CMAKE_BINARY_DIR})