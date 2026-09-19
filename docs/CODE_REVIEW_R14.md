# R14a Code Review Synthesis Report

## 1. Coverage Table
Aggregate audit of the codebase to ensure physics fidelity, stability, and architectural integrity.

| Module | File | Total Lines | Lines Reviewed | Status |
| :--- | :--- | :---: | :---: | :--- |
| `physics` | `physics_snapshot.h` | 63 | 63 | ✅ Complete |
| `physics` | `physics_snapshot.c` | 137 | 137 | ✅ Complete |
| `physics` | `physics.h` | 92 | 92 | ✅ Complete |
| `physics` | `physics.c` | 766 | 766 | ✅ Complete |
| `game` | `board.c` | 252 | 252 | ✅ Complete |
| `game` | `board.h` | 43 | 43 | ✅ Complete |
| `game` | `events.c` | 78 | 78 | ✅ Complete |
| `game` | `events.h` | 28 | 28 | ✅ Complete |
| `game` | `match.c` | 41 | 41 | ✅ Complete |
| `game` | `match.h` | 22 | 22 | ✅ Complete |
| `game` | `rules.c` | 458 | 458 | ✅ Complete |
| `game` | `rules.h` | 35 | 35 | ✅ Complete |
| `game` | `scoring.c` | 62 | 62 | ✅ Complete |
| `game` | `scoring.h` | 27 | 27 | ✅ Complete |
| `common` | `types.c` | 189 | 189 | ✅ Complete |
| `common` | `types.h` | 259 | 259 | ✅ Complete |
| `common` | `rng.h` | 147 | 147 | ✅ Complete |
| `common` | `pcg32.h.in` | 76 | 76 | ✅ Complete |
| `common` | `strategy_profiles.h` | 123 | 123 | ✅ Complete |
| `app` | `app.h` | 88 | 88 | ✅ Complete |
| `app` | `app.c` | 947 | 947 | ✅ Complete |
| `app` | `main.c` | 22 | 22 | ✅ Complete |
| `app` | `capture.c` | 18 | 18 | ✅ Complete |
| `app` | `diagnostic.c` | 18 | 18 | ✅ Complete |
| `app` | `soak.c` | 19 | 19 | ✅ Complete |
| `platform` | `platform.h` | 54 | 54 | ✅ Complete |
| `platform` | `platform.c` | 158 | 158 | ✅ Complete |
| `render` | `board_view.c` | 614 | 614 | ✅ Complete |
| `render` | `effects.c` | 129 | 129 | ✅ Complete |
| `render` | `hud.c` | 92 | 92 | ✅ Complete |
| `render` | `renderer.c` | 447 | 447 | ✅ Complete |
| `render` | `board_view.h` | 17 | 17 | ✅ Complete |
| `render` | `effects.h` | 18 | 18 | ✅ Complete |
| `render` | `hud.h` | 17 | 17 | ✅ Complete |
| `render` | `renderer.h` | 74 | 74 | ✅ Complete |
| `ai` | `baseline_controller.c` | 11 | 11 | ✅ Complete |
| `ai` | `shot_evaluator.c` | 219 | 219 | ✅ Complete |
| `ai` | `controller.c` | 252 | 252 | ✅ Complete |
| `ai` | `shot_candidates.c` | 334 | 334 | ✅ Complete |
| `ai` | `shot_evaluator.h` | 31 | 31 | ✅ Complete |
| `ai` | `controller.h` | 50 | 50 | ✅ Complete |
| `ai` | `arena_controller.c` | 12 | 12 | ✅ Complete |
| `ai` | `shot_candidates.h` | 40 | 40 | ✅ Complete |
| `telemetry` | `trace.h` | 62 | 62 | ✅ Complete |
| `telemetry` | `trace.c` | 684 | 684 | ✅ Complete |
| `telemetry` | `replay.c` | 148 | 148 | ✅ Complete |
| `tests` | `CMakeLists.txt` | 196 | 196 | ✅ Complete |
| `tests` | `test_ai.c` | 234 | 234 | ✅ Complete |
| `tests` | `test_capture.c` | 114 | 114 | ✅ Complete |
| `tests` | `test_integration.c` | 152 | 152 | ✅ Complete |
| `tests` | `test_physics.c` | 308 | 308 | ✅ Complete |
| `tests` | `test_regression.c` | 496 | 496 | ✅ Complete |
| `tests` | `test_rules.c` | 359 | 359 | ✅ Complete |
| `tests` | `test_trace_circular.c` | 415 | 415 | ✅ Complete |
| `build` | `CMakeLists.txt` | 167 | 167 | ✅ Complete |
| `build` | `src/CMakeLists.txt` | 231 | 231 | ✅ Complete |
| `ci` | `.github/workflows/ci.yml` | 122 | 122 | ✅ Complete |
| `scripts` | `clean_build.sh` | 21 | 21 | ✅ Complete |
| `scripts` | `kill-all-runs.sh` | 131 | 131 | ✅ Complete |
| `scripts` | `soak_verification.sh` | 176 | 176 | ✅ Complete |
| `scripts` | `git-hooks/pre-push` | 21 | 21 | ✅ Complete |

