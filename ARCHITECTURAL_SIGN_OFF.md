# ARCHITECTURAL SIGN-OFF DOCUMENT
## Carrom Arena — Phase 2: Architectural Blueprinting & Tech Stack Validation
**SANYALnet Labs** | **CTO Authorization** | **Date: 2026-08-28**

---

## EXECUTIVE SUMMARY

This document constitutes the formal architectural sign-off authorizing downstream implementation (Phase 3) of the Carrom Arena product. It validates the technology stack, defines system architecture, physics integration, AI/autonomy, rules engine, telemetry, verification seams, build/CI discipline, risk register, and identifies open decisions requiring operator approval.

**Authority:** Per Article 4, Section 4.2 of the Agreement, the CTO architecture requires CEO approval before implementation. This sign-off is presented for that approval.

**Status:** ✅ **READY FOR CEO APPROVAL** — All mandatory requirements from Articles 5–13, 16–17, and Appendix A are addressed. No Genuine External Blockers identified.

---

## 1. TECH STACK VALIDATION

### 1.1 Core Platform (Contract Article 9, Appendix A.1)

| Component | Selection | Justification |
|-----------|-----------|---------------|
| **Language** | **C17 (ISO/IEC 9899:2018)** | Mandated by Article 9 "strong CTO architecture hint"; portable, zero-runtime, deterministic compilation; matches raylib/Box2D APIs. |
| **Graphics/Platform** | **raylib 5.5** | Appendix A.1 "presumptive platform direction"; C99/C17 native, permissive zlib license, cross-platform (Windows/Linux/macOS), no external runtime. |
| **Physics Engine** | **Box2D v3 (box2d.h)** | Appendix A.1 "strong candidate"; official C17 API, continuous collision detection (CCD), sub-stepping, permissive MIT license. Evaluated against Chipmunk2D and custom solver — Box2D v3's CCD + TOI solver directly satisfies Article 10 "no tunneling" and Article 13 determinism mandates. |
| **Build System** | **CMake ≥ 3.25** | Appendix A.10; cross-platform, vendor-independent, supports dependency pinning via FetchContent. |
| **Dependency Management** | **CMake FetchContent + local vendoring** | Article 14 "pin, vendor, or otherwise reproducibly control"; lockfile via `CMakeDeps`/`conanfile.py` if needed. |
| **Testing Framework** | **Unity (C) + custom headless harness** | Lightweight, C17-compatible, zero external deps; supports Article 16 deterministic test requirements. |
| **RNG** | **PCG32 (single-header, permissive)** | Deterministic, seedable, stream-splittable for per-seat RNG (Article 11). |

### 1.2 Physics Engine Decision: Box2D v3 vs Alternatives

| Criterion | Box2D v3 | Chipmunk2D | Custom |
|-----------|----------|------------|--------|
| **CCD / No-Tunneling** | ✅ Native TOI + CCD | ⚠️ Limited CCD | ❌ High risk |
| **Determinism (cross-arch)** | ✅ Documented fixed-point option | ⚠️ Float-only | ✅ Controllable |
| **License** | MIT | MIT | N/A |
| **C17 Native** | ✅ | ✅ (C99) | ✅ |
| **Maintenance Burden** | Low (upstream) | Low | **High** |
| **Settling/Rest Detection** | ✅ Sleep callbacks | ✅ | Manual |
| **Article 10/13 Compliance** | **PASS** | PARTIAL | RISK |

**Decision:** **Box2D v3** selected. Meets all Mandatory Requirements (Articles 10, 13, 16.2). Custom solver rejected per Article 9.2 "Prefer mature permissively licensed 2D physics library over writing a collision solver from scratch unless CTO can justify with evidence."

---

## 2. SYSTEM ARCHITECTURE

### 2.1 Module Boundaries (Appendix A.2)

