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
    ai/geometry_planner.c
    telemetry/flight.c
    audio/audio_policy.c
    audio/radio_stream.c
    audio/radio_mp3.c
    ai/shot_evaluator.c
    telemetry/trace.c
    platform/platform.c
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
    ${CMAKE_SOURCE_DIR}/third_party/minimp3
)

# minimp3 is third-party code: compiled without our warning flags
set_source_files_properties(audio/radio_mp3.c PROPERTIES COMPILE_OPTIONS "-w")

# Link libraries for core library
target_link_libraries(carrom_core
    box2d
    ${PLATFORM_LIBS}
)
if(UNIX AND NOT APPLE)   # librt is Linux-only
    target_link_libraries(carrom_core rt)
endif()

# Render library (depends on raylib)

# Embed the sound effects (assets/audio/*.ogg) into the executable as C arrays
file(GLOB CARROM_AUDIO_FILES ${CMAKE_SOURCE_DIR}/assets/audio/*.ogg)
list(SORT CARROM_AUDIO_FILES)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CARROM_AUDIO_FILES})
set(CARROM_AUDIO_C "${CMAKE_CURRENT_BINARY_DIR}/audio_assets.c")
set(_audio_body "#include \"audio/audio_assets.h\"\n")
set(_audio_table "")
set(_audio_count 0)
foreach(_f ${CARROM_AUDIO_FILES})
    get_filename_component(_n ${_f} NAME_WE)
    file(READ ${_f} _hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," _bytes "${_hex}")
    string(LENGTH "${_hex}" _hl)
    math(EXPR _len "${_hl} / 2")
    string(APPEND _audio_body "static const unsigned char snd_${_n}[] = {${_bytes}};\n")
    string(APPEND _audio_table "    { \"${_n}\", snd_${_n}, ${_len} },\n")
    math(EXPR _audio_count "${_audio_count} + 1")
endforeach()
string(APPEND _audio_body "const AudioAsset audio_assets[] = {\n${_audio_table}    { 0, 0, 0 }\n};\nconst int audio_asset_count = ${_audio_count};\n")
file(WRITE ${CARROM_AUDIO_C} "${_audio_body}")

add_library(carrom_render
    render/renderer.c
    render/board_view.c
    render/effects.c
    audio/audio.c
    audio/radio.c
    ${CARROM_AUDIO_C}
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
    else()
        # Clang-specific: disable warnings not applicable or too strict for our codebase
        list(APPEND STRICT_WARNINGS -Wno-newline-eof -Wno-implicit-float-conversion)
    endif()
    target_compile_options(carrom_core PRIVATE ${STRICT_WARNINGS})
    target_compile_options(carrom_render PRIVATE ${STRICT_WARNINGS})
    # Suppress double-promotion warnings from Box2D headers
    target_compile_options(carrom_core PRIVATE -Wno-double-promotion)
    target_compile_options(carrom_render PRIVATE -Wno-double-promotion)
    # Force include system math.h before Box2D headers to fix missing sqrtf/atan2f declarations (Linux only)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(carrom_core PRIVATE -include math.h)
        target_compile_options(carrom_render PRIVATE -include math.h)
    endif()
    
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
    MAX_SUBSTEPS=40
    BOARD_SIDE=1.0f
    POCKET_RADIUS=0.030f
    PIECE_RADIUS=0.021f
    STRIKER_RADIUS=0.028f
    BOARD_COULOMB=1.3f
    BOARD_VISCOUS=0.0f
    SETTLE_SPEED_EPS=1e-3f
    SETTLE_ACCEL_EPS=0.60f
    SETTLE_CONFIRM_STEPS=5
    SETTLE_TIMEOUT_SECONDS=30.0f
    MAX_CANDIDATES=320
    PLACEMENTS_PER_SEAT=8
    VARIANTS_PER_PLACEMENT=40
)

target_compile_definitions(carrom_render PRIVATE
    PHYSICS_HZ=120
    MAX_SUBSTEPS=40
    BOARD_SIDE=1.0f
    POCKET_RADIUS=0.030f
    PIECE_RADIUS=0.021f
    STRIKER_RADIUS=0.028f
    BOARD_COULOMB=1.3f
    BOARD_VISCOUS=0.0f
    SETTLE_SPEED_EPS=1e-3f
    SETTLE_ACCEL_EPS=0.60f
    SETTLE_CONFIRM_STEPS=5
    SETTLE_TIMEOUT_SECONDS=30.0f
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

# Radio stream check (network + MP3 decode, no audio device)
add_executable(radio_probe tools/radio_probe.c)
target_link_libraries(radio_probe carrom_core)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(radio_probe PRIVATE -fsanitize=address,undefined)
endif()

# Flight-recorder decoder
add_executable(flight_dump tools/flight_dump.c)
target_link_libraries(flight_dump carrom_core)

# Headless AI-vs-AI self-play (stall detector)
add_executable(selfplay tools/selfplay.c)
target_link_libraries(selfplay carrom_core)
set_target_properties(selfplay PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(selfplay PRIVATE -Wno-error=implicit-function-declaration)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_link_options(selfplay PRIVATE -fsanitize=address,undefined)
    endif()
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

# Windows: a GUI-subsystem program, so no console window opens next to the game window (debug output goes to the trace directory)
if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(carrom_arena PRIVATE -mwindows)
endif()

# Suppress double-promotion warnings from Box2D headers - GNU/Clang
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(carrom_arena PRIVATE -Wno-double-promotion)
    # Clang-specific: disable warnings not applicable or too strict for our codebase
    if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
        target_compile_options(carrom_arena PRIVATE -Wno-newline-eof -Wno-implicit-float-conversion)
    endif()
    # Force include system math.h before Box2D headers to fix missing sqrtf/atan2f declarations (Linux only)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(carrom_arena PRIVATE -include math.h)
    endif()
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
set_target_properties(flight_dump PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(flight_dump PRIVATE -Wno-error=implicit-function-declaration)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_link_options(flight_dump PRIVATE -fsanitize=address,undefined)
    endif()
endif()

# Disable -Werror=implicit-function-declaration for targets including Box2D headers
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(carrom_core PRIVATE -Wno-error=implicit-function-declaration)
    target_compile_options(carrom_render PRIVATE -Wno-error=implicit-function-declaration)
    target_compile_options(carrom_arena PRIVATE -Wno-error=implicit-function-declaration)
    # -Werror=builtin-declaration-mismatch is GNU-only
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(carrom_core PRIVATE -Wno-error=builtin-declaration-mismatch)
        target_compile_options(carrom_render PRIVATE -Wno-error=builtin-declaration-mismatch)
        target_compile_options(carrom_arena PRIVATE -Wno-error=builtin-declaration-mismatch)
    endif()
endif()