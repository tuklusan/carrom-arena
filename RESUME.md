# Carrom Arena: PAUSE / RESUME Playbook

**Updated:** 2026-09-19 (UTC). Paused by the operator to upgrade kimi. Nothing is running for carrom (kimi stopped, stall-watchdog stopped). The unrelated ZX-UX kimi process on the same Linux box must not be touched.

## Repo state
- `main` head `7d3237c` ("docs: R14a full code review report"), on top of R12 `5763f4a`. Tags: `beta-0.0.1`..`beta-0.0.5`, `BETA-0.0.1` (accidental duplicate), `TURNS-OK` (de5a2b7, turn sequencing confirmed on real hardware).
- Uncommitted partial R14b work in the Linux working tree (`~/SOFTWARE-DEVELOPMENT/carrom`), saved as `wip_r14b_partial.patch` (untracked): `board.c` (generated two-ring layout), `physics_snapshot.c`, `rules.c`, `shot_candidates.c`, `telemetry/replay.c`, `telemetry/trace.c`, `tests/test_trace_circular.c`. It compiles and passes ctest 7/7. NOT reviewed. Known concern: `INNER_RING_RADIUS 0.04` is less than 2*PIECE_RADIUS_NORM (0.042), so inner pieces overlap the queen by 0.002; the layout must have ICF geometry (inner ring 6 touching the queen, outer ring 12) and a unit test. Decide: continue from this tree or reset with `git checkout -- src tests` (the patch stays on disk).
- Other saved artifacts (untracked, Linux repo dir): `wip_r13_broken.patch` (R13 attempt the operator found broken, reference only), `.kimi_directive.md` (R14b, current), `.kimi_directive_r14c_staged.md` (R14c, staged, not yet run).
- Windows 11 build box was deep-cleaned (build dirs, logs, scratch, old helper scripts). Next build there reconfigures from scratch (about a minute). Local `build_fresh/` holds `carrom_arena.exe` (R12) and `carrom_arena_WIP.exe` (R13 attempt, broken; safe to delete).

## What the operator reports on real hardware (R12 build; all still open)
1. Physics wrong: pieces/strikers change direction or speed without cause, launches do not settle, weak/no reaction on collision. Law: straight-line deceleration only until a piece, cushion, or pocket.
2. Initial piece placement wrong (root cause found: `board.c` hand-written tables, ring radius 0.08 vs touching 0.043, a white piece on the queen spot plus a hack offset).
3. Pieces reaching a pocket bounce back.
4. A roughly 15 s startup countdown (real source NOT yet found; not `thinking_min_wall`).

## Work plan (the operator's three-step instruction)
1. R14b (`.kimi_directive.md`): address the sensible review comments. Rejects GL-001 (radii: report used diameters as radii; current radii 0.021/0.028/0.030 already match ICF on 74 cm). Fixes GL-002 layout (with test), PH-01 snapshot half-angle, TEL-04 replay use-after-free, TEL-01/02/03, AI-02/04, MA-04, real tests instead of stubs. Defers PH-02, timing, render polish.
2. R14c (`.kimi_directive_r14c_staged.md`): thorough second review of the code that decides the symptoms (physics step, fixed-step accumulator vs playback speed, manual board-resistance loop, pocket detection, app timing and the 15 s countdown, shot-to-impulse path), then fix with deterministic physics-law tests.
3. Then a fresh Windows 11 build for the operator, and re-enable the deferred CI cells.

## Verified findings of the first review (docs/CODE_REVIEW_R14.md)
Confirmed real: GL-002, PH-01, TEL-04. Wrong: GL-001. Missed: root causes of physics symptoms and the 15 s countdown. The report gave no chunk ranges despite the protocol.

## Model and provider facts (kimi 0.38.0, provider NVIDIA NIM, `~/.kimi-code/config.toml`; backups `config.toml.bak_*`)
- The old default `nemotron-3-ultra-550b-a55b` is dead (404). The config's model catalog is stale: many entries are end-of-life (410) and some are listed by `/v1/models` but 404 when used (`kimi-k2.6`, `nemotron-ultra-253b`).
- Works and is the current default: `nvidia/google/gemma-4-31b-it` (vision + thinking + tools, 256K, 16K output). It completed the full review and made steady progress on R14b.
- Unusable: `z-ai/glm-5.3` (answers a tiny prompt, then hangs silently on real requests), `kimi-k3` and `deepseek-v4-flash-0731` (silent hang), `nemotron-3-super-120b` (runs away to the token ceiling, about 28 min, then dies), `nemotron-3-nano-omni-30b` (cannot orchestrate subagents), `gpt-oss-20b` (works, no vision, small).
- DeepSeek provider is configured (`deepseek/deepseek-flash`, `deepseek/deepseek-v4-pro`, key from Linux `.bashrc`, credentials must live in config.toml because kimi ignores shell env) but the account returned 402 Insufficient Balance; needs a top-up.
- A session pins its model and thinking effort at creation (change model => `FORCE_FRESH_KIMI_SESSION=1`); output cap is re-read on resume.
- Silent provider hangs happen (no error, process idle). Use the stall watchdog `/tmp/watchdog.sh` (restarts with `--continue` after 8 min of log silence; exits and writes `/tmp/watchdog.done`). `/tmp` may be cleared by a reboot; recreate it from this description if missing.

## Resume procedure
1. After the kimi upgrade re-check: `kimi --version`, that `~/.kimi-code/config.toml` still has the gemma default and providers, `agents/*.md` and `OPERATOR_CANONICAL_PREAMBLE.md` intact, and `bash /tmp/relaunch.sh` exists. Smoke-test with `kimi -m nvidia/google/gemma-4-31b-it --agent ceo -p "reply OK"`; a newer version may support other models or fix silent hangs, so re-test the model list before trusting the table above.
2. Existing session for `--continue`: `session_84a3bc55-ad2e-4e86-8658-b8d1e0e111d7` (gemma, R14b partial). If the upgrade breaks session compatibility use `FORCE_FRESH_KIMI_SESSION=1`; the working tree and patches hold the state.
3. Relaunch R14b only via `bash /tmp/relaunch.sh` (never call kimi directly; CEO-only and parallel-subagent gates are enforced by the wrapper and the preamble). Optionally re-arm `/tmp/watchdog.sh` in the background.
4. Verify every claim yourself before believing kimi (read the code, run ctest, build on Windows 11). Real hardware is the only reliable arbiter; Xvfb gives false positives.
5. Re-arm the 6-hour supervision cron (session-scoped, does not survive a session change) if the operator still wants it; every report states the current phase and step. Keep the blog `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\SANYALnet-Labs-Dev-Blog.md` updated.

## Canonical rules (unchanged)
Interact only with the kimi CEO; the CEO must spawn the full company in parallel; never invoke kimi directly; run `scripts/kill-all-runs.sh` before local carrom_arena runs (pre-push hook does it); no attribution lines in commits; do not touch tags; LOCKED_INVARIANTS.md items 1-6 are operator-confirmed, items 7-10 are unconfirmed kimi claims.
