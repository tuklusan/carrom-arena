# Code Review R14c - Part 1 Findings

## Root-Cause Analysis

### (a) Physics Symptoms (General)
- **Observation**: Board resistance is applied post-step in `physics_apply_board_resistance`.
- **Finding**: The logic `new_speed = speed - decel * PHYSICS_DT` in `src/physics/physics.c:274` is a linear approximation of deceleration. While standard, the `BOARD_COULOMB` (0.50) and `BOARD_VISCOUS` (2.00) values are high, and applying this *after* the Box2D step may lead to energy loss that doesn't perfectly align with the integrator.
- **Violation**: Potentially inconsistent energy dissipation compared to Box2D's internal friction.

### (b) Pocket Bounce
- **Observation**: Pieces may "bounce" out of pockets or fail to be captured.
- **Finding**: 
    - `physics_check_pocket_events` (Box2D sensors) is the primary mechanism.
    - `physics_check_pockets` (fallback) requires `pressed_against_cushion` (`src/physics/physics.c:410`).
    - **Issue**: In `physics_check_pocket_events` (`src/physics/physics.c:342`), pieces are pocketed and destroyed *immediately* upon `b2SensorBeginTouchEvent`. However, if a piece hits the cushion *before* the sensor center, it might bounce away before the sensor triggers, or the sensor might trigger but the piece is physically pushed out by the collision resolution in the same step.
    - **Violation**: Lack of a "capture window" or "sink" volume. The current system relies on a thin sensor and a strict cushion-press check in the fallback.

### (c) 15s Countdown / Timers
- **Observation**: User mentioned a "15s source".
- **Finding**: 
    - `app_run_simulation` has a frame limit of 15 FPS (`src/app/app.c:687`).
    - `PHASE_AIM_PREVIEW` has a hard-coded 5.0s wall-time timer (`src/app/app.c:548`, `src/app/app.c:572`).
    - `PHASE_PLACEMENT` has a 1.0s timer adjusted by `playback_speed` (`src/app/app.c:520`).
    - `SETTLE_TIMEOUT_SECONDS` is 30.0s (`src/physics/physics.h:27`).
    - **Search Result**: No explicit "15s" timer found in `app.c` or `physics.c`. The 15 FPS target might be confused with 15 seconds, or the timer is in a file not yet reviewed (e.g., `rules.c` or `match.c`).

## Detailed Findings

| Severity | File:Line | Evidence | Violation | Fix |
| :--- | :--- | :--- | :--- | :--- |
| Medium | `src/physics/physics.c:273` | `float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;` | Resistance is applied as a velocity scale after the physics step. | Integrate resistance as a force/damping within Box2D or use a more robust integration. |
| Medium | `src/physics/physics.c:410` | `pressed_against_cushion = (pos.y >= CUSHION_INNER - 0.005f);` | The fallback pocket detection is extremely strict (0.5mm margin). | Increase margin or rely on a larger sensor volume. |
| Low | `src/app/app.c:687` | `double target_frame_time = 1.0 / 15.0;` | Hardcoded 15 FPS frame limit. | Move to configuration. |
| Low | `src/app/app.c:548` | `ctx->aim_preview_timer = 5.0;` | Hardcoded 5s aim preview. | Move to configuration. |

## Could Not Verify
- **15s Timer**: Not found in the targets. Likely in `src/game/rules.c` or `src/game/match.c`.
- **Settle Detection Accuracy**: `SETTLE_ACCEL_EPS` is 0.60f. Need to verify if this is too high, causing premature settling.