## 2. Findings List

### Critical
*None identified.*

### High
- **[PH-01] High | Physics - Snapshot Rotation Error**
  - **File**: `src/physics/physics_snapshot.c:94`
  - **Defect Class**: Physics Violation
  - **Evidence**: `(b2Rot){ cosf(snap->pieces[i].angle * 0.5f), sinf(snap->pieces[i].angle * 0.5f) }`
  - **Why it matters**: Incorrect rotation reconstruction during snapshot restore. This causes AI scratch simulations to be physically invalid.
  - **Recommended Fix**: Use `cosf(angle)` and `sinf(angle)` without the `0.5f` multiplier.

- **[GL-001] High | Game Logic - Spec Violation (Radii)**
  - **File**: `src/common/types.h:38-40`
  - **Defect Class**: Spec Violation
  - **Evidence**: `POCKET_RADIUS_NORM 0.030f`, `PIECE_RADIUS_NORM 0.021f`, `STRIKER_RADIUS_NORM 0.028f`
  - **Why it matters**: Normalized values do not match ICF specs for a 74cm board. This fundamentally changes the difficulty and physics of the game.
  - **Recommended Fix**: Update normalized values to: Pocket $\approx$ 0.060, Piece $\approx$ 0.043, Striker $\approx$ 0.055.

- **[AI-01] High | AI - Determinism Break in RNG Handling**
  - **File**: `src/ai/controller.c:170-226`
  - **Defect Class**: Determinism Break
  - **Evidence**: `rng_snapshot(rng)` $\rightarrow$ `pcg32_random_float(rng)` $\rightarrow$ `rng_restore(rng, rng_snap)`
  - **Why it matters**: Noise draws are erased from the RNG stream history but the resulting shot is kept. This breaks the strict temporal determinism required for trace verification.
  - **Recommended Fix**: Ensure RNG draws for imperfection are either part of the permanent stream or logically isolated from the verification stream.

- **[TEL-01] High | Telemetry - Circular Buffer Wrap Overwrite**
  - **File**: `src/telemetry/trace.c:135-142`
  - **Defect Class**: Circular Trace Wraparound Bug
  - **Evidence**: `fseek(f, (long)data_start, SEEK_SET); fwrite(line, 1, line_len, f);`
  - **Why it matters**: Fails to handle split records at the buffer end, leaving stale fragments that corrupt JSONL reading.
  - **Recommended Fix**: Zero out remaining space at the end of the buffer when wrapping.

- **[TEL-02] High | Telemetry - Ignored Wrap-around in Reader**
  - **File**: `src/telemetry/trace.c:577`
  - **Defect Class**: Circular Trace Wraparound Bug
  - **Evidence**: `(void)write_offset;`
  - **Why it matters**: Reads physical order instead of logical temporal order in wrapped files, corrupting replay/analysis.
  - **Recommended Fix**: Use `write_offset` to linearize the buffer before extraction.

