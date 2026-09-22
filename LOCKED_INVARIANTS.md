# LOCKED INVARIANTS — do not modify without a new, specific operator bug report against the item

This file is read by every directive. Items here are confirmed acceptable by the operator on real hardware (not just Xvfb/CI). If your work would touch code that affects one of these, and the ask doesn't explicitly target it, find another way, or stop and flag the conflict in `.kimi_progress.log` instead of proceeding.

1. **Board visual style** — brown wood texture, pocket rendering, piece colors/shapes. (Locked since R8.)
2. **Striker placement animation** — the pulsing-halo hold + "Striker placed at (x,y) — striking in Ns" countdown banner. (Locked since R8.)
3. **Aim-preview line** — origin at striker position, direction, boundary clamping, arrowhead, 5-second wall-clock hold, then clears. (Locked since R8.)
4. **Window/canvas sizing and board placement within the window** — confirmed acceptable by the operator on live Windows 11 hardware as of `beta-0.0.5` (commit `3ff07ff`). This includes: default window size when no `--width`/`--height` given, the board's size relative to the window, and its centering. (Locked since R9/beta-0.0.5, 2026-09-15.)
5. **THINKING-phase full-baseline oscillation range** — the striker/figure sliding the full edge-to-edge span of the seat's baseline. (Locked since R9/beta-0.0.5.)
6. **Figure settle-alignment outside the board** — the human figure tracking the striker and settling aligned with it while staying outside the board boundary. (Locked since R9/beta-0.0.5.)
7. **Physical Dimensions & Geometry** — confirmed acceptable by the operator as of `beta-0.0.6` (2026-09-21). UNCONFIRMED.
   - `BOARD_SIDE_NORM`: 1.0
   - `CUSHION_THICKNESS`: 0.025
   - `POCKET_RADIUS_NORM`: 0.030
   - `PIECE_RADIUS_NORM`: 0.021
   - `STRIKER_RADIUS_NORM`: 0.028
   - Pocket centers (`POCKET_CENTERS`)
   - Baselines (`BASELINE_Y_NORTH`, `BASELINE_Y_SOUTH`, `BASELINE_X_EAST`, `BASELINE_X_WEST`)
   - ICF formation geometry


## Adding to this list

When a directive confirms a new area as "good enough, don't touch," append it here with the date and the commit/tag where it was confirmed, then commit this file alongside the rest of that directive's changes.
