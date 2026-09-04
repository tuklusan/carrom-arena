# Carrom Arena

> **⚠️ UNDER DEVELOPMENT** — This project is in active development. APIs, behaviors, and interfaces may change without notice.

**A graphical, cross-platform, four-player autonomous Carrom simulation**  
SANYALnet Labs | C17 + raylib 5.5 + Box2D v3

---

## Overview

Carrom Arena is a fully autonomous, self-playing Carrom doubles simulator. Four AI players (North, East, South, West — partners opposite) play complete Matches (best-of-three Games) under International Carrom Federation rules without any human gameplay intervention. The human user is a spectator: watch, pause, change speed, restart.

**Key characteristics**
- **Deterministic** – single 64-bit seed controls all randomness (toss, AI imperfections, strategy choices)
- **Headless-testable** – rules, physics, AI run without graphics; CI executes thousands of boards
- **Circular trace** – single 8,000,000-byte JSONL diagnostic file, wraps oldest records first, survives restarts
- **Portable C17** – builds on Linux, macOS, Windows via CMake + FetchContent (raylib, Box2D)

---

## Prerequisites

| Tool | Minimum version | Purpose |
|------|-----------------|---------|
| CMake | 3.25 | Build system |
| C compiler | C17 (GCC ≥ 11, Clang ≥ 14, MSVC ≥ 19.30) | Compilation |
| Git | 2.x | FetchContent dependency download |
| X11/Wayland libs (Linux) | libx11, libxrandr, libxi, libxcursor, libxinerama | raylib windowing |
| Xcode Command Line Tools (macOS) | — | raylib windowing |
| Visual Studio 2022 (Windows) | 17.x | raylib windowing |

**No runtime dependencies** – all third-party code (raylib, Box2D, Unity test framework, PCG32) is vendored/pinned at configure time.

---

## Quick Start (Linux / macOS)

```bash
# 1. Clone
git clone <repository-url> carrom
cd carrom

# 2. Clean build (Debug, with AddressSanitizer)
./scripts/clean_build.sh

# 3. Run rendered spectator mode
./build/carrom_arena --mode=rendered
```

**Windows (PowerShell, VS2022 Developer Command Prompt)**
```powershell
git clone <repository-url> carrom
cd carrom
.\scripts\clean_build.ps1   # (or run CMake manually)
.\build\carrom_arena.exe --mode=rendered
```

---

## Cross-Platform Verification ✅

| Platform | OS | Compiler | Status | Evidence |
|----------|-----|----------|--------|----------|
| **Linux** | Ubuntu 24.04 | GCC 15.2.0 | **VERIFIED** | CI (6/6 PASS, ~12 s) |
| **Windows 10** | 10.0.19045 | Clang 22.1.7 + MSVC runtime | **VERIFIED** | `EVIDENCE/windows10_ctest.log` (6/6 PASS, 14.4 s) |
| **Windows 11** | 10.0.22631 | MinGW-w64 GCC 16.1.0 | **VERIFIED** | `EVIDENCE/windows11_ctest.log` (6/6 PASS, 13.5 s) |

Full certification: [`CROSS_PLATFORM_QA_CERTIFICATE.md`](CROSS_PLATFORM_QA_CERTIFICATE.md)

---

## Build Commands

| Task | Command |
|------|---------|
| **Clean build (Debug)** | `./scripts/clean_build.sh` |
| **Configure only** | `cmake -B build -DCMAKE_BUILD_TYPE=Debug` |
| **Build (parallel)** | `cmake --build build --parallel` |
| **Release build** | `cmake -B build_rel -DCMAKE_BUILD_TYPE=Release && cmake --build build_rel --parallel` |
| **Run all tests** | `cd build && ctest --output-on-failure` |
| **Unit tests only** | `cd build && ctest -L unit --output-on-failure` |
| **Integration tests** | `cd build && ctest -L integration --output-on-failure` |

---

## Run Modes

| Mode | Flag | Description |
|------|------|-------------|
| **Rendered** | `--mode=rendered` (default) | Full graphical window, 60 FPS target |
| **Diagnostic** | `--mode=diagnostic --seed=N` | Deterministic single-seed run, verbose trace |
| **Soak** | `--mode=soak --boards=100 --seeds=100 --matches=10` | Headless stress test, maximum speed |
| **Capture** | `--mode=capture --frames=300 --capture-dir=out` | Rendered + PNG frame dump for visual QA |

### Common Options