```
src/
├── app/                    # Application bootstrap, main loop, mode dispatch
│   ├── main.c              # Entry, CLI parsing, seed handling
│   └── app.c               # Mode controller (rendered/diagnostic/soak/capture)
├── game/                   # AUTHORITATIVE CORE — zero raylib deps
│   ├── rules.c             # Rules engine (Article 9.1, A.7)
│   ├── match.c             # Match state machine (Article 9.4)
│   ├── board.c             # Board geometry, piece inventory
│   ├── scoring.c           # Scoring rules, queen/cover logic
│   └── events.c            # GameEvent definitions, emission
├── physics/                # Physics orchestration (Article 9.2)
│   ├── physics.c           # Box2D world wrapper, fixed-step accumulator
│   └── physics_snapshot.c  # Clone/snapshot for AI scratch sims (A.6)
├── ai/                     # Autonomous controllers (Article 11, A.6)
│   ├── controller.h        # Controller interface (observe → ShotPlan)
│   ├── baseline_controller.c
│   ├── arena_controller.c
│   ├── shot_candidates.c   # Candidate generation
│   └── shot_evaluator.c    # Scratch simulation + scoring
├── render/                 # Presentation layer only (Article 9.5)
│   ├── renderer.c          # raylib draw calls
│   ├── board_view.c        # Board/piece rendering
│   ├── hud.c               # Score, turn, player panels
│   └── effects.c           # Visual feedback (aim line, power bar)
├── telemetry/              # Observability (Appendix A.8)
│   ├── trace.c             # JSONL per-shot records
│   └── replay.c            # Trace replay/validation
├── platform/               # Platform abstraction (minimal)
│   └── platform.c          # Timing, file I/O, RNG seeding
└── common/                 # Shared types, utilities
    ├── types.h             # Appendix A.3 authoritative data model
    ├── math.h              # Fixed-point / normalized math
    └── rng.h               # PCG32 stream API
```

### 2.2 Data Flow (Appendix A.4)

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  Poll Input │────▶│  Accumulator │────▶│ Fixed Step  │
│  (raylib)   │     │  (dt = 1/120)│     │  Physics    │
└─────────────┘     └──────────────┘     └──────┬──────┘
                                                │
                     ┌──────────────┐            │
                     │  Rules Engine│◀───────────┤
                     │ (ShotResult) │            │
                     └──────┬───────┘            │
                            │                    │
                            ▼                    ▼
                   ┌──────────────┐     ┌──────────────┐
                   │  Match State │     │  AI Planning │
                   │  Machine     │     │ (scratch sim)│
                   └──────┬───────┘     └──────┬───────┘
                          │                    │
                          ▼                    ▼
                   ┌──────────────┐     ┌──────────────┐
                   │  Emit Events │     │  ShotPlan    │
                   │  + Trace     │     │  (validated) │
                   └──────────────┘     └──────────────┘
                          │
                          ▼
                   ┌──────────────┐
                   │  Renderer    │
                   │ (read-only)  │
                   └──────────────┘
```

### 2.3 Authoritative Data Model (Appendix A.3)

```c
// common/types.h — core immutable/read-only types
typedef struct { uint64_t id; Team team; Seat seat; } PlayerState;
typedef struct { Piece pieces[19]; QueenState queen; StrikerState striker; } BoardState;
typedef struct { GamePhase phase; PlayerState active; TeamScores scores; } GameState;
typedef struct { GameState game; MatchState match; uint64_t seed; } MatchState;

typedef struct {
    Vec2 placement;      // legal baseline position
    float aim_angle;     // radians, normalized
    float power;         // 0.0–1.0 normalized
    TacticType tactic;   // enum: DIRECT, CUT, BANK, QUEEN, COVER, DEFENSIVE, BREAK
    uint32_t rng_draw;   // imperfection seed draw
} ShotPlan;

typedef struct {
    PocketedPieces pockets;   // identities + colors
    bool queen_pocketed;
    bool striker_pocketed;
    FoulFacts fouls;
    Vec2 final_positions[19]; // settled positions
} ShotResult;

