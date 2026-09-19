# Product Spec: Carrom Arena Bug Fixes (R13)

## Overview
This document specifies the acceptance criteria for fixing three identified bugs in the Carrom Arena game:
1. Startup countdown visibility
2. Striker direction inconsistency on first shot  
3. Pocket capture bounce-back behavior

These fixes will improve game usability and correctness without changing core gameplay mechanics.

## Bug 1: Startup Countdown Visibility

### Problem
The placement countdown timer (showing "Striker placed at (X, Y) - striking in T.s") is only visible when the north seat (SEAT_NORTH) is breaking. For breaks from east, south, or west seats, no countdown is displayed, making it unclear when the strike will occur.

### Root Cause
In `src/render/renderer.c`, the `draw_placement_banner` function (lines 224-246) contains an early return that only shows the banner for SEAT_NORTH:
```c
if (game->turn_seat != SEAT_NORTH) return;  // Only show for N seat (human-readable)
```

### Acceptance Criteria
The countdown timer must be visible for all seats when they are breaking, formatted appropriately for human readability.

#### Test Conditions
- Game in PHASE_PLACEMENT phase
- Striker placed on baseline and not pocketed
- Any seat (SEAT_NORTH, SEAT_EAST, SEAT_SOUTH, SEAT_WEST) is the active breaking seat

#### Expected Behavior
1. When SEAT_NORTH is breaking:
   - Display: "Striker placed at (X, Y) - striking in T.s"
   - X, Y: striker position coordinates
   - T: countdown timer value (1 decimal place, seconds)

2. When SEAT_EAST is breaking:
   - Display: "Striker placed at (X, Y) - striking in T.s" 
   - X, Y: striker position coordinates
   - T: countdown timer value (1 decimal place, seconds)

3. When SEAT_SOUTH is breaking:
   - Display: "Striker placed at (X, Y) - striking in T.s"
   - X, Y: striker position coordinates
   - T: countdown timer value (1 decimal place, seconds)

4. When SEAT_WEST is breaking:
   - Display: "Striker placed at (X, Y) - striking in T.s"
   - X, Y: striker position coordinates
   - T: countdown timer value (1 decimal place, seconds)

#### Implementation Notes
- Remove the seat-restriction condition in `draw_placement_banner`
- Maintain existing coordinate formatting (2 decimal places) and timer formatting (1 decimal place)
- Ensure banner positioning and styling remain consistent across all seats

## Bug 2: Striker Direction Change on First Shot

### Problem
The first shot (break shot) of each board does not consistently travel straight down the baseline toward the center of the pack. Instead, the direction varies based on where the striker is positioned along the baseline, causing unpredictable break shot behavior.

### Root Cause
In `src/ai/shot_candidates.c`, the TACTIC_BREAK logic (lines 236-249) aims at the geometric center of the board (0,0):
```c
Vec2 to_center = vec2_sub((Vec2){0, 0}, placement);
float aim = atan2f(to_center.y, to_center.x);
```
This only produces a straight shot along the baseline when the striker is positioned exactly on the center line (x=0 for north/south seats, y=0 for east/west seats). For off-center placements, the aim angle varies, sending the striker in non-straight trajectories.

### Acceptance Criteria
The break shot must always travel straight toward the center of the board along the baseline, regardless of striker placement along the baseline.

#### Test Conditions
- First shot of a board (consecutive_turns == 0)
- Striker placed at any legal position on the baseline for the breaking seat
- Break shot tactic selected by AI

#### Expected Behavior
1. When SEAT_NORTH is breaking:
   - Striker must travel straight south (aim angle = -π/2 radians)
   - Regardless of x-position on north baseline (y = BASELINE_Y_NORTH)

2. When SEAT_SOUTH is breaking:
   - Striker must travel straight north (aim angle = π/2 radians)
   - Regardless of x-position on south baseline (y = BASELINE_Y_SOUTH)

3. When SEAT_EAST is breaking:
   - Striker must travel straight west (aim angle = π radians)
   - Regardless of y-position on east baseline (x = BASELINE_X_EAST)