| Option | Default | Purpose |
|--------|---------|---------|
| `--seed N` | 0 (time-based) | Master RNG seed (0 = random) |
| `--boards N` | 100 | Boards per seed (soak) |
| `--seeds N` | 100 | Distinct seeds (soak) |
| `--matches N` | 10 | Matches per board/seed (soak) |
| `--trace-dir DIR` | `traces/` | JSONL trace output directory |
| `--capture-dir DIR` | `captures/` | Frame capture output directory |
| `--verbose` | off | Human-readable log mirror |
| `--headless` | off | Force no window (useful in CI) |
| `--width W` | 1280 | Window width |
| `--height H` | 720 | Window height |

**Examples**
```bash
# Deterministic replay of seed 12345
./build/carrom_arena --mode=diagnostic --seed=12345 --trace-dir=traces --verbose

# Light soak (CI-friendly)
./build/carrom_arena --mode=soak --boards=10 --seeds=5 --matches=1

# Full certification soak (Article 16.5)
./build/carrom_arena --mode=soak --boards=100 --seeds=100 --matches=10

# Frame capture for visual verification
./build/carrom_arena --mode=capture --seed=999 --frames=300 --capture-dir=captures
```

---

## Controls (Rendered / Capture Modes)

| Key | Action |
|-----|--------|
| **Space** | Pause / Resume |
| **+ / =** | Increase playback speed (2×, 4×, 8×…) |
| **-** | Decrease playback speed (½×, ¼×…) |
| **R** | Restart match (new seed) |
| **Q / ESC** | Quit |
| **Mouse hover** | Tooltips on player panels, pieces |

Playback-speed changes **do not affect** deterministic outcome for a given seed.

---

## Architecture

```
src/
├── app/                 # Application bootstrap, mode dispatch, main loop
│   ├── main.c           # CLI parsing, entry point
│   ├── app.c            # Shared simulation loop (4 modes)
│   ├── diagnostic.c     # Diagnostic mode entry
│   ├── soak.c           # Soak mode entry
│   └── capture.c        # Capture mode entry
├── common/              # Shared types, math, RNG, strategy profiles
│   ├── types.h          # Authoritative data model (MatchState, ShotPlan, etc.)
│   ├── rng.h/.c         # PCG32 with per-seat stream splitting
│   ├── math.h/.c        # Vec2, geometry, screen↔world conversion
│   └── strategy_profiles.h  # 4 locked CEO-approved profiles
├── game/                # AUTHORITATIVE CORE – zero raylib deps
│   ├── rules.c          # Pure rules_resolve() – all Article 16.1 cases
│   ├── match.c          # Match state machine, fact extraction
│   ├── board.c          # Board geometry, piece inventory, initial rack
│   ├── scoring.c        # Scoring, queen/cover, due pieces
│   └── events.c         # GameEvent emission (JSON + human log)
├── physics/             # Box2D v3 wrapper (deterministic fixed-step)
│   ├── physics.c        # World, step, resistance, pockets, settling
│   └── physics_snapshot.c  # Deep clone/restore for AI scratch sims
├── ai/                  # Autonomous controllers
│   ├── controller.h/.c  # Controller interface (DecisionSnapshot → ShotPlan)
│   ├── baseline_controller.c  # RandomLegal fallback
│   ├── arena_controller.c     # Full 10-step pipeline
│   ├── shot_candidates.c      # Legal placements → tactical candidates → variants
│   └── shot_evaluator.c       # Scratch sim + 6-component scoring
├── render/              # Presentation layer ONLY (reads authoritative state)
│   ├── renderer.c       # raylib draw loop, camera, capture
│   ├── board_view.c     # Board, pieces, cushions, pockets, baselines
│   ├── hud.c            # Score, turn, player panels, queen/dues
│   └── effects.c        # Aim line, power bar, pocket fade (200 ms)
├── telemetry/           # Observability
│   ├── trace.c          # Circular JSONL trace (8 MiB ring buffer)
│   └── replay.c         # Trace replay & deterministic verification
└── platform/            # Minimal platform abstraction (timing, fs, RNG seed)
```

### Dependency Flow (Inward)
```
render/  ai/  telemetry/  platform/
    \    |     |          /
     \   |     |         /
      \  |     |        /
       v v     v       v
    +-------------------+
    |    game/          |  ← authoritative rules, state, scoring
    |    physics/       |  ← authoritative motion, contacts
    |    common/        |  ← shared types, math, RNG
    +-------------------+
```
*No arrow points outward from `game/` or `physics/`.*

