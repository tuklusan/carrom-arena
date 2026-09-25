# Carrom Arena: RESUME playbook

**Updated:** 2026-09-24 (UTC). Development is direct and hands-on: the kimi "software company" was fired on 2026-09-24. Nothing is running for carrom. Do not relaunch kimi or use `~/bin/relaunch.sh` / `~/bin/watchdog.sh` unless the operator asks. The unrelated ZX-UX project on the same Linux box must not be touched.

## Repo state
- `main` head `0b3a50c` ("default game speed 1.0x (real time)"), identical on the Linux box (`~/SOFTWARE-DEVELOPMENT/carrom`), GitHub (`tuklusan/carrom-arena`) and the Windows H: clone. Latest tag `beta-0.0.6`; the next tag is `beta-0.0.7` (never move existing tags).
- The Linux box is ephemeral. "Push" means `bash ~/bin/push_all.sh` (GitHub + the guard against secrets) and then fast-forwarding the H: clone. The blog lives at `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\SANYALnet-Labs-Dev-Blog.md` and is kept up to date as a story for a future blog post (no secrets).
- Fresh Windows 11 build for the operator: `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\build_fresh\carrom_arena.exe`.

## How to work (the evidence discipline)
1. Edit on the Linux repo. Build with `cmake --build build_debug` (Debug + ASan/UBSan + -Werror), run `ctest` in `build_debug`.
2. Commit, then `bash ~/clean_verify.sh` (clones the committed HEAD, builds, runs all tests; needs `100% tests passed`, currently 16/16). Then `bash ~/bin/push_all.sh`, then fast-forward the H: clone, then check CI.
3. CI is serialized ("one CI job per runner architecture at a time"): `.github/workflows/ci.yml` with the composite action `.github/actions/ci-cell`.
4. Look at the real game: run `carrom_arena --mode=rendered` on Xvfb via a SCRIPT FILE (never inline in an ssh command: `scripts/kill-all-runs.sh` kills any process whose command line contains the binary name, including your own shell), screenshot with `import -window root`, and Read the PNGs. `--mode=capture` currently writes blank white frames (open bug: the capture texture is only drawn when the window is hidden).
5. Windows build: tar `git ls-files` on Linux, scp to the W11 box (`vagab@192.168.4.103`), extract to `C:/Users/vagab/carrom_wip`, `cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`, build target `carrom_arena`, copy the exe to `build_fresh`, delete the scratch on both machines.
6. Never write the shared machine password anywhere. Never push kimi config. No attribution lines in commits. Do not squash GitHub history (the operator said hold off).

## What is done (2026-09-22 to 2026-09-24)
- Pockets work end to end: sensor events in physics, `rules_resolve` marks pieces pocketed, pocketed pieces are registered in the game state the moment physics pockets them and drawn in their 3x3 slot by the pocket (the "looping pieces" bug is fixed; a piece physics reports pocketed is never drawn from physics).
- Trace: 8 MiB circular JSONL with `POCKET` (immediate), `SHOT_PROGRESS` (every 2 s of sim time) and `SHOT_INTERRUPTED` (flushed on close mid-shot) records.
- Game speed: the configured speed (default 1.0x, `--playback-speed`, keys 0.05x-4x) applies only from striker launch to board settle; thinking, placement and aim preview run at 1x. The app runs at 60 FPS.
- Physics: constant board deceleration 1.3 u/s^2 (about mu 0.10 for a 0.74 m board, no viscous term). No international coefficient exists; the ICF only requires 3.5 runs of a 15 g striker from a base line at maximum force. Full-power measurement (test `striker_realism_test`): crosses the board in 0.18 s, rests after 3.0 s, 7 cushion hits (possibly slightly slippery).
- Layout: compact 560x560 canvas (the side margins equal the top/bottom margin of the board, 94 px; vertical layout unchanged), smaller player figures close to the board, no text occluded.
- Motion: players and the striker never teleport; they glide (2.0 board widths/s) between turns; the thinking slide is a continuous triangle wave; the striker is placed at the planned spot when thinking ends so the drawn striker, the aim line and the launch agree.
- Aim line: length proportional to power (full power = 0.55 board width) with a visible arrowhead (drawn in screen space; raylib culls one triangle winding so both are drawn).

