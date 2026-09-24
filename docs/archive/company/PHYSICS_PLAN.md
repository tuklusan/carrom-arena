# Physics Settling Retune Plan

## Problem Analysis

**Current Constants (physics.h:23-28):**
```c
#define BOARD_COULOMB     0.12f   // Coulomb friction coefficient
#define BOARD_VISCOUS     0.85f   // Viscous damping coefficient
#define SETTLE_SPEED_EPS  1e-4f   // Speed threshold for settling
#define SETTLE_ACCEL_EPS  1e-4f   // Acceleration threshold for settling
#define SETTLE_TIMEOUT_SECONDS 30.0f  // Max simulation time before forced settle
```

**Root Cause:** The settling detection is fundamentally broken.

In `physics_is_settled()` (physics.c:461-503), `max_accel` is computed as the maximum deceleration among moving pieces:
```c
float decel = BOARD_COULOMB + BOARD_VISCOUS * speed;
```

With `BOARD_COULOMB = 0.12`, the **minimum deceleration** for any moving piece is **0.12** (when speed → 0). But `SETTLE_ACCEL_EPS = 1e-4`. Since `max_accel >= 0.12 >> 1e-4` whenever any piece moves, the condition `max_accel <= SETTLE_ACCEL_EPS` is **never true** during active motion.

Settling only occurs via the 30-second timeout (`SETTLE_TIMEOUT_SECONDS`), explaining why pieces "take 30s+ to settle."

**Secondary Issue:** Even if acceleration check worked, current coefficients yield ~4.2s sim time from max speed (5.0) to 1e-4 — too slow for carrom (real pieces settle in 2-3s).

---

## Physics Model

The discrete deceleration per substep (dt = 1/120s):
```
v_{n+1} = v_n - (COULOMB + VISCOUS * v_n) * dt
        = v_n * (1 - VISCOUS*dt) - COULOMB*dt
```

Closed form:
```
v_n = (v_0 + COULOMB/VISCOUS) * (1 - VISCOUS*dt)^n - COULOMB/VISCOUS
```

**Target:** Piece at max speed (5.0) stops within **120-240 steps** (1-2 seconds sim time).

---

## Proposed New Constants

| Constant | Old Value | New Value | Rationale |
|----------|-----------|-----------|-----------|
| `BOARD_COULOMB` | 0.12 | **0.50** | 4× stronger dry friction; ensures hard stop at low speeds (0.50/120 = 0.0042 per step → stops from 0.01 in 2-3 steps) |
| `BOARD_VISCOUS` | 0.85 | **2.00** | 2.35× stronger speed-dependent drag; time constant 1/V = 0.5s → 3τ = 1.5s exponential decay |
| `SETTLE_SPEED_EPS` | 1e-4 | **1e-3** | Practical threshold; 0.001 m/s is visually stationary; avoids precision issues |
| `SETTLE_ACCEL_EPS` | 1e-4 | **0.60** | **Must exceed COULOMB (0.50)** so `max_accel <= 0.60` becomes true when all pieces stop (max_accel=0) |
| `SETTLE_TIMEOUT_SECONDS` | 30.0 | **8.0** | Generous fallback; 8s sim time >> expected 1.5-2s settle time |

---

## Settling Time Verification (Mental Model)

**With new constants (C=0.50, V=2.00, dt=1/120):**
- `1 - V*dt = 1 - 2/120 = 0.98333`
- `C/V = 0.25`
- `C*dt = 0.50/120 = 0.00417`

From **v₀ = 5.0**:
```
v_n = 5.25 * 0.98333^n - 0.25
```
- n=120 (1.0s): v = 5.25×0.133 - 0.25 = **0.45** (still moving)
- n=180 (1.5s): v = 5.25×0.049 - 0.25 = **0.008** ≈ threshold
- n=200 (1.67s): v = 5.25×0.034 - 0.25 = **-0.07** → **clamped to 0**

**Settles in ~180 steps (1.5s sim time)** ✓

From **v₀ = 0.1** (post-collision residual):
- decel = 0.50 + 2.0×0.1 = 0.70
- decel×dt = 0.0058/step
- Steps to 0.001: ~17 steps (0.14s) ✓

**Low-speed behavior:** Coulomb term (0.0042/step) dominates → hard stop in few steps.

---

## Settling Detection Fix

With `SETTLE_ACCEL_EPS = 0.60 > BOARD_COULOMB = 0.50`:
- While pieces move: `max_accel ≥ 0.50` → condition false
- When all pieces stop (speed ≤ 1e-3): `max_speed ≤ 1e-3` AND `max_accel = 0 ≤ 0.60` → **condition true**
- 3 consecutive steps (`SETTLE_CONFIRM_STEPS`) → settled ✓

No more 30s timeout fallback needed in normal play.

---

## Files to Modify

1. **src/physics/physics.h** (lines 23-28) — Update all five constants
2. **No changes to physics.c** — `physics_apply_board_resistance` logic is correct; `physics_is_settled` logic works with new thresholds

---

## Verification Steps

1. Build project: `cmake --build build --target carrom_arena`
2. Run integration tests: `ctest --test-dir build/tests`
3. Manual test: Play a shot, verify pieces visually settle in <2s wall time at 1x playback
4. Check logs: `sim_time` at settle should be 1.5-2.5s, not 30s