### Medium
- **[PH-02] Medium | Physics - Inconsistent Pocket Detection**
  - **File**: `src/physics/physics.c:368-490`
  - **Defect Class**: Logic Flaw
  - **Evidence**: Parallel systems `physics_check_pocket_events` and `physics_check_pockets` (manual distance check).
  - **Why it matters**: Divergence between sensor and distance checks can lead to non-deterministic pocketing results.
  - **Recommended Fix**: Unify pocketing trigger logic.

- **[GL-002] Medium | Game Logic - Arbitrary Piece Placement**
  - **File**: `src/game/board.c:106`
  - **Defect Class**: Logic Error
  - **Evidence**: `board->pieces[0].position = (Vec2){ INITIAL_RADIUS * 0.5f, INITIAL_RADIUS * 0.5f };`
  - **Why it matters**: Manual offset is non-symmetric and arbitrary.
  - **Recommended Fix**: Implement a proper formation generator.

- **[DEF-001] Medium | App State - Thinking Phase Budget Gap**
  - **File**: `src/app/app.c:292-295`
  - **Defect Class**: Visualization/UX
  - **Evidence**: `if (ctx->thinking_min_wall < 2.0) ctx->thinking_min_wall = 2.0;`
  - **Why it matters**: Disconnect between configured AI budget and actual wall-clock behavior in Rendered mode.
  - **Recommended Fix**: Make minimum delay optional or scale with budget.

- **[DEF-005] Medium | App State - Phase Transition Hole**
  - **File**: `src/app/app.c:297-299`
  - **Defect Class**: Phase Transition
  - **Evidence**: `case TURN_BOARD_OVER: break;` (relies on external logic)
  - **Why it matters**: Creates a 1-frame "gap" where game state is in limbo before next board's `PHASE_THINKING`.
  - **Recommended Fix**: Explicitly set `PHASE_THINKING` immediately after `match_start_board`.

- **[AI-02] Medium | AI - Potential Buffer Overflow in Tactics**
  - **File**: `src/ai/shot_candidates.c:38-44`
  - **Defect Class**: Algorithmic Error
  - **Evidence**: `Vec2 targets[19];` loop over `MAX_PIECES`.
  - **Why it matters**: If `MAX_PIECES` > 19, this causes a stack overflow.
  - **Recommended Fix**: Size `targets` array using `MAX_PIECES`.

- **[AI-03] Medium | AI - Incorrect Aiming Calculation**
  - **File**: `src/ai/shot_candidates.c:64`
  - **Defect Class**: Algorithmic Error
  - **Evidence**: `float aim = atan2f(to_target.y, to_target.x);`
  - **Why it matters**: Aims at center of target; fails to account for required contact point for pocketing.
  - **Recommended Fix**: Calculate aim based on required contact point.

- **[TEL-03] Medium | Telemetry - JSON Buffer Overflow**
  - **File**: `src/telemetry/trace.c:175-190`
  - **Defect Class**: Memory Bounds
  - **Evidence**: `char pockets_json[2048] = "["; ... strcat(pockets_json, "]");`
  - **Why it matters**: Potential overflow if the buffer is exactly filled before the closing bracket.
  - **Recommended Fix**: Use `snprintf` or reserve space for the bracket.

- **[TEL-04] Medium | Telemetry - Use After Free in Replay**
  - **File**: `src/telemetry/replay.c:88`
  - **Defect Class**: Memory Bounds / Logic
  - **Evidence**: `controller_destroy(controller)` called at line 84, but `controller` used in fallback at line 88.
  - **Why it matters**: Immediate crash or undefined behavior during shot validation failure.
  - **Recommended Fix**: Move `controller_destroy` to the end of the function.

