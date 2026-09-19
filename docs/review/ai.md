# AI Module Review Note

## Coverage Table

| File | Total Lines | Lines Reviewed | Chunk Ranges | Status |
| :--- | :--- | :--- | :--- | :--- |
| `src/ai/baseline_controller.c` | 11 | 11 | 1-11 | Complete |
| `src/ai/shot_evaluator.c` | 219 | 219 | 1-219 | Complete |
| `src/ai/controller.c` | 252 | 252 | 1-252 | Complete |
| `src/ai/shot_candidates.c` | 334 | 334 | 1-334 | Complete |
| `src/ai/shot_evaluator.h` | 31 | 31 | 1-31 | Complete |
| `src/ai/controller.h` | 50 | 50 | 1-50 | Complete |
| `src/ai/arena_controller.c` | 12 | 12 | 1-12 | Complete |
| `src/ai/shot_candidates.h` | 40 | 40 | 1-40 | Complete |

## Findings

### AI-01: Determinism Break in `arena_decide`
- **Severity**: High
- **Location**: `src/ai/controller.c:170-226`
- **Defect Class**: Determinism Break
- **Evidence**:
  ```c
  170:    RNGSnapshot rng_snap = rng_snapshot(rng);
  ...
  219:    float aim_noise = self->profile.aim_noise_std * (pcg32_random_float(rng) * 2.0f - 1.0f);
  220:    float power_noise = self->profile.power_noise_std * (pcg32_random_float(rng) * 2.0f - 1.0f);
  ...
  226:    rng_restore(rng, rng_snap);
  ```
- **Description**: The AI saves the RNG state at the start of `arena_decide` and restores it at the end. However, it uses the `rng` to apply "imperfection" noise (lines 219-220) *before* restoring the state. This means the RNG draws used for the final shot noise are effectively erased from the RNG stream's history, but the resulting `best_plan` is returned. While this makes the *internal* search deterministic, the *outcome* depends on the RNG state. Crucially, if `controller_fallback_shot` is called (line 230), it uses the restored RNG. This creates a discrepancy where the "imperfection" draws are lost. More importantly, `best_plan.rng_draw` is assigned at line 223, but the RNG state that produced it is discarded.
- **Recommended Fix**: Move `rng_restore` to before the noise application if noise should be part of the permanent RNG stream, or ensure the restoration doesn't invalidate the logic used by the game engine to verify the shot's randomness.

### AI-02: Potential Out-of-Bounds in `shot_candidates_tactical`
- **Severity**: Medium
- **Location**: `src/ai/shot_candidates.c:38-44`
- **Defect Class**: Algorithmic Error
- **Evidence**:
  ```c
  38:    Vec2 targets[19];
  39:    int target_count = 0;
  40:    for (int i = 0; i < MAX_PIECES; i++) {
  41:        if (board->pieces[i].on_board && board->pieces[i].color == (team == TEAM_WHITE ? PIECE_WHITE : PIECE_BLACK)) {
  42:            targets[target_count++] = board->pieces[i].position;
  43:        }
  44:    }
  ```
- **Description**: `targets` is fixed at 19. If `MAX_PIECES` is larger than 19 (which it likely is, as the queen is separate and there are usually 9 pieces per side + queen), and all pieces of one color are on board, `target_count` could exceed 19, causing a buffer overflow.
- **Recommended Fix**: Use `MAX_PIECES` or a defined `MAX_TEAM_PIECES` for the `targets` array size.

### AI-03: Incorrect Aim Calculation in `shot_candidates_tactical`
- **Severity**: Medium
- **Location**: `src/ai/shot_candidates.c:64`
- **Defect Class**: Algorithmic Error
- **Evidence**:
  ```c
  64:                float aim = atan2f(to_target.y, to_target.x);
  ```
- **Description**: `to_target` is the vector from `placement` to `target`. The AI aims directly at the target piece. While this is "direct", for a piece to go into a pocket, the striker must hit the target at a specific contact point (offset from the center of the target). Aiming at the center of the target will only work if the target is perfectly aligned with the pocket.
- **Recommended Fix**: Calculate the aim angle based on the required contact point to drive the piece toward the chosen pocket.

### AI-04: Missing Boundary Check in `shot_candidates_variants`
- **Severity**: Low
- **Location**: `src/ai/shot_candidates.c:297-298`
- **Defect Class**: Algorithmic Error
- **Evidence**:
  ```c
  297:            out_variants[count].plan.power = powers[p];
  298:            out_variants[count].plan.aim_angle = math_wrap_angle(plan->aim_angle + aim_offsets[a]);
  ```
- **Description**: The `out_variants` array is filled based on `max_variants`. While the loops check `count < max_variants`, the `out_variants` array is passed from `shot_candidates_generate` where it's a fixed size of 40. If `max_variants` exceeds 40, this will overflow.
- **Recommended Fix**: Ensure `max_variants` is clamped to the actual size of the `variants` buffer in `shot_candidates_generate`.
