# Carrom Arena — Autonomous SDLC Run: PAUSE / RESUME Playbook

**Paused:** 2026-09-03 (UTC)
**Reason:** Operator requested a clean pause for later resume.
**Goal in flight:** "fully working and acceptable carrom arena game proven to work on windows and linux (and later 18 runners at github)."

---

## State at pause

- **Linux host** `sanyalnet@10.0.0.161`, project root `~/SOFTWARE-DEVELOPMENT/carrom`.
  - Kimi CEO ran for ~5h under `--agent ceo -p "<.kimi_directive.md>"`.
  - Killed cleanly by the operator on pause. **Uncommitted** changes remain in the working tree.
  - Modified (staged in working tree, not committed):
    - `src/physics/physics.h` — settling constants retuned (Coulomb 0.12→0.50, Viscous 0.85→2.00, SETTLE_ACCEL_EPS 1e-4→0.60, SETTLE_TIMEOUT 30s→8s, SPEED_EPS 1e-4→1e-3).
    - `src/render/renderer.c` — window sizing / monitor clamp rewrite (**buggy under Xvfb**, see below).
    - `src/ai/controller.c` — minor edits (early in the run).
    - `src/CMakeLists.txt`, `tests/CMakeLists.txt` — wiring for the new regression test.
  - New untracked artefacts:
    - Planning: `CTO_DIAGNOSIS.md`, `PHYSICS_PLAN.md`, `RENDER_PLAN.md`, `TEST_PLAN.md`.
    - Test: `tests/test_regression.c`, `scripts/soak_verification.sh`.
    - Build tree: `build_rel/` (multi-config CMake; ctest broken in this tree, use `build/` instead).
    - Operator artefacts: `.kimi_directive.md`, `.kimi_run/` (PID + last session.log).
    - Baseline frames: `captures/before/frame_*.png`.
  - `build/` binary builds; ctest 5/6 pass (`regression_test` hung at 180 s wall — may be an infinite settle loop with the new constants).
- **Windows 10** `sanyalnet@10.0.0.106`, project root `D:\SW_DEV\CARROM`: directory exists, empty. Dev toolchain probed: `cmake`, `ninja`, `git` on `/usr/bin/` via MSYS bash over SSH. `cl.exe` not on that PATH — needs VS Dev environment probe.
- **Windows 11** `vagab@10.0.0.133`, target project root `C:\Users\vagab\SOFTWARE-DEVELOPMENT\carrom`: not yet created. Same toolchain shape as W10.
- **Passwordless SSH** from Linux → both Windows boxes is installed and verified (`C:\ProgramData\ssh\administrators_authorized_keys`, ACL locked to SYSTEM + Administrators).

## Known open defects (found during pause window)

1. **`renderer_create` height assert under Xvfb.** New window-clamp math uses `GetMonitorHeight(monitor)`; Xvfb's "Failed to find selected monitor" makes this return 0, then `req_h = 0*4/5 = 0`, `game_surface = req_h - title_h - copyright_h < 0`, `InitWindow(w, negative_h, …)` → `assert(height >= 0)` → SIGABRT. Fix: floor `max_w`/`max_h` at a sane default (e.g. 1280×720) when GLFW reports 0.
2. **`regression_test` hangs.** After 180 s wall, no result. Suspect an unbounded settle loop or a match that never terminates with the new physics constants. Add stricter wall-time asserts inside the test itself, and print the sim state periodically so a hang is diagnosable.
3. **Plan not fully applied.** The CTO's 7-file blueprint has only 3 files done. Missing:
   - `src/physics/physics.c` — snapshot prev positions in `physics_step`, add `physics_get_prev_positions` / `physics_get_prev_striker_position` / `physics_get_accumulator` accessors.
   - `src/common/math.h` — add `vec2_lerp`.
   - `src/render/board_view.c` — interpolate positions using `alpha = accumulator / PHYSICS_DT`.
   - `src/render/hud.c` — viewport-relative layout (scale font sizes to game surface).
   - `src/app/app.c` — pass alpha to renderer; tighten `PHASE_SETTLING` transition.
4. **Cross-platform strand not started.** All prep is done, but no code has been synced to W10/W11 and neither has been built.

## Resume procedure (for the next session)

1. **Read this file first**, then read `SANYALnet-Labs-Dev-Blog.md` on the operator's local Windows box (`H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\SANYALnet-Labs-Dev-Blog.md`) for narrative context.
2. **Check state hasn't drifted:**
   ```bash
   ssh sanyalnet@10.0.0.161 'cd ~/SOFTWARE-DEVELOPMENT/carrom && git status && git log --oneline -5'
   ```
3. **Resume kimi** in one of two ways:
   - Fresh single-shot with an amended directive: `nohup ~/.kimi-code/bin/kimi --agent ceo -p "$(cat .kimi_directive.md) <PLUS a section listing the two defects above and telling the CEO to finish the CTO's plan and then commit>" </dev/null > .kimi_run/session.log 2>&1 &`
   - Or continue the paused session (Kimi supports `-S <sessionId>` and `-c` for the working directory). Last session id at pause: **session_307eef93-1ad8-42c2-9d87-b61ced2d2058**. Try:
     ```bash
     ~/.kimi-code/bin/kimi --agent ceo -c -p "resume: complete the 4 files still missing from CTO_DIAGNOSIS.md, then commit; also fix the two defects listed in RESUME.md #1 and #2."
     ```
4. **Re-arm the watchdog** on the operator side. Same script that lives at `/tmp/poll.sh` on the linux box.
5. **Cross-platform**: when the Linux game is visually presentable (rendered mode plays a match to completion with smooth motion + legible HUD + a winner announced), fan out to W10/W11:
   - `rsync -e ssh -a --exclude 'build*/' --exclude 'traces/' --exclude 'captures/' --exclude '.kimi_run/' ~/SOFTWARE-DEVELOPMENT/carrom/ sanyalnet@10.0.0.106:D:/SW_DEV/CARROM/`
   - Same for `vagab@10.0.0.133:C:/Users/vagab/SOFTWARE-DEVELOPMENT/carrom/`
   - Configure + build + ctest on each. Kimi CEO should own this; feed it via a fresh `-p` prompt with the goal + host list.
6. **CI matrix (later)**: 18 GitHub runners. Not started. When the local cross-platform works, the CTO agent should generate the workflow.

## Files this pause added

- `RESUME.md` (this file) — under the project subtree, as required.

