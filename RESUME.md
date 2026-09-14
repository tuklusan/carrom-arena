# Carrom Arena — Autonomous SDLC Run: PAUSE / RESUME Playbook

**Updated:** 2026-09-14 (UTC), for a planned operator reboot.
**Reason:** The operator agent's host is rebooting. The 6-hour supervision cron and this conversation's session-scoped state will NOT survive the reboot. Kimi itself is idle (exited cleanly) — nothing is currently running on the Linux box that needs to be stopped.

## Where things stand

- **Repo head:** `f256c4f` on `main`, pushed, CI green ([34576176222](https://github.com/tuklusan/carrom-arena/actions/runs/34576176222), 1m27s).
- **Tags:** `beta-0.0.1` (52009a2), `beta-0.0.2` (216e8cd), `beta-0.0.3` (041df95, explicit revert point before the R8 corrective pass).
- **Kimi:** last PID 859058, exited cleanly after landing `f256c4f`. No process running. Session exists on disk for `--continue` to pick up (`~/.kimi-code/sessions/wd_carrom_7548a6022dce/`).
- **Working tree on Linux box:** clean except `.kimi_directive.md` (routine, holds the last dispatched directive text) and three harmless untracked files kimi created for its own testing: `Testing/` (ctest artifacts), `analyze_behavior.py`, `analyze_board.py`.

## What's actually working (verified, not just claimed)

- Linux, Windows 10, Windows 11 all build and pass ctest 7/7.
- GitHub Actions CI green on the (pruned) matrix — Linux gcc/clang Debug+Release, Windows-2022 mingw Debug+Release. MSVC, macOS, Windows-2019, mingw-Release cells are still disabled (deferred R6 work, never resumed after the UX pivot).
- Window sizing: **R8-1 board centering fully verified** across 7 resolutions (400x300 through 3840x2160) with real numeric evidence (board_size, offset%, monotonic growth) — this is the fix for the original "board confined to a small top-left corner" regression the operator found on real hardware.
- Footer URL corrected to `https://supratim-sanyal.blogspot.com/` (R8-4, verified).
- 30 to 15 FPS, 0.05x default playback speed, AI CPU budget cap (`--ai-budget-ms`), single-instance lock, cross-machine kill gate (`scripts/kill-all-runs.sh` + pre-push hook) — all landed and stable from earlier cycles.

## What's still broken (R8-2/R8-3, in progress)

Per kimi's own `R8_VERIFICATION_REPORT.md` (on disk at repo root) after `f256c4f`:

- **Figure-striker correlation ~0.0** — the human figure is supposed to mirror the striker's position along the baseline axis during the THINKING oscillation and settle in sync with it. Kimi's diagnosis: a camera/coordinate clipping issue in the figure bands (bands above/below/beside the board where the N/S/E/W figures live) — likely the figure draw code isn't reading the same live coordinate transform as the striker.
- **Figures partially inside the board bounding box** — the "stay outside the board" invariant (R8-2) still isn't fully held. Kimi's diagnosis: the perpendicular-offset logic needs "viewport unification" — i.e. the figure-band coordinate space and the board coordinate space aren't consistently transformed together, especially after the `camera zoom = 0.7` change kimi made to fit the figure bands in frame.
- Kimi explicitly flagged needing "camera/viewport unification" as the next real fix — this sounds like the coordinate system for board vs. figure-bands vs. HUD needs a single source of truth (one `Layout`/transform struct used everywhere), rather than the board's `math_world_to_screen()` and the figure-band drawing using different assumptions.

**What did NOT recur this cycle:** no file corruption, no silent cutoff — kimi followed the rebuild-after-every-edit discipline from the last directive and completed cleanly with a pushed, green commit. Good sign for directive quality improving over iterations.

## Resume procedure

1. Re-read this file plus `docs/DEV_BLOG.md` in the repo (mirrors `SANYALnet-Labs-Dev-Blog.md` on the operator's local Windows box) for full narrative context.
2. Re-verify reachability: Linux `sanyalnet@192.168.4.76`, Windows 10 `sanyalnet@192.168.4.75`, Windows 11 `vagab@192.168.4.103`, macOS `rumtuk@192.168.4.77` (bonus, not required). Password `***REMOVED***` everywhere; SSH keys are also installed (`~/.ssh/id_ed25519` on the Linux box) for passwordless cross-machine access.
3. **Re-arm the 6-hour supervision cron** — it was a session-scoped `CronCreate` job (`17 */6 * * *` local), auto-expiring after 7 days from creation (created 2026-09-10), and does NOT survive a reboot or a new session. Recreate it with the same "SSH check state, commit-if-safe or relaunch-if-needed, update blog with phase/step, report" instructions used throughout this project (see blog entries for the exact prompt text if needed).
4. **Next directive should target R8-2/R8-3's "camera/viewport unification"** — dispatch via `bash /tmp/relaunch.sh` (never invoke kimi directly; the wrapper enforces `--continue`-first session resumption and the canonical CEO+parallel-subagent gate via `~/.kimi-code/OPERATOR_CANONICAL_PREAMBLE.md`). Suggested framing: have kimi unify the board/figure-band coordinate transform into a single function/struct used by both `board_view.c`'s striker/piece drawing AND the figure drawing, rather than two separate assumptions about camera zoom and offsets.
5. Once R8-2/R8-3 pass with real evidence (same discipline as R8-1: numeric proof across multiple sizes/frames, not just "implemented"), get the operator a fresh real-hardware build (W11 has been the reliable target: `git pull && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel`, then scp `build/carrom_arena.exe` back) for final visual sign-off.
6. After R8 fully passes: resume the deferred R6 CI-matrix work (re-enable MSVC/macOS/Windows-2019 cells one at a time), then consider the macOS port (SSH auth already installed on `rumtuk@192.168.4.77`, needs `cmake`+`ninja` — currently only `git`/`clang`/`gcc`/`make` present).

## Canonical rules that must survive resumption (from memory files, re-read them)

- **CEO-only interaction, mandatory parallel subagent spawn** — enforced by `/tmp/relaunch.sh` plus `~/.kimi-code/OPERATOR_CANONICAL_PREAMBLE.md` on the Linux box. Never invoke `kimi` directly.
- **Session continuity** — `/tmp/relaunch.sh` prefers `kimi --continue` over a fresh `--agent ceo` session whenever one already exists for the project's working directory (checked via `~/.kimi-code/session_index.jsonl`). This meaningfully improved turnaround starting 2026-09-10 (kimi no longer re-explores the codebase from scratch each relaunch).
- **kimi's model** (`nvidia/nvidia/nemotron-3-ultra-550b-a55b`) is on a provider deprecation list. Operator explicitly said: wait until it actually fails before swapping, do not swap proactively. See `kimi-model-deprecation-watch.md` memory for the replacement mapping when needed.
- **Never bypass the cross-machine kill gate** (`scripts/kill-all-runs.sh`) before any local `carrom_arena` invocation on any host — prevents duplicate/orphaned processes across the fleet.
- Scheduled check-ins must state the current phase/step (not just PID/CI status) — operator explicitly requested this on 2026-09-10.

## Files this update touched

- `RESUME.md` (this file), rewritten in place at the repo root.