### Low
- **[PH-03] Low | Physics - Proxy Acceleration Check**
  - **File**: `src/physics/physics.c:581-584`
  - **Defect Class**: Physics Violation
  - **Evidence**: `float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;`
  - **Why it matters**: Measures theoretical board resistance, not actual body acceleration.
  - **Recommended Fix**: Measure $\Delta v / \Delta t$.

- **[DEF-002] Low | App State - Jittery First Turn**
  - **File**: `src/app/app.c:380-383`
  - **Defect Class**: Logic Inconsistency
  - **Evidence**: 0.5s min delay for turn 1 vs 2.0s for others.
  - **Why it matters**: Poor UX feel.
  - **Recommended Fix**: Unify delays.

- **[DEF-003] Low | App State - Division by Zero Risk**
  - **File**: `src/app/app.c:496-497`
  - **Defect Class**: Robustness
  - **Evidence**: Budget ternary check vs HUD scaling.
  - **Why it matters**: Extremely low budget causes HUD jumps.
  - **Recommended Fix**: Validate budget at context creation.

- **[DEF-004] Low | App State - State Machine Dead-end**
  - **File**: `src/app/app.c:444-445`
  - **Defect Class**: State Machine
  - **Evidence**: `case PHASE_IDLE: break;`
  - **Why it matters**: State machine "hole".
  - **Recommended Fix**: Document or add transition.

- **[RND-01] Low | Render - HUD Duplication**
  - **File**: `src/render/renderer.c:126-190`
  - **Defect Class**: Maintenance Risk
  - **Evidence**: `draw_hud_sidebar` vs `hud_draw`.
  - **Recommended Fix**: Use `hud_draw`.

- **[RND-02] Low | Render - Angle Format Duplication**
  - **File**: `src/render/renderer.c:115-123`
  - **Defect Class**: Code Duplication
  - **Evidence**: `format_angle_deg_min` duplicated.
  - **Recommended Fix**: Move to common utility.

- **[RND-03] Low | Render - Precision Loss in Coordinates**
  - **File**: `src/render/board_view.c:158`
  - **Defect Class**: Rendering Artifacts
  - **Evidence**: `(int)shoulders_center.x`
  - **Why it matters**: Potential jitter during slow movement.
  - **Recommended Fix**: Use `DrawEllipseV`.

- **[RND-04] Low | Render - Layout Recomputation Waste**
  - **File**: `src/render/renderer.c:345`
  - **Defect Class**: Performance
  - **Evidence**: `layout_compute` called every frame.
  - **Recommended Fix**: Only call on window resize.

- **[RND-05] Low | Render - Aim Line Gap**
  - **File**: `src/render/board_view.c:233`
  - **Defect Class**: Layout Math
  - **Evidence**: `boundary_dist - 0.01f`
  - **Why it matters**: Potential gaps/overlaps on different resolutions.
  - **Recommended Fix**: Scale offset by world-to-screen.

- **[AI-04] Low | AI - Variant Buffer Overflow**
  - **File**: `src/ai/shot_candidates.c:297-298`
  - **Defect Class**: Algorithmic Error
  - **Evidence**: `out_variants` filled up to `max_variants` (fixed 40).
  - **Why it matters**: Potential overflow if `max_variants` > 40.
  - **Recommended Fix**: Clamp `max_variants`.

- **[ID-01] Low | Tests - AI Scoring Stub**
  - **File**: `tests/test_ai.c:184`
  - **Defect Class**: Coverage Gap
  - **Evidence**: `TEST_ASSERT_TRUE(true); // This is a stub test`
  - **Recommended Fix**: Implement mock board scoring test.

- **[ID-03] Low | Tests - Integration Stubs**
  - **File**: `tests/test_integration.c:19`
  - **Defect Class**: Coverage Gap
  - **Evidence**: `test_app_config_default` is stub.
  - **Recommended Fix**: Implement mock CLI tests.

