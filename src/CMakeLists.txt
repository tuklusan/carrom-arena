# Carrom Arena - Source CMakeLists.txt

# Core library (headless-testable, no raylib)
add_library(carrom_core
    game/rules.c
    game/match.c
    game/board.c
    game/scoring.c
    game/events.c
    physics/physics.c
    physics/physics_snapshot.c
    ai/controller.c
    ai/baseline_controller.c
    ai/arena_controller.c
    ai/shot_candidates.c
    ai/shot_evaluator.c
    telemetry/trace.c
    telemetry/replay.c
    platform/platform.c
    common/types.c
    common/rng.c
    common/math.c
    common/strategy_profiles.c
)

# Include directories for core library
target_include_directories(carrom_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/game
    ${CMAKE_CURRENT_SOURCE_DIR}/physics
    ${CMAKE_CURRENT_SOURCE_DIR}/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${box2d_SOURCE_DIR}/include
)

# Link libraries for core library
target_link_libraries(carrom_core
    box2d
    ${PLATFORM_LIBS}
)
if(UNIX)
    target_link_libraries(carrom_core rt)
endif()

# Render library (depends on raylib)
add_library(carrom_render
    render/renderer.c
    render/board_view.c
    render/hud.c
    render/effects.c
)

target_include_directories(carrom_render PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/game
    ${CMAKE_CURRENT_SOURCE_DIR}/physics
    ${CMAKE_CURRENT_SOURCE_DIR}/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/render
    ${CMAKE_CURRENT_SOURCE_DIR}/telemetry
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${raylib_SOURCE_DIR}/src
)

target_link_libraries(carrom_render
    carrom_core
    raylib
)

# Strict warnings for our library code only (Debug builds)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(STRICT_WARNINGS
        -Wall -Wextra -Wpedantic
        -Wshadow -Wconversion -Wsign-conversion
        -Wnull-dereference
        -Wformat=2 -Wmisleading-indentation
        -Wno-unused-parameter
        -Wno-unused-function
    )
    # GCC-specific warnings not supported by Clang
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        list(APPEND STRICT_WARNINGS -Wduplicated-cond -Wlogical-op)
    endif()
    target_compile_options(carrom_core PRIVATE ${STRICT_WARNINGS})
    target_compile_options(carrom_render PRIVATE ${STRICT_WARNINGS})
    # Suppress double-promotion warnings from Box2D headers
    target_compile_options(carrom_core PRIVATE -Wno-double-promotion)
    target_compile_options(carrom_render PRIVATE -Wno-double-promotion)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(carrom_core PRIVATE -Werror)
        target_compile_options(carrom_render PRIVATE -Werror)
        # On Windows/Clang, Box2D headers trigger -Wimplicit-function-declaration
        # Disable -Werror for this specific warning on Windows
        if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
            target_compile_options(carrom_core PRIVATE -Wno-error=implicit-function-declaration)
            target_compile_options(carrom_render PRIVATE -Wno-error=implicit-function-declaration)
        endif()
        # Sanitizers only on Linux/GNU - not supported on Windows/Clang
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_compile_options(carrom_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_compile_options(carrom_render PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(carrom_core PRIVATE -fsanitize=address,undefined)
            target_link_options(carrom_render PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
elseif(MSVC)
    target_compile_options(carrom_core PRIVATE /W4 /WX /permissive- /std:c17)
    target_compile_options(carrom_render PRIVATE /W4 /WX /permissive- /std:c17)
endif()

# Compile definitions
target_compile_definitions(carrom_core PRIVATE
    PHYSICS_HZ=120
    MAX_SUBSTEPS=4
    BOARD_SIDE=1.0f
    POCKET_RADIUS=0.030f
    PIECE_RADIUS=0.021f
    STRIKER_RADIUS=0.028f
    BOARD_COULOMB=0.50f
    BOARD_VISCOUS=2.00f
    SETTLE_SPEED_EPS=1e-3f
    SETTLE_ACCEL_EPS=0.60f
    SETTLE_CONFIRM_STEPS=3
    SETTLE_TIMEOUT_SECONDS=8.0f
    MAX_CANDIDATES=320
    PLACEMENTS_PER_SEAT=8
    VARIANTS_PER_PLACEMENT=40
)

target_compile_definitions(carrom_render PRIVATE
    PHYSICS_HZ=120
    MAX_SUBSTEPS=4
    BOARD_SIDE=1.0f
    POCKET_RADIUS=0.030f
    PIECE_RADIUS=0.021f
    STRIKER_RADIUS=0.028f
    BOARD_COULOMB=0.50f
    BOARD_VISCOUS=2.00f
    SETTLE_SPEED_EPS=1e-3f
    SETTLE_ACCEL_EPS=0.60f
    SETTLE_CONFIRM_STEPS=3
    SETTLE_TIMEOUT_SECONDS=8.0f
    MAX_CANDIDATES=320
    PLACEMENTS_PER_SEAT=8
    VARIANTS_PER_PLACEMENT=40
)

# Enable math.h float functions (sqrtf, atan2f, etc.) on MinGW
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND WIN32)
    target_compile_definitions(carrom_core PRIVATE __USE_MINGW_ANSI_STDIO=1)
    target_compile_definitions(carrom_render PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()

# Suppress CRT secure warnings on Windows (fopen, strcat, strncpy deprecation)
if(WIN32)
    target_compile_definitions(carrom_core PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(carrom_render PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Main executable
add_executable(carrom_arena
    app/main.c
    app/app.c
)

target_link_libraries(carrom_arena
    carrom_core
    carrom_render
    raylib
)

# Suppress double-promotion warnings from Box2D headers - GNU/Clang
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(carrom_arena PRIVATE -Wno-double-promotion)
    # Enable math.h float functions (sqrtf, atan2f, etc.) on MinGW
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND WIN32)
        target_compile_definitions(carrom_arena PRIVATE __USE_MINGW_ANSI_STDIO=1)
    endif()
    # Suppress CRT secure warnings on Windows
    if(WIN32)
        target_compile_definitions(carrom_arena PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    # On Windows/Clang, Box2D headers trigger -Wimplicit-function-declaration
    # Disable -Werror for this specific warning on Windows
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(carrom_arena PRIVATE -Wno-error=implicit-function-declaration)
        target_compile_options(carrom_arena PRIVATE -Wno-error=missing-format-attribute)
    endif()
    # Sanitizers only on Linux/GNU - not supported on Windows/Clang
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_link_options(carrom_arena PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# Set output directory
set_target_properties(carrom_arena PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)