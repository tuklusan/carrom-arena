# Technical Strategy: Carrom Arena Bug Fixes (R13)

## Approach Rationale
The selected approach implements minimal, targeted fixes for each identified bug as outlined in the product spec. This strategy prioritizes:
- **Correctness**: Each fix directly addresses the root cause identified in the spec.
- **Minimal Impact**: Changes are localized to specific functions without altering core gameplay mechanics.
- **Consistency**: Fixes apply uniform behavior across all seats/pockets as required by acceptance criteria.
- **Verifiability**: Each change can be independently tested against the specified test conditions.

## Files to Modify

1. **src/render/renderer.c**
   - Function: `draw_placement_banner` (lines 224-246)
   - Change: Remove the seat-restriction early return (`if (game->turn_seat != SEAT_NORTH) return;`) to show countdown for all seats.
   - Preserve existing coordinate (2 decimal places) and timer (1 decimal place) formatting.

2. **src/ai/shot_candidates.c**
   - Function: `shot_candidates_tactical` (TACTIC_BREAK logic, lines 236-249)
   - Change: Replace geometry-based aim calculation with seat-specific fixed angles:
     - SEAT_NORTH: aim = -π/2 (straight south)
     - SEAT_SOUTH: aim = π/2 (straight north)
     - SEAT_EAST: aim = π (straight west)
     - SEAT_WEST: aim = 0 (straight east)
   - Preserve existing break shot power (0.8) and tactic (TACTIC_BREAK).

3. **src/physics/physics.c**
   - Function: `physics_check_pockets` (lines 367-490)
   - Change: Implement proper boundary checks for pocket capture:
     - North/South pockets: Add upper bound check `pos.y <= CUSHION_INNER`
     - East/West pockets: Add missing bounds (west: lower bound, east: upper bound)
     - Maintain existing pocket radius distance check as secondary validation.
   - Preserve all other pocket detection logic (sensor events, body destruction, tracking).

## Cross-Cutting Concerns

### 1. Seat/Pocket Symmetry
- All fixes must ensure consistent behavior across all four seats (North, East, South, West) and four pockets.
- Coordinate system assumptions must be verified (e.g., baseline positions, pocket coordinates).

### 2. Numerical Precision
- Maintain existing formatting precision (2 decimal places for position, 1 decimal for timer).
- Boundary checks use defined constants (CUSHION_INNER, POCKET_RADIUS_NORM) to avoid magic numbers.

### 3. Testing and Regression
- Existing test suite must pass to ensure no regressions.
- Manual verification required for:
  - Countdown visibility during placement phase for all seats
  - Break shot straightness from various baseline positions
  - Pocket capture reliability for high-speed/shallow-angle shots
- Edge cases: striker exactly at baseline center, pieces entering pockets at extreme angles.

### 4. Build and Dependencies
- Changes are confined to C source files; no header modifications required.
- No external dependencies affected; standard math library usage (atan2f, π) already present.

### 5. Performance Impact
- All changes are O(1) operations with negligible performance impact.
- No alterations to physics simulation frequency or rendering pipeline.

## Implementation Notes
- Follow existing code style in each file (indentation, naming conventions).
- Ensure constants like M_PI are properly included via existing headers.
- No changes required to game state structures or enum definitions.
- Each fix can be implemented and tested independently.

## Verification Plan
1. Unit-level verification:
   - Renderer: Visual inspection of countdown for all seats
   - AI: Log aim angles for break shots from various positions
   - Physics: Boundary condition tests for each pocket
2. Integration testing:
   - Full gameplay scenarios involving placement, breaking, and pocketing
   - Regression testing via existing test suite
3. Validation against acceptance criteria test conditions specified in product spec.