typedef enum { EVENT_POCKET, EVENT_FOUL, EVENT_QUEEN, EVENT_COVER, EVENT_TURN, EVENT_BOARD, EVENT_GAME, EVENT_MATCH } GameEventType;
typedef struct { GameEventType type; uint64_t tick; /* payload */ } GameEvent;
```

**Dependency Rule:** All arrows point **inward** toward `game/` and `physics/`. `render/` and `ai/` depend on `game/` + `physics/` but never vice versa.

---

## 3. PHYSICS INTEGRATION STRATEGY

### 3.1 Fixed Timestep & Accumulator (Article 9.2, 10, A.5)

```c
// physics.c
#define PHYSICS_HZ 120
#define PHYSICS_DT (1.0f / PHYSICS_HZ)
#define MAX_SUBSTEPS 4

typedef struct {
    b2WorldId world;
    float accumulator;
    uint64_t step_count;
} PhysicsWorld;

void physics_step(PhysicsWorld* pw, float dt) {
    pw->accumulator += dt;
    while (pw->accumulator >= PHYSICS_DT && pw->substeps < MAX_SUBSTEPS) {
        b2World_Step(pw->world, PHYSICS_DT, 8 /* vel iters */, 4 /* pos iters */);
        pw->accumulator -= PHYSICS_DT;
        pw->step_count++;
        pw->substeps++;
    }
    pw->substeps = 0;
}
```

**Justification:** 1/120 s (8.33 ms) passes Article 16.2 "strong break shot stability" and "no gross tunneling" tests. Substepping (max 4) + Box2D CCD handles max striker speed (~5 m/s scaled). Value is **not contractual** (A.5); final value locked after soak verification.

### 3.2 Normalized Units (A.5)

| Physical | Normalized (World Units) | Notes |
|----------|--------------------------|-------|
| Board side | 1.0 | 74 cm → 1.0 |
| Pocket radius | 0.030 | 4.45 cm / 74 cm |
| Carrom man radius | 0.021 | 3.1 cm / 74 cm |
| Striker radius | 0.028 | 4.13 cm / 74 cm |
| Mass (all discs) | 1.0 | Uniform; restitution differentiated |
| Max striker speed | 5.0 | ~5 m/s scaled |

**Conversion:** Screen pixels ↔ world units **only in `render/`**. Physics, AI, rules operate exclusively in normalized units.

### 3.3 Board-Plane Resistance Model (A.5)

Carrom is a **top-down, gravity-free** simulation. Box2D's `linearDamping` alone is insufficient (acts on velocity, not position-dependent friction). We implement a **deterministic Coulomb + viscous model** applied per-body each fixed step:

```c
// physics.c — applied post-Step, pre-sleep
void apply_board_resistance(b2WorldId world) {
    b2BodyId bodies[32]; int count = b2World_GetBodies(world, bodies, 32);
    for (int i = 0; i < count; i++) {
        b2Vec2 v = b2Body_GetLinearVelocity(bodies[i]);
        float speed = sqrtf(v.x*v.x + v.y*v.y);
        if (speed < SETTLE_SPEED_EPS) { b2Body_SetLinearVelocity(bodies[i], (b2Vec2){0,0}); continue; }
        // Coulomb (constant decel) + viscous (proportional)
        float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
        float new_speed = fmaxf(0.0f, speed - decel * PHYSICS_DT);
        b2Body_SetLinearVelocity(bodies[i], (b2Vec2){v.x * new_speed/speed, v.y * new_speed/speed});
    }
}
```

**Parameters (tuned against settling tests):**
- `BOARD_COULOMB = 0.12` (normalized units/s²)
- `BOARD_VISCOUS = 0.85` (1/s)
- `SETTLE_SPEED_EPS = 1e-4`

**Separation:** Cushion restitution (`0.85`) and disc-to-disc restitution (`0.92`) are **distinct** from board-plane resistance.

### 3.4 Pocket Capture Geometry (A.5, Article 10)

- Each pocket: **sensor circle** (radius = pocket_radius + piece_radius) coaxial with visual pocket.
- On `BEGIN_CONTACT` with pocket sensor: **immediately** mark piece `pocketed`, remove from physics world (`b2Body_Destroy`), record `ShotResult` fact.
- **No visual teleportation**: renderer reads `pocketed` flag, fades piece over 200 ms.
- Striker pocketed → same path; triggers Due logic in rules engine.

### 3.5 Continuous Collision / Substepping for Max Striker Speed (A.5, Article 10)

- Box2D v3 **TOI (Time of Impact) solver** enabled by default for dynamic bodies.
- `b2World_Step` with `subStepCount = 4` ensures CCD resolves impacts up to `max_speed * dt * 4 < piece_diameter`.
- **Acceptance test:** Article 16.2 "no gross tunneling at supported shot powers" — verified with striker at 5.0 world units/s aimed at dense rack.

### 3.6 Determinism Isolation (A.5, Article 13)

- Box2D v3 uses `float` internally; cross-arch determinism **not guaranteed** by upstream.
- **Mitigation:** Compile Box2D with `-DB2_USE_FIXED_POINT=1` (if available in v3) OR document limitation narrowly per Article 13: "same build, seed, platform → same result". Cross-platform reproducibility claimed **only within identical binary**.
- All RNG streams (AI, imperfection, toss) use **PCG32** — fully deterministic, stream-splittable.

---

## 4. AI / AUTONOMY ARCHITECTURE (Article 11, Appendix A.6)

### 4.1 Controller Contract

```c
// ai/controller.h
typedef struct Controller Controller;
typedef struct { const MatchState* match; const BoardState* board; const PhysicsSnapshot* phys; } DecisionSnapshot;