4. When SEAT_WEST is breaking:
   - Striker must travel straight east (aim angle = 0 radians)
   - Regardless of y-position on west baseline (x = BASELINE_X_WEST)

#### Implementation Notes
- Modify the TACTIC_BREAK logic in `shot_candidates_tactical` to compute aim angle based on seat:
  - SEAT_NORTH: aim = -π/2 (straight south)
  - SEAT_SOUTH: aim = π/2 (straight north)  
  - SEAT_EAST: aim = π (straight west)
  - SEAT_WEST: aim = 0 (straight east)
- Preserve existing break shot power (0.8) and tactic (TACTIC_BREAK)
- Ensure the fix works for all legal baseline positions

## Bug 3: Pocket Capture Bounce-Back

### Problem
Pieces that are pocketed sometimes bounce back out of the pocket instead of remaining pocketed, particularly when they enter the pocket at high speed or at shallow angles.

### Root Cause
In `src/physics/physics.c`, the `physics_check_pockets` function (lines 367-490) contains flawed logic for determining if a piece is "pressed against the cushion." The boundary checks only validate one side of the required range, allowing pieces to be considered pocketed when they are actually beyond the cushion (off the board).

Specifically:
- North/south pockets: only checks `pos.y >= CUSHION_INNER - 0.005f` (missing upper bound)
- East/west pockets: only checks `pos.x >= CUSHION_INNER - 0.005f` (missing upper bound for east, missing lower bound for west)

### Acceptance Criteria
Pieces must only be considered pocketed when they are within the pocket radius AND properly positioned relative to the cushion (on the playing surface side).

#### Test Conditions
- Piece moving toward any pocket with sufficient velocity to enter pocket area
- Piece position near pocket boundaries

#### Expected Behavior
1. **North Pockets** (SEAT_NORTH/SEAT_SEAT combinations with pocket_pos.y > 0):
   - Piece considered pocketed ONLY when:
     - Within pocket radius of pocket center
     - AND `CUSHION_INNER - 0.005f <= pos.y <= CUSHION_INNER`
     - (e.g., 0.470 <= pos.y <= 0.475 for POCKET_RADIUS_NORM=0.03, CUSHION_THICKNESS=0.025)

2. **South Pockets** (pocket_pos.y < 0):
   - Piece considered pocketed ONLY when:
     - Within pocket radius of pocket center
     - AND `-CUSHION_INNER <= pos.y <= -CUSHION_INNER + 0.005f`
     - (e.g., -0.475 <= pos.y <= -0.470)

3. **East Pockets** (pocket_pos.x > 0):
   - Piece considered pocketed ONLY when:
     - Within pocket radius of pocket center
     - AND `CUSHION_INNER - 0.005f <= pos.x <= CUSHION_INNER`
     - (e.g., 0.470 <= pos.x <= 0.475)

4. **West Pockets** (pocket_pos.x < 0):
   - Piece considered pocketed ONLY when:
     - Within pocket radius of pocket center
     - AND `-CUSHION_INNER <= pos.x <= -CUSHION_INNER + 0.005f`
     - (e.g., -0.475 <= pos.x <= -0.470)

#### Implementation Notes
- Fix boundary condition checks in `physics_check_pockets` function
- For each pocket direction, implement both lower and upper bound checks
- Maintain existing pocket radius distance check as secondary validation
- Preserve all other pocket detection logic (sensor events, body destruction, tracking)
- Ensure fix works for all four pockets and both pieces/striker

## Related Files to Modify
1. `src/render/renderer.c` - Fix startup countdown visibility
2. `src/ai/shot_candidates.c` - Fix break shot aim direction  
3. `src/physics/physics.c` - Fix pocket capture boundary checks

## Testing Recommendations
- Verify countdown visibility for all four seats during placement phase
- Test break shot consistency with striker at baseline extremes and center
- Validate pocket behavior with high-speed and shallow-angle shots into each pocket
- Run existing test suite to ensure no regressions
- Perform manual gameplay testing to confirm fixes resolve reported issues