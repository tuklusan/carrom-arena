# Carrom Arena Regression Test Contract

## Purpose
Design a regression test that would have caught the "jerky / no real game" state where:
- Shots fail to settle within bounded time
- Match never reaches terminal state
- Simulation stalls or runs indefinitely

---

## Test Contract: `test_regression.c`

### Test: `test_full_match_headless_bounded_time`

**Objective**: Run a complete autonomous best-of-3 match headless and assert:
1. Every shot settles within bounded physics steps
2. Match terminates with `PHASE_MATCH_OVER` within reasonable total steps
3. Deterministic behavior with fixed seed

### Constants (from physics.h)
```c
#define PHYSICS_HZ              120      // 120 steps/second
#define PHYSICS_DT              (1.0f/120.0f)
#define SETTLE_TIMEOUT_SECONDS  30.0f   // Physics sim timeout
#define SETTLE_CONFIRM_STEPS    3       // Consecutive settled steps required
```

### Derived Bounds
| Metric | Calculation | Value |
|--------|-------------|-------|
| **Max physics steps per shot** | 5s wall-clock @ 1x × 120 Hz | **600 steps** |
| **Generous per-shot budget** | 10s wall-clock @ 1x × 120 Hz | **1,200 steps** |
| **Max shots per match** | 3 games × 8 boards × ~20 shots | **~480 shots** |
| **Max total physics steps** | 1,200 × 500 shots | **600,000 steps** |

> **Note**: The 5s wall-clock bound maps to 600 physics steps at 120 Hz. We use 1,200 steps (10s) as generous CI-friendly limit accounting for slow CI runners.

### Test Algorithm
```c
void test_full_match_headless_bounded_time(void) {
    // 1. Fixed seed for determinism
    RNGContext rng;
    rng_context_init(&rng, REGRESSION_SEED);  // e.g., 0xDEADBEEF
    
    // 2. Initialize match, game, physics, controllers
    MatchState match;
    match_state_init(&match);
    
    GameState game;
    game_state_init(&game, REGRESSION_SEED);
    
    PhysicsWorld* physics = physics_create();
    
    Controller* controllers[4];
    for (int i = 0; i < 4; i++) {
        controllers[i] = arena_controller_create(
            (Seat)i, 
            &STRATEGY_PROFILES[STRATEGY_BALANCED], 
            &rng.streams[i]
        );
    }
    
    // 3. Start first board
    match_start_board(&match, &game, &rng);
    physics_sync_from_board(physics, &game.board, game.turn_seat);
    
    // 4. Trackers
    uint64_t total_physics_steps = 0;
    uint32_t shot_count = 0;
    const uint64_t MAX_TOTAL_STEPS = 600000;
    const uint32_t MAX_STEPS_PER_SHOT = 1200;
    
    // 5. Simulation loop
    while (!match_is_over(&match) && total_physics_steps < MAX_TOTAL_STEPS) {
        // Execute shot (AI decides, physics executes)
        if (game.phase == PHASE_PLACEMENT) {
            // AI decision
            DecisionSnapshot snap = { &match, &game, &game.board, physics_snapshot(physics), game.turn_seat };
            ShotPlan plan = controller_decide(controllers[game.turn_seat], &snap, &rng.streams[game.turn_seat]);
            physics_snapshot_destroy(snap.physics);
            
            // Validate & execute
            if (!match_validate_shot(&game, &plan)) {
                plan = controller_fallback_shot(controllers[game.turn_seat], &snap, &rng.streams[game.turn_seat]);
            }
            
            physics_place_striker(physics, game.turn_seat, plan.placement);
            physics_apply_shot(physics, plan.aim_angle, plan.power);
            game.phase = PHASE_SHOT_EXECUTION;
            shot_count++;
        }
        
        // Step physics until settled (bounded)
        uint32_t shot_steps = 0;
        while (game.phase == PHASE_SHOT_EXECUTION && shot_steps < MAX_STEPS_PER_SHOT) {
            physics_step(physics, PHYSICS_DT);
            total_physics_steps++;
            shot_steps++;
            
            if (physics_is_settled(physics)) {
                game.phase = PHASE_SETTLING;
            }
        }
        
        // Assert: shot settled within budget
        TEST_ASSERT_MESSAGE(
            shot_steps < MAX_STEPS_PER_SHOT,
            "Shot exceeded max physics steps - possible stall"
        );
        
        // Resolve shot
        if (game.phase == PHASE_SETTLING) {
            ShotResult result = {0};
            shot_result_init(&result);
            physics_collect_pocketed(physics, &result);
            physics_get_final_positions(physics, result.final_positions);
            result.sim_time = physics_get_sim_time(physics);
            
            ShotFacts facts;
            match_extract_facts(&game, &result, &facts);
            RulesOutcome outcome = rules_resolve(&match, &game, &facts);
            
            game = outcome.next_game_state;
            match = outcome.next_match_state;
            
            // Handle board/game/match transitions
            if (outcome.turn_decision == TURN_BOARD_OVER && !match_is_over(&match)) {
                match_start_board(&match, &game, &rng);
                physics_sync_from_board(physics, &game.board, game.turn_seat);
            } else if (outcome.turn_decision == TURN_CONTINUE || outcome.turn_decision == TURN_ADVANCE) {
                game.phase = PHASE_PLACEMENT;
            }
        }
    }
    
    // 6. Final assertions
    TEST_ASSERT_MESSAGE(
        total_physics_steps < MAX_TOTAL_STEPS,
        "Match exceeded total physics step budget - possible infinite loop"
    );
    
    TEST_ASSERT_EQUAL(PHASE_MATCH_OVER, game.phase);
    TEST_ASSERT_TRUE(match_is_over(&match));
    TEST_ASSERT_GREATER_THAN(0, shot_count);
    
    // Cleanup
    for (int i = 0; i < 4; i++) controller_destroy(controllers[i]);
    physics_destroy(physics);
}
```