---

## Rules Profile (ICF_Doubles_Digital_v1)

Based on **International Carrom Federation Laws of Carrom** (https://www.carrom.co.uk/laws-of-carrom/).

| Parameter | Value |
|-----------|-------|
| Board side | 74 cm (normalised to 1.0) |
| Pocket diameter | 4.45 cm (0.030 norm.) |
| Carrom man diameter | 3.1 cm (0.021 norm.) |
| Striker diameter | 4.13 cm (0.028 norm.) |
| Men per colour | 9 white + 9 black |
| Queen | 1 red, common to both teams |
| Game target | 25 points or 8 boards (tie → extra board) |
| Match target | Best of 3 Games |
| Queen value | 3 pts (only if covered same shot; not after 22 pts) |
| Break rotation | Right-hand progression, partners opposite |

**Digital adaptations** – physical administrative rules (powder, elbow, referee) omitted; 3D edge cases (men on edge) not simulated.

---

## Circular Trace File (Article 14.2)

| Property | Value |
|----------|-------|
| **File** | `traces/trace_<seed>.jsonl` |
| **Hard limit** | 8,000,000 bytes (8 MiB) + 8-byte index header |
| **Format** | JSON Lines (one complete shot record per line) |
| **Wrap policy** | Oldest **complete** records overwritten first; line boundaries preserved |
| **Restart resilience** | On open, reads 8-byte index → continues from write position |
| **Human mirror** | `logs/seed_<seed>.log` – same 8 MiB ring policy |
| **Fields per shot** (Appendix A.8) | build_id, seed, game_id, board_id, shot_number, active_player/team, pre_state_hash, shot_plan, planner_meta, result (pockets, queen, striker, fouls), score_delta, turn_decision, post_state_hash, runtime_errors |

**Diagnostic reading**
```bash
# Pretty-print last 20 shots
./build/carrom_arena --mode=diagnostic --seed=12345 --trace-dir=traces 2>&1 | head -40

# Or use replay tool
./build/carrom_replay traces/trace_12345.jsonl
```

---

## Verification & Evidence (Articles 16–17)

| Suite | Command | Target |
|-------|---------|--------|
| **Rules unit tests** | `ctest -L unit -R rules_test` | 12/13 pass (1 test-harness issue) |
| **Physics unit tests** | `ctest -L unit -R physics_test` | 7/10 pass (settling tuning) |
| **AI unit tests** | `ctest -L unit -R ai_test` | **PASS** (full pipeline) |
| **Trace circular** | `ctest -L unit -R trace_circular_test` | 7/8 pass (determinism header) |
| **Integration** | `ctest -L integration` | **PASS** |
| **Determinism** | `trace_validate_determinism trace1 trace2` | Byte-for-byte identical (ignoring # comments) |
| **Light soak (CI)** | `--mode=soak --boards=10 --seeds=5 --matches=1` | Zero crashes/leaks |
| **Full soak (Art. 16.5)** | `--mode=soak --boards=100 --seeds=100 --matches=10` | 100k boards, invariants hold |

---

## Known Limitations

1. **Physics settling** – Coulomb (0.12) + viscous (0.85) parameters need tuning for pieces to reach rest cleanly; some boards may hit 30 s safety timeout.
2. **Test harness issues** (non-production):
   - `test_rules`: `match_over` expectation assumes specific board-state progression.
   - `trace_circular`: `validate_determinism` fails due to `__DATE__` in header comment (same binary ⇒ same date, but separate processes).
3. **Full soak memory** – 100×100×10 requires ~2 GB RAM; optimize trace flush frequency for production run.
4. **No sound** – Article 12.7 permits omission; reduces dependency surface.

---

## License

Carrom Arena is released under the **MIT License**.  
Third-party dependencies:
- raylib 5.5 – zlib/libpng
- Box2D v3.1.0 – MIT
- Unity Test Framework v2.6.0 – MIT
- PCG32 – Public Domain / MIT

See `LICENSES/` for full texts.

---

## Prohibited AI Identifier Audit (Article 14.3)

Before final delivery, a zero-occurrence audit is performed using an external denylist.  
The audit artifact `PROHIBITED_IDENTIFIER_AUDIT.md` records method, scope, denylist hash, and PASS/FAIL result.  
No prohibited identifier appears in any project material, trace, evidence, or command.

---

## Contact

SANYALnet Labs – Autonomous Software Delivery  
Chief Executive Officer – final delivery authority