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
            -Wduplicated-cond -Wlogical-op
            -Wno-unused-parameter
            -Wno-unused-function
        )
        target_compile_options(${target} PRIVATE ${TEST_WARNINGS})
        # Force include system math.h before Box2D headers to fix missing sqrtf/atan2f declarations
        target_compile_options(${target} PRIVATE -include /usr/include/math.h -Wno-double-promotion)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            target_compile_options(${target} PRIVATE -Werror)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
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

# Disable format-truncation warning for this test (false positive)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
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

# Set test labels
set_tests_properties(rules_test PROPERTIES LABELS "unit")
set_tests_properties(physics_test PROPERTIES LABELS "unit")
set_tests_properties(ai_test PROPERTIES LABELS "unit")
set_tests_properties(trace_circular_test PROPERTIES LABELS "unit")
set_tests_properties(integration_test PROPERTIES LABELS "integration")