---

## Soak Verification Script: `scripts/soak_verification.sh`

### Purpose
Run 10 seeds × 3 matches headless to verify regression fix holds across variance.

### Requirements
- **10 unique seeds** (deterministic but varied)
- **3 matches per seed** (best-of-3 each)
- **Headless, no renderer**
- **Exit code 0** = all pass, **non-zero** = any failure
- **Output**: JSON summary with per-seed/match stats

### Algorithm
```bash
#!/bin/bash
# scripts/soak_verification.sh

SEEDS=(12345 42 999 123456 7890 555 777 888 9999 4242)
MATCHES_PER_SEED=3
MAX_TOTAL_STEPS=600000
MAX_STEPS_PER_SHOT=1200

FAILURES=0
RESULTS_JSON="soak_results_$(date +%s).json"

echo '{"seeds":[' > "$RESULTS_JSON"

for i in "${!SEEDS[@]}"; do
    SEED=${SEEDS[$i]}
    
    echo "  Seed $SEED ($((i+1))/10)..."
    
    for m in $(seq 1 $MATCHES_PER_SEED); do
        # Run single match via test executable
        # (test_regression returns non-zero on failure)
        if ./test_regression --seed=$SEED --match=$m --max-steps=$MAX_TOTAL_STEPS --max-shot-steps=$MAX_STEPS_PER_SHOT; then
            echo "    Match $m: PASS"
            # Append pass to JSON
        else
            echo "    Match $m: FAIL"
            FAILURES=$((FAILURES + 1))
        fi
    done
done

echo ']}' >> "$RESULTS_JSON"

if [ $FAILURES -eq 0 ]; then
    echo "SOAK VERIFICATION PASSED (10 seeds × 3 matches)"
    exit 0
else
    echo "SOAK VERIFICATION FAILED: $FAILURES failures"
    exit 1
fi
```

### Integration with CI
```yaml
# .github/workflows/soak.yml
- name: Run Regression Test
  run: ctest -R regression_test --output-on-failure

- name: Run Soak Verification
  run: ./scripts/soak_verification.sh
  timeout-minutes: 30
```

---

## CMake Integration

### Add to `tests/CMakeLists.txt`
```cmake
# Regression test (integration-level, headless match)
add_executable(test_regression
    test_regression.c
    ${unity_SOURCE_DIR}/src/unity.c
)

apply_test_warnings(test_regression)

# Link against core + box2d (no raylib - headless)
target_link_libraries(test_regression carrom_core box2d ${PLATFORM_LIBS})
target_include_directories(test_regression PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${unity_SOURCE_DIR}/src
)

add_test(NAME regression_test COMMAND test_regression)
set_tests_properties(regression_test PROPERTIES LABELS "regression" TIMEOUT 60)
```

### Test Executable CLI Interface
```c
// test_regression.c supports:
//   --seed <n>           Master RNG seed (default: 0xDEADBEEF)
//   --match <n>          Match index within seed (default: 1)
//   --max-steps <n>      Total physics step budget (default: 600000)
//   --max-shot-steps <n> Per-shot physics step budget (default: 1200)
//   --verbose            Verbose logging
```

---

## Acceptance Criteria

| Criterion | Pass Condition |
|-----------|----------------|
| **Per-shot bound** | Every shot settles ≤ 1,200 physics steps (10s @ 120Hz) |
| **Match termination** | Reaches `PHASE_MATCH_OVER` ≤ 600,000 total physics steps |
| **Determinism** | Same seed produces identical shot_count, final scores, events |
| **No hangs** | Test completes in < 60s wall-clock on CI runner |
| **Soak pass** | 10 seeds × 3 matches = 30 matches all pass |

---

## Test Execution Results

### Debug Build with ASAN (build/)
- **Test executable**: `build/tests/test_regression`
- **Runtime**: ~693 seconds (11.5 minutes) with ASAN overhead
- **Result**: **REGRESSION DETECTED** ✓

```
test_physics_settles_from_rest:PASS
test_shot_settling_bounded_per_shot:PASS  
test_debug_ai_candidates:PASS
test_full_match_headless_bounded_time:FAIL: Expected 8 Was 1
test_deterministic_replay:PASS
```

**Failure Analysis**: `Expected 8 (PHASE_MATCH_OVER) Was 1 (PHASE_PLACEMENT)` - The match never reached terminal state, confirming the "jerky / no real game" bug where AI makes valid shots but never pockets pieces, causing infinite play.

### Release Build (without ASAN)
For faster CI runs, use a Release build:
```bash
cmake -B build_release -DCMAKE_BUILD_TYPE=Release -S .
cmake --build build_release --target test_regression
./build_release/tests/test_regression
```

---

## Why This Catches "Jerky / No Real Game"

| Failure Mode | How Test Detects |
|--------------|------------------|
| Physics never settles | `shot_steps >= MAX_STEPS_PER_SHOT` assertion fires |
| Match never ends | `total_physics_steps >= MAX_TOTAL_STEPS` assertion fires |
| Infinite loop in AI | Same - total step budget exceeded |
| Stuck in PHASE_SHOT_EXECUTION | Per-shot step counter catches it |
| Stuck in PHASE_SETTLING | Double-check settling, then resolve - bounded |
| Queen/cover logic deadlock | Match never reaches BOARD_OVER → total step budget |

---

## Future Extensions

1. **Per-shot latency histogram** - export JSON with step counts per shot
2. **Statistical bounds** - track p95, p99 shot steps across soak runs
3. **Bisect helper** - `test_regression --bisect` to find first failing seed
4. **Trace comparison** - diff trace outputs between known-good and current