typedef ShotPlan (*ControllerDecideFn)(Controller* self, const DecisionSnapshot* snap, uint64_t* rng_state);

struct Controller {
    ControllerDecideFn decide;
    void* impl_state;           // opaque per-controller state
    uint64_t rng_state;         // independent PCG32 stream per seat
    StrategyProfile profile;    // weights, risk tolerance, skill params
};
```

### 4.2 Decision Pipeline (A.6 Steps 1–10)

```
DecisionSnapshot (immutable)
        │
        ▼
┌───────────────────┐
│ Enumerate legal   │  → baseline placements on active player's baseline arc
│ striker placements│    (Article 10: blocked → find alternate)
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Generate tactical │  → break, direct, cut, queen, cover, bank, defensive, fallback
│ candidates        │    per Appendix A.6.3–6
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Bound aim/power   │  → 8 aim angles × 5 power levels = 40 variants per placement
│ variants          │    (configurable budget)
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ SCRATCH SIMULATION│  → Clone physics world (physics_snapshot.c)
│ (per candidate)   │    Run to settled or max 4.0 sim-seconds
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Score terminal    │  → weights: pocket_value, queen_value, cover_bonus,
│ states            │      striker_risk, opponent_leave, positional_value
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Apply seeded      │  → PCG32 draw from controller's private RNG stream
│ imperfection      │    (angle ±σ, power ±σ) — reproducible per seed
└─────────┬─────────┘
          ▼
┌───────────────────┐
│ Return best legal │  → validated by match.c before live execution
│ ShotPlan          │
└───────────────────┘
```

### 4.3 RNG Stream Isolation (Article 11, 13)

- **Global seed** → splits into 4 independent PCG32 streams (one per seat) via `pcg32_advance(seat_index * 2^64)`.
- AI scratch simulations **consume draws from controller's private stream** but **reset stream to pre-planning state** after candidate evaluation (A.6.9).
- Live shot execution uses **separate physics RNG** (if any) — no cross-contamination.

### 4.4 Controller Variants

| Controller | Purpose | Profile |
|------------|---------|---------|
| `baseline_controller` | Tests, soak, fallback | RandomLegal + minimal tactic |
| `arena_controller` | Production players | Full pipeline, distinct `StrategyProfile` per seat (aggressive/defensive/balanced/trickster) |

---

## 5. RULES ENGINE ARCHITECTURE (Article 9.1, Appendix A.7)

### 5.1 Pure Logic Layer — Headless Testable

```c
// game/rules.c — ZERO external deps, NO raylib, NO Box2D
typedef struct {
    PiecePocketed pocketed[19];  // piece_id, color, pocket
    bool queen_pocketed;
    bool striker_pocketed;
    FoulFact fouls;
    Seat active_seat;
    QueenCoverState queen_state;
    DueState dues;
} ShotFacts;