## Endgame loop (fixed 2026-09-25)
Symptom: late in a board every player repeated the same shot and nothing moved. Three root causes, all fixed:
1. `game.board.pieces[i].position` was never updated after a shot, so the AI and the shot planner kept aiming at the INITIAL
   rack layout (now `board_apply_final_positions` after every resolve).
2. A queen pocketed without cover stayed off the board forever (`QUEEN_STATE_POCKETED_NO_COVER` never resolved) and the board
   could not end. Now (ICF 92-101, simplified): the player gets the next stroke to cover her, otherwise she returns to the
   centre; a player who clears his coins wins the board whatever the queen did (ICF 52a).
3. The AI imperfection was drawn from an RNG state that was restored every turn, so a seat repeated exactly the same error;
   now it is mixed with `game.shots_played`. Also: shots that touch nothing are penalised in `shot_evaluator.c`.
Tool: `selfplay` (src/tools/selfplay.c) plays headless AI-vs-AI boards and dumps the AI's view at a stall; ctest
`selfplay_boards_finish` guards it.

## Adversarial code review (2026-09-25) - all four seats equal, expert level
- Skill: the four seats now share ONE accuracy (`EXPERT_AIM_NOISE` 0.008 rad = +/-0.46 deg, `EXPERT_POWER_NOISE` 0.02 in
  `strategy_profiles.h`; it used to be 0.02..0.05, so North was three times as accurate as West). The seats still differ in style weights.
  In self-play the pair that breaks wins the board (first-mover cascade: E/W breaking flips it), not a skill difference.
- The big find: `physics_snapshot.c` carried its own hand-copied, shorter `struct PhysicsWorld` (layout mismatch: restore wrote
  sim_time over pocket bookkeeping), and a world made from a snapshot kept a body at the origin for every POCKETED coin, so every AI
  scratch simulation after the first pocket contained phantom coins in the middle of the board. One shared definition now lives
  in `physics/physics_internal.h`; restore destroys/recreates bodies to match the snapshot; scratch worlds start at sim_time 0.
  Boards now finish in ~25 turns (they took 60-100+ before).
- A pocketed striker is now a foul: turn ends, one own coin (this stroke's first) returns to the centre (`board_find_free_spot`), point removed.
- Also fixed: striker recreated without `isBullet`; snapshot leak per decision; trace ring wrote a NUL instead of a newline at the wrap
  point and leaked a FILE*; reader trusted a corrupt index; NaN passed `match_validate_shot`; `board_get_legal_placements(.., 1)` divided by zero;
  evaluator scored coins pocketed in the simulation as lying at (0,0); planner cap silently dropped later placements; `pcg32_random_float`
  could return 1.0; `--seed 5` (space form, as in --help) was ignored; window size unclamped; window/audio init failure crashed instead of a message.
- Console: the exe is a GUI-subsystem program on Windows (`-mwindows`; borrows the parent terminal only for --help/--version/soak).
  All debug output goes to `traces/debug_<seed>.log` (`platform_diag_logf`; raylib's log is routed there too). Old trace/flight/log files
  are pruned to the newest 20 per kind at start-up.

## Open items
- Blank frames in `--mode=capture` (see above).
- Aim preview holds 2 s per turn.
- Dues: a due with no own coin on the stash to return stays counted only (never enforced later).
- Outer-ring colour pattern of the ICF layout (W,W,B,B pairs vs alternating) is undecided and cosmetic.
- Remaining `-Wno-unused-function` hides dead statics (for example `distance_to_board_boundary` in `board_view.c` is now unused).
- Possibly tag `beta-0.0.7` once the operator confirms the current build on real hardware.
- The operator said earlier there are "many issues": collect more from hands-on testing of the latest exe.

## Locked
Board, piece and striker dimensions are operator-locked (LOCKED_INVARIANTS.md, guard test `tests/test_locked_dimensions.c`): normalized radii piece 0.021, striker 0.028, pocket 0.030, cushion 0.025, board 1.0 = 74 cm.
