# App State & Platform Review Note

## Coverage Table

| File | Total Lines | Reviewed | Ranges | Status |
| :--- | :--- | :--- | :--- | :--- |
| `src/app/app.h` | 88 | 88 | 1-88 | Done |
| `src/app/app.c` | 947 | 947 | 1-947 | Done |
| `src/app/main.c` | 22 | 22 | 1-22 | Done |
| `src/app/capture.c` | 18 | 18 | 1-18 | Done |
| `src/app/diagnostic.c` | 18 | 18 | 1-18 | Done |
| `src/app/soak.c` | 19 | 19 | 1-19 | Done |
| `src/platform/platform.h` | 54 | 54 | 1-54 | Done |
| `src/platform/platform.c` | 158 | 158 | 1-158 | Done |

## Findings

### DEF-001: Thinking Phase Visualization Budget Gap
- **Severity**: Medium
- **File**: `src/app/app.c:292-295`
- **Defect Class**: Visualization/UX
- **Evidence**:
  ```c
  double budget_sec = (ctx->config.ai_budget_ms > 0) ? (ctx->config.ai_budget_ms / 1000.0) : 0.15;
  ctx->thinking_min_wall = budget_sec * 0.2;
  if (ctx->thinking_min_wall < 2.0) ctx->thinking_min_wall = 2.0;  // At least 2 seconds for verification
  ctx->thinking_min_wall += platform_time_now();
  ```
- **Description**: The code enforces a hard minimum of 2.0 seconds for the THINKING phase visualization on subsequent turns, regardless of the `ai_budget_ms` config. While this ensures verification visibility, it creates a disconnect between configured AI speed and actual wall-clock behavior in `RENDERED` mode, potentially misleading users about AI performance.
- **Recommended Fix**: Make the 2.0s minimum optional via a config flag (e.g., `--force-viz-delay`) or scale it more logically with the `ai_budget_ms`.

### DEF-002: Inconsistent First-Turn Visualization Timer
- **Severity**: Low
- **File**: `src/app/app.c:380-383` vs `src/app/app.c:292-295`
- **Defect Class**: Logic Inconsistency
- **Evidence**:
  - First turn: `if (ctx->thinking_min_wall < 0.5) ctx->thinking_min_wall = 0.5;` (Line 382)
  - Subsequent turns: `if (ctx->thinking_min_wall < 2.0) ctx->thinking_min_wall = 2.0;` (Line 294)
- **Description**: The first turn has a 0.5s minimum delay, while all subsequent turns have a 2.0s minimum delay. This results in a "jittery" feeling where the first shot of the match is significantly faster than all others.
- **Recommended Fix**: Unify the minimum wall-clock delay for the THINKING phase across all turns.

### DEF-003: Potential Division by Zero in Candidates Estimation
- **Severity**: Low
- **File**: `src/app/app.c:496-497`
- **Defect Class**: Robustness
- **Evidence**:
  ```c
  ctx->candidates_evaluated = (int)(ctx->thinking_timer * 1000.0 / 
      ((ctx->config.ai_budget_ms > 0) ? ctx->config.ai_budget_ms : 150) * ctx->max_candidates);
  ```
- **Description**: While there is a ternary check `ctx->config.ai_budget_ms > 0`, if `ai_budget_ms` were somehow set to 0 in a way that bypasses CLI checks (though `app_parse_args` does have a lower bound of 10), it relies on the fallback `150`. However, the logic in `app_config_default` (line 59) and `app_parse_args` (line 907) suggests a minimum of 10ms. If the budget is extremely low (e.g., 10ms), the multiplication by `max_candidates` (up to 12) might cause the HUD to jump very quickly.
- **Recommended Fix**: Ensure `ai_budget_ms` is consistently validated at the `AppContext` creation level.

### DEF-004: Missing `TICK` / Phase Transition Hole in `PHASE_IDLE`
- **Severity**: Low
- **File**: `src/app/app.c:444-445`
- **Defect Class**: State Machine
- **Evidence**:
  ```c
  case PHASE_IDLE:
      break;
  ```
- **Description**: The `PHASE_IDLE` state is a dead end. There is no transition defined to move from `IDLE` to `THINKING` unless the match is restarted. While not currently triggered by the main loop, it's a "hole" in the state machine.
- **Recommended Fix**: Document the intended use of `PHASE_IDLE` or add a transition trigger.

### DEF-005: Unhandled `TURN_BOARD_OVER` in `app_resolve_shot`
- **Severity**: Medium
- **File**: `src/app/app.c:297-299`
- **Defect Class**: Phase Transition
- **Evidence**:
  ```c
  case TURN_BOARD_OVER:
      // match_start_board will set PHASE_THINKING for new board
      break;
  ```
- **Description**: When `TURN_BOARD_OVER` occurs, the switch block in `app_resolve_shot` does nothing. It relies on the logic at lines 340-345:
  ```c
  if (outcome.turn_decision == TURN_BOARD_OVER) {
      if (!match_is_over(&ctx->match)) {
          match_start_board(&ctx->match, &ctx->game, &ctx->rng);
          physics_sync_from_board(ctx->physics, &ctx->game.board, ctx->game.turn_seat);
      }
  }
  ```
  However, `match_start_board` (implied) doesn't explicitly set the `ctx->game.phase = PHASE_THINKING` inside `app_resolve_shot`. The simulation loop will continue with the *old* phase (which was `PHASE_RESOLVING`) until the next frame, but the logic in `app_run_simulation` for `PHASE_RESOLVING` (line 610-612) is empty. This creates a 1-frame "gap" where the game state is in limbo before the next board's `PHASE_THINKING` is explicitly set.
- **Recommended Fix**: Explicitly set `ctx->game.phase = PHASE_THINKING` and initialize thinking timers immediately after `match_start_board` in the `TURN_BOARD_OVER` handler.