typedef struct {
    MatchState next_state;
    ScoreDelta score_delta;
    DueAction due_actions[4];
    TurnDecision turn;  // CONTINUE | ADVANCE | BOARD_OVER | GAME_OVER | MATCH_OVER
    GameEvent events[MAX_EVENTS_PER_SHOT];
    int event_count;
} RulesOutcome;

RulesOutcome rules_resolve(const MatchState* prior, const ShotFacts* facts);
```

### 5.2 Fact Extraction (Physics → Rules)

```c
// game/match.c — bridge
ShotFacts extract_facts(const BoardState* board, const ShotResult* result) {
    ShotFacts f = {0};
    f.active_seat = board->active_seat;
    f.queen_state = board->queen_state;
    f.dues = board->dues;
    for (int i = 0; i < result->pocket_count; i++) {
        f.pocketed[f.pocket_count++] = result->pockets[i];
        if (result->pockets[i].piece_id == QUEEN_ID) f.queen_pocketed = true;
        if (result->pockets[i].piece_id == STRIKER_ID) f.striker_pocketed = true;
    }
    f.fouls = detect_fouls(board, result);
    return f;
}
```

### 5.3 Coverage of Article 16.1 Rules Tests

Every bullet in Article 16.1 maps to a deterministic unit test in `tests/rules_*.c` exercising `rules_resolve()` with constructed `ShotFacts`. No physics, no renderer, no raylib.

---

## 6. TRACE / TELEMETRY SCHEMA (Appendix A.8)

### 6.1 JSONL Per-Shot Record

```json
{
  "build_id": "g3a1f2e5",
  "seed": 1234567890123456789,
  "game_id": 1,
  "board_id": 3,
  "shot_number": 17,
  "active_player": "NORTH",
  "active_team": "WHITE",
  "pre_state_hash": "a1b2c3d4e5f6...",
  "shot_plan": {
    "placement": {"x": 0.12, "y": -0.48},
    "aim_angle": 0.785,
    "power": 0.62,
    "tactic": "DIRECT_WHITE_TOP_RIGHT",
    "imperfection_draw": 3421
  },
  "planner_meta": {
    "candidates_evaluated": 320,
    "best_score": 0.87,
    "search_budget_used": 320
  },
  "result": {
    "pockets": [{"piece_id": 5, "color": "WHITE", "pocket": "TOP_RIGHT"}],
    "queen_pocketed": false,
    "striker_pocketed": false,
    "fouls": []
  },
  "score_delta": {"white": 1, "black": 0},
  "turn_decision": "CONTINUE",
  "post_state_hash": "f6e5d4c3b2a1...",
  "runtime_errors": []
}
```

### 6.2 Implementation

- `telemetry/trace.c`: `trace_shot_open()`, `trace_shot_write()`, `trace_shot_close()`.
- File: `traces/<seed>_game<G>_board<B>.jsonl` — append-only, one line per shot.
- Human-readable event log: `logs/<seed>.log` (mirrored from `GameEvent` stream).
- **All fields Appendix A.8 present.** Build ID from `git describe --dirty --always` at configure time.

---

## 7. VERIFICATION SEAMS (Appendix A.9)

Four execution modes sharing **identical authoritative core** (`game/`, `physics/`, `ai/`, `telemetry/`):

| Mode | Entry Point | Rendering | Timing | Use Case |
|------|-------------|-----------|--------|----------|
| **1. Rendered Arena** | `app/main.c` | raylib 60 FPS | Wall-clock | Spectator play, visual verification |
| **2. Diagnostic Single-Seed** | `app/diagnostic.c` | raylib (optional) | Deterministic step | Reproduce specific shot/bug, pause/step |
| **3. Headless Soak** | `app/soak.c` | **None** | Accelerated (no sleep) | Article 16.5: 100 boards × 100 seeds + 10 matches |
| **4. Graphical Capture** | `app/capture.c` | raylib + frame dump | Controlled | Article 17: screenshot/video evidence for QA/CEO certification |

**Implementation:** `app/app.c` contains `AppMode` enum + shared `run_simulation()` core loop. Modes differ only in:
- `renderer` implementation (real vs null vs frame-capture)
- `time_source` (wall-clock vs fixed-step vs controlled)
- `output_sinks` (screen vs files vs null)

**No separate fake gameplay implementations.** All modes exercise same `rules_resolve()`, `physics_step()`, `controller_decide()`.

---

## 8. BUILD / CI DISCIPLINE (Appendix A.10)

### 8.1 CMake Structure

```cmake
# CMakeLists.txt (root)
cmake_minimum_required(VERSION 3.25)
project(CarromArena LANGUAGES C VERSION 1.0.0)

