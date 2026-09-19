# Physics Review Report

## Coverage Table

| File | Total Lines | Lines Reviewed | Chunk Ranges | Status |
| :--- | :---: | :---: | :--- | :--- |
| `src/physics/physics_snapshot.h` | 63 | 63 | 1-63 | Complete |
| `src/physics/physics_snapshot.c` | 137 | 137 | 1-137 | Complete |
| `src/physics/physics.h` | 92 | 92 | 1-92 | Complete |
| `src/physics/physics.c` | 766 | 766 | 1-250, 251-500, 501-766 | Complete |

## Findings

### PH-01: Potential Determinism/Consistency Issue in `physics_snapshot_restore`
- **Severity**: Medium
- **File**: `src/physics/physics_snapshot.c:94` & `109`
- **Defect Class**: Physics Violation / Determinism
- **Evidence**:
  ```c
  b2Body_SetTransform(pw->piece_bodies[i], 
      (b2Vec2){ snap->pieces[i].position.x, snap->pieces[i].position.y },
      (b2Rot){ cosf(snap->pieces[i].angle * 0.5f), sinf(snap->pieces[i].angle * 0.5f) });
  ```
- **Observation**: The rotation is being reconstructed using `cosf(angle * 0.5f)` and `sinf(angle * 0.5f)`. In Box2D, `b2Rot` typically represents a rotation matrix/complex number where $c = \cos(\theta)$ and $s = \sin(\theta)$. Multiplying the angle by $0.5$ before taking the sine/cosine is mathematically incorrect for restoring a standard rotation angle $\theta$.
- **Recommended Fix**: Use `cosf(snap->pieces[i].angle)` and `sinf(snap->pieces[i].angle)`.

### PH-02: Inconsistent Pocket Detection Logic
- **Severity**: Low
- **File**: `src/physics/physics.c:368-490`
- **Defect Class**: Logic Flaw
- **Evidence**: 
  The code implements two parallel pocket detection systems:
  1. `physics_check_pocket_events` (using Box2D v3 sensor events).
  2. `physics_check_pockets` (using manual distance and "pressed against cushion" checks).
- **Observation**: The "fallback" distance-based check uses a very specific "pressed against cushion" logic (lines 406, 414, 422, 430) with a $0.005\text{f}$ tolerance. If the sensor event fails, this fallback might be too restrictive or slightly different in behavior than the sensor, leading to non-deterministic pocketing if a piece "skips" the sensor but barely misses the fallback tolerance.
- **Recommended Fix**: Unify the pocketing trigger. If sensors are used, rely on them; if a fallback is needed, ensure the fallback geometry exactly matches the sensor's trigger volume.

### PH-03:- Accuracy of `physics_is_settled` Acceleration Check
- **Severity**: Low
- **File**: `src/physics/physics.c:581-584`
- **Defect Class**: Physics Violation
- **Evidence**:
  ```c
  if (speed > SETTLE_SPEED_EPS) {
      float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
      if (decel > max_accel) max_accel = decel;
  }
  ```
- **Observation**: The "acceleration" check is not actually measuring the acceleration of the body, but rather calculating the *theoretical deceleration* that *would* be applied by the board resistance. It doesn't account for actual collisions or external forces. While it works for a settling piece, it's a proxy, not a measurement.
- **Recommended Fix**: To be more robust, measure the actual change in velocity between steps ($\frac{\Delta v}{\Delta t}$).

## Summary
The physics implementation is generally clean and follows a strict fixed-timestep pattern. The most critical issue is the rotation restoration in snapshots (PH-01), which likely causes pieces to be rotated incorrectly when restoring an AI scratch simulation state.