- **[ID-04] Low | Tests - CI Infrastructure Gap**
  - **File**: `.github/workflows/ci.yml:65`
  - **Defect Class**: Portability
  - **Evidence**: `Skip capture test on Windows CI`
  - **Recommended Fix**: Implement Mesa/Software GL for Windows.

### Medium (from Verification/Prior audits)
- **[ID-02] Medium | Tests - Shallow Physics Verification**
  - **File**: `tests/test_physics.c:184`
  - **Defect Class**: False-passing
  - **Evidence**: `TEST_ASSERT_TRUE(true); // Just ensure it doesn't crash`
  - **Recommended Fix**: Implement actual pocket capture verification.

- **[MA-04] Medium | Game Logic - Board Sync Stub**
  - **File**: `src/game/board.c:249`
  - **Defect Class**: Missing Implementation
  - **Evidence**: `// Stub - will be implemented when physics_snapshot is ready`
  - **Recommended Fix**: Implement `board_sync_from_physics`.

## 3. Root-Cause Map

- **Physics behaving wrongly (striker direction, deceleration)**
  - $\rightarrow$ [PH-01] Snapshot Rotation (Crucial for AI simulations)
  - $\rightarrow$ [GL-001] Incorrect Radii (Fundamental physics shift)
  - $\rightarrow$ [PH-03] Proxy Acceleration (Accuracy issue)

- **Initial piece placement wrong**
  - $\rightarrow$ [GL-002] Arbitrary Piece 0 offset

- **Pieces bouncing back from pockets / pocketing issues**
  - $\rightarrow$ [PH-02] Inconsistent Pocket Detection
  - $\rightarrow$ [ID-02] Shallow Pocket Verification (Test gap)

- **Long startup countdown / UX Jitter**
  - $\rightarrow$ [DEF-001] Thinking Budget Gap
  - $\rightarrow$ [DEF-002] Inconsistent Turn-1 Delay

- **Systemic Stability / Data Integrity**
  - $\rightarrow$ [TEL-01, TEL-02] Circular Buffer Logic
  - $\rightarrow$ [AI-01] RNG Determinism Break
  - $\rightarrow$ [TEL-04] Use-After-Free in Replay
  - $\rightarrow$ [AI-02, AI-04, TEL-03] Buffer Overflow risks

## 4. Not Verified Section
- **Cross-Module Render/Physics Interaction**: While individual files were reviewed, the precise timing of `physics_sync_from_board` vs `render` frame updates was not formally verified for race conditions.
- **AI Convergence**: The correctness of the `shot_evaluator` scoring weights was not audited against a ground-truth expert game.

## 5. Prioritised Fix Plan (R14b)

1. **Priority 1 (Critical/High Stability)**
   - Fix [TEL-04] Use-After-Free in `replay.c` (Crash risk).
   - Fix [PH-01] Snapshot Rotation in `physics_snapshot.c` (AI Validity).
   - Fix [AI-01] RNG Determinism in `controller.c` (Trace Validity).
   - Fix [TEL-01, TEL-02] Circular Trace Logic (Data Integrity).

2. **Priority 2 (High Spec/Physics Fidelity)**
   - Update [GL-001] Normalized Radii to ICF Spec.
   - Implement [MA-04] `board_sync_from_physics`.
   - Fix [AI-02, AI-04, TEL-03] Memory buffer overflows.

3. **Priority 3 (Medium Logic/UX)**
   - Fix [PH-02] Unify Pocket Detection.
   - Fix [DEF-005] Phase Transition Hole in `app.c`.
   - Fix [AI-03] AI Aiming Logic.
   - Fix [GL-002] Board Formation Generator.
   - Fix [DEF-001, DEF-002] App Thinking Phase Timing.

4. **Priority 4 (Low Polish/Tests)**
   - Implement [ID-01, ID-02, ID-03] Test stubs.
   - Fix [RND-01, RND-02] Code duplication in Renderer.
   - Fix [RND-03, RND-04, RND-05] Render precision and waste.
   - Fix [PH-03] Actual acceleration measurement.