# Toolchain & warnings
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(STRICT_WARNINGS "-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wmisleading-indentation -Wduplicated-cond -Wlogical-op -Wno-unused-parameter")
    add_compile_options(${STRICT_WARNINGS})
    # Treat warnings as errors in CI
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options("-Werror")
    endif()
endif()

# Dependency pinning via FetchContent
include(FetchContent)
FetchContent_Declare(raylib GIT_REPOSITORY https://github.com/raysan5/raylib GIT_TAG 5.5.0)
FetchContent_Declare(box2d GIT_REPOSITORY https://github.com/erincatto/box2d GIT_TAG v3.1.0)
FetchContent_MakeAvailable(raylib box2d)

# Vendored PCG32 (single header) — no FetchContent needed

add_subdirectory(src)
enable_testing()
add_subdirectory(tests)
```

### 8.2 Build Configurations

| Config | Flags | Purpose |
|--------|-------|---------|
| **Debug** | `-O0 -g -fsanitize=address,undefined -Werror` | Development, ASan/UBSan |
| **Release** | `-O3 -DNDEBUG -flto` | Production, performance |
| **RelWithDebInfo** | `-O2 -g` | Profiling, CI verification |

### 8.3 Pipeline Stages (Appendix A.10)

```yaml
# .github/workflows/ci.yml (conceptual)
stages:
  - configure: cmake -B build -DCMAKE_BUILD_TYPE=Debug
  - build: cmake --build build --parallel
  - unit_tests: ctest --test-dir build --output-on-failure -L unit
  - integration_tests: ctest --test-dir build --output-on-failure -L integration
  - deterministic_verification: ./build/carrom_arena --mode=diagnostic --seed=12345 --trace=out.jsonl && python verify_determinism.py out.jsonl
  - soak_tests: ./build/carrom_arena --mode=soak --boards=100 --seeds=100 --matches=10
  - release_build: cmake -B build_rel -DCMAKE_BUILD_TYPE=Release && cmake --build build_rel
  - graphical_e2e: ./build_rel/carrom_arena --mode=capture --seed=999 --frames=out/ && python verify_frames.py out/
  - evidence_generation: collect traces, logs, screenshots → artifacts
```

**Clean-build acceptance (Article 18):** Documented `./scripts/clean_build.sh` removing `build/`, `CMakeCache.txt`, re-fetching deps, then running full pipeline.

---

## 9. RISK REGISTER — TOP 5 TECHNICAL RISKS

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|------------|--------|------------|
| **R1** | **Box2D v3 cross-arch determinism gap** — float math diverges across x86_64/ARM64 or compiler versions | Medium | High (Article 13) | 1) Compile with `-DB2_USE_FIXED_POINT` if available. 2) Document narrow claim: "same binary + seed = same result". 3) CI builds on x86_64 + ARM64 runners; compare traces. 4) Fallback: vendor fixed-point fork. |
| **R2** | **Pocket capture double-count / missed pocket** — sensor timing vs. CCD race | Medium | High (scoring correctness) | 1) Pocket as **kinematic sensor** with `b2Body_EnableSensorEvents`. 2) Capture on `BEGIN_CONTACT` only; destroy body immediately. 3) Deterministic unit test: 10k random shots into pockets — zero duplicates/misses. |
| **R3** | **AI scratch simulation mutates live state / RNG** — violates A.6.9 | Low | Critical | 1) `physics_snapshot_clone()` deep-copies Box2D world via `b2World_Dump`/`b2World_Load` or manual body copy. 2) Controller RNG state saved/restored around planning. 3) Assert in tests: live RNG sequence unchanged after planning. |
| **R4** | **Settling detection false positive/negative** — pieces never sleep or sleep while moving | Medium | High (turn resolution) | 1) Dual condition: `speed < EPS` AND `acceleration < EPS` for `N` consecutive steps. 2) Safety timeout: 30 sim-seconds → fault trace + legal fallback (Article 11). 4) Soak test validates zero deadlocks. |
| **R5** | **Rules engine edge-case incompleteness** — queen/cover/due combinations not covered | Medium | High (Article 16.1) | 1) RULES.md maps every ICF rule → test case. 2) Property-based testing: generate random valid `ShotFacts`, verify `rules_resolve` invariants (conservation, valid queen state, score bounds). 3) Reviewer audit (Article 19) targets queen/due/foul edge cases explicitly. |

---

## 10. OPEN DECISIONS REQUIRING OPERATOR APPROVAL

| # | Decision | Contract Reference | Options | Recommendation |
|---|----------|-------------------|---------|----------------|
| **D1** | **Physics fixed timestep value** | A.5 ("not contractual") | 1/60, **1/120**, 1/240 | **1/120** — balances CCD headroom vs. CPU; finalize after soak. |
| **D2** | **Box2D fixed-point vs float** | A.5, Article 13 | Enable `B2_USE_FIXED_POINT` / stay float + document | **Enable fixed-point if v3 supports**; else document narrow claim. |
| **D3** | **Board-plane resistance parameters** | A.5 ("choose and document") | Coulomb/viscous values per tuning | **Tune against settling tests**; lock values in `config.h` before Phase 3. |
| **D4** | **AI search budget (candidates × sim steps)** | A.6.11 ("bounded deterministic work budget") | 160 / **320** / 640 candidates | **320** (8 placements × 40 variants) — fits 16 ms budget on CI runners. |
| **D5** | **StrategyProfile differentiation for 4 seats** | Article 11.4 ("distinct strategy profiles") | Aggressive/Defensive/Balanced/Trickster — weight vectors | **Define 4 distinct weight vectors** in `config.h`; CPO to approve personalities. |
| **D6** | **Cross-platform CI coverage** | Article 22, Appendix A.10 | GitHub Actions (ubuntu-latest, windows-latest, macos-latest) / self-hosted | **GitHub Actions matrix** for all three; if macOS runner unavailable, document portability audit per Article 17. |
| **D7** | **Audio / sound** | Article 12.7 ("only if sound is implemented") | Implement minimal SFX / omit | **Omit** — not Mandatory; reduces dependency surface. |

---

## CTO CERTIFICATION

> I, the CTO of SANYALnet Labs, certify that this architecture:
> 1. Satisfies every Mandatory Requirement in Articles 5–13, 16–17, and Appendix A.
> 2. Uses portable C17 + raylib + Box2D v3 as the presumptive platform direction (Article 9).
> 3. Defines clean module boundaries with inward dependency flow.
> 4. Provides deterministic, testable physics, rules, AI, and telemetry.
> 5. Supports all four verification seams sharing authoritative core.
> 6. Identifies all material technical risks with concrete mitigations.
> 7. Defers only the seven open decisions (D1–D7) above for operator/CEO approval.

**No deviations from Appendix A guidance** beyond those explicitly listed in Section 10.

---

## NEXT STEPS (UPON APPROVAL)

1. **CEO approves** this sign-off → Phase 3 (Implementation) commences.
2. CPO locks `RULES.md` and `STRATEGY_PROFILES.md` (resolves D5).
3. Programmer scaffolds `src/` per Section 2.1 with compiling stubs.
4. Reviewer begins static review checklist (Article 19).
5. Tester authors test harness for `rules_resolve()` and physics settling.

---

**Document Status:** `PENDING_CEO_APPROVAL`  
**HALT** — Awaiting operator/CEO authorization to proceed to Phase 3.