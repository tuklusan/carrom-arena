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
    rt
)

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
        -Wduplicated-cond -Wlogical-op
        -Wno-unused-parameter
        -Wno-unused-function
    )
    target_compile_options(carrom_core PRIVATE ${STRICT_WARNINGS})
    target_compile_options(carrom_render PRIVATE ${STRICT_WARNINGS})
    # Force include system math.h before Box2D headers to fix missing sqrtf/atan2f declarations
    target_compile_options(carrom_core PRIVATE -include /usr/include/math.h)
    target_compile_options(carrom_render PRIVATE -include /usr/include/math.h)
    # Suppress double-promotion warnings from Box2D headers
    target_compile_options(carrom_core PRIVATE -Wno-double-promotion)
    target_compile_options(carrom_render PRIVATE -Wno-double-promotion)
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(carrom_core PRIVATE -Werror)
        target_compile_options(carrom_render PRIVATE -Werror)
        target_compile_options(carrom_core PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_compile_options(carrom_render PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(carrom_core PRIVATE -fsanitize=address,undefined)
        target_link_options(carrom_render PRIVATE -fsanitize=address,undefined)
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
    BOARD_COULOMB=0.12f
    BOARD_VISCOUS=0.85f
    SETTLE_SPEED_EPS=1e-4f
    SETTLE_ACCEL_EPS=1e-4f
    SETTLE_CONFIRM_STEPS=3
    SETTLE_TIMEOUT_SECONDS=30.0f
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
    BOARD_COULOMB=0.12f
    BOARD_VISCOUS=0.85f
    SETTLE_SPEED_EPS=1e-4f
    SETTLE_ACCEL_EPS=1e-4f
    SETTLE_CONFIRM_STEPS=3
    SETTLE_TIMEOUT_SECONDS=30.0f
    MAX_CANDIDATES=320
    PLACEMENTS_PER_SEAT=8
    VARIANTS_PER_PLACEMENT=40
)

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

# Force include math.h before Box2D headers (for sqrtf, atan2f, etc.)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(carrom_arena PRIVATE -include /usr/include/math.h -Wno-double-promotion)
    target_link_options(carrom_arena PRIVATE -fsanitize=address,undefined)
endif()

# Set output directory
set_target_properties(carrom_arena PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)