# Carrom Arena: RESUME playbook

**Updated:** 2026-09-26 (UTC). Development is direct and hands-on: the kimi "software company" was fired on 2026-09-24. Nothing is running for carrom. Do not relaunch kimi or use `~/bin/relaunch.sh` / `~/bin/watchdog.sh` unless the operator asks. The unrelated ZX-UX project on the same Linux box must not be touched.

## Repo state
- `main` head: see `git log`; tag `beta-0.0.12` = the spinning-arms release; identical on the Linux box (`~/SOFTWARE-DEVELOPMENT/carrom`), GitHub (`tuklusan/carrom-arena`) and the Windows H: clone. The next tag is `beta-0.0.13` (only when the operator asks; never move existing tags).
- CANONICAL RULE (operator): all build and edit activities happen on the Linux clone, which is the canonical local repo. Changes go from it to GitHub, and then the H: clone is updated to match GitHub and Linux. Never edit or build source in the H: clone.
- Standing operator rule: after ANY change, commit on Linux, run `bash ~/clean_verify.sh`, `bash ~/bin/push_all.sh`, then `git pull --ff-only --tags` in the H: clone, without being asked. `HANDOFF.md` (next to the blog on H:) has the full procedure and the Windows exe build. Edits made on Windows must keep LF endings: the H: clone checks files out as CRLF, so never scp a Windows-side file over a Linux one without converting it.
- The Linux box is ephemeral. "Push" means `bash ~/bin/push_all.sh` (GitHub + the guard against secrets) and then fast-forwarding the H: clone. The blog lives at `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\SANYALnet-Labs-Dev-Blog.md` and is kept up to date as a story for a future blog post (no secrets).
- The Windows exe for the operator is built by CI (artifact `carrom-arena-windows-2022`), downloaded into `build_fresh\` on H: with `gh run download`. The Windows 11 build box is retired.

## How to work (the evidence discipline)
1. Edit on the Linux repo. Build with `cmake --build build_debug` (Debug + ASan/UBSan + -Werror), run `ctest` in `build_debug`.
2. Commit, then `bash ~/clean_verify.sh` (clones the committed HEAD, builds, runs all tests; needs `100% tests passed`, currently 24/24). Then `bash ~/bin/push_all.sh`, then fast-forward the H: clone, then check CI.
3. CI: see step 5; the old composite action `ci-cell` is gone.
4. Look at the real game: run `carrom_arena --mode=rendered` on Xvfb via a SCRIPT FILE (never inline in an ssh command: `scripts/kill-all-runs.sh` kills any process whose command line contains the binary name, including your own shell), screenshot with `import -window root`, and Read the PNGs. `--mode=capture` currently writes blank white frames (open bug: the capture texture is only drawn when the window is hidden).
5. Build and CI: `python build/build.py` (see `build/README.md`) is the ONLY build path, on the Linux clone and in GitHub Actions; `.github/workflows/ci.yml` just picks runners. Six runners cover all hosted architectures (ubuntu-24.04, ubuntu-24.04-arm, windows-2022, windows-11-arm, macos-15, macos-15-intel) and all pass. Queue rule: per runner kind one job runs and one may wait; `admit` rejects a third. Windows exe: `gh run download <run> -n carrom-arena-windows-2022`.
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
- Dead duplicate definitions removed: `common/types.c` redefined seven init functions that `board.c`/`rules.c`/`match.c` also define (which copy linked depended on archive order).
- Console: the exe is a GUI-subsystem program on Windows (`-mwindows`; borrows the parent terminal only for --help/--version/soak).
  All debug output goes to `traces/debug_<seed>.log` (`platform_diag_logf`; raylib's log is routed there too). Old trace/flight/log files
  are pruned to the newest 20 per kind at start-up.

## Robots, teams, scoreboard, radio (2026-09-25 evening)
- Players are robots (`draw_human_figure` in `board_view.c`, drawn from rotated boxes; antenna, eyes, arms, chest panel). The white team is RED and
  the black team BLUE everywhere on screen (`render/theme.h`; the rules code keeps the names white/black). The queen is GREEN.
- Scoreboard top left: red/blue points and games won, a running tally across boards AND games; in rendered mode a finished game or match now starts
  the next one instead of quitting (other modes still stop).
- Radio, bottom right: `audio/radio_stream.c` (worker thread: HTTPS via WinHTTP on Windows, `curl` on Linux/macOS dev hosts; minimp3 in
  `third_party/minimp3`, CC0; mirrors eu/us/fr/fr2 .ah.fm/live tried in turn) + `audio/radio.c` (raylib AudioStream glue, prebuffer, underrun = silence).
  Plays by default. The button pauses; a manual pause stays paused; a stream failure only silences it (button shows play) while it keeps reconnecting and
  resumes by itself. `--no-radio` disables it; `radio_probe` tool checks network + decode; test `radio_stream_test` covers the reconnect logic with a fake source.

## Waiting robots and the active robot (2026-09-25 night, 2026-09-26)
- The robots no longer fade (the flash alpha on the figures during thinking/aim preview is removed; the striker still flashes). A robot that is
  NOT on turn animates in `draw_human_figure` (`board_view.c`): pupils circle inside the eyes and each arm flaps about the shoulder
  (`robot_rbox` draws a box rotated about a pivot). `idle_wave(seat, k, t)` gives each seat its own irregular rhythm. The robot ON turn has no gold outline and NO halo any more (2026-09-26; the gold antenna ball stays, it shows the robot's brain is on) and spins BOTH arms fast, at different speeds and phases so they are out of step (`flap_r = 14 t`, `flap_l = 10.5 t + 2`); its eyes are not animated.

## Build system (2026-09-26)
- `build/` is tracked: `build.py`, `pins.txt` (raylib/Box2D/Unity commit shas), `tools.txt` (pip-installed cmake 3.31.6, ninja 1.13.0). Output goes to `out/` (ignored).
- Portability fixes made for macOS/clang: explicit narrowing of M_PI angles in `board.c`, `-Wformat-nonliteral` pragmas on the two log forwarders, `-Wno-implicit-float-conversion` for clang, no `-lrt` on Apple, `capture_test` uses `timeout` only if present and is skipped on Windows and macOS CI (no window system on those runners).
- The requested build type is honoured (Debug only when none is given). CI builds and tests Debug and Release on all six runners (`--build-type Debug,Release`); artifacts are `...-debug` and `...-release`. Box2D's hardcoded -Werror is neutralised (COMPILE_WARNING_AS_ERROR OFF, -Wno-error=maybe-uninitialized) and the Box2D header patch now recognises its own edit (it used to re-apply on every configure).
- The dependency/ccache caching from the first attempt was dropped when the workflow became a call to `build.py` (actions/cache steps do not fit a one-line workflow); deps are fetched depth-1 each run (seconds).

## Starting position, arranging, break variety, queue tickets (2026-09-26)
- ICF Rule 41 does not fix the orientation of the Y, so `board_setup_initial_formation` turns the whole formation by a random angle taken from `rng->global` (no rng = no turn, which the layout tests use). Rule 41(a) holds at every angle (`test_ICF_Layout_Random_Rotation_Keeps_Rule41a`).
- The break stroke varies: while `board.break_made` is false, `arena_decide` picks at random among the 5 best simulated candidates (`BREAK_CHOICES`) and multiplies the aim error by 8 and the power error by 5 (`BREAK_AIM_SCALE`, `BREAK_POWER_SCALE`); it draws from a copy of the seat stream, so the planning-does-not-consume-the-stream contract holds (`test_ai_break_stroke_varies_with_seed`).
- Arranging (rendered mode only; headless play is untouched): at every new board the coins glide from where they were (on the board or in their stash slot) into the new formation, queen first, then inner ring, then outer ring, each on its own curve (`app_begin_arrange`, `effects_trigger_slide`, staggered 0.08 s, 1.0 s per coin). The game waits (`arrange_wait`, the phase counts as idle) and that is the pause: 0.8 s after a board, 2.5 s after a game, 3.5 s after a match. The very first scene starts with the coins scattered at random (visual-only xorshift seeded from the master seed, never the game's streams) and arranges them after 0.7 s.
- Queue tickets (`build/build.py admit/release`): per runner kind two tickets, git refs `refs/ci-lock/<kind>/<n>` created atomically, so simultaneous requests cannot both take the last one (the old counting gate could be raced). `admit` takes one per kind or rejects the kind; the last step of the build job (`if: always()`) releases it; finished-run tickets are taken over. The concurrency group `carrom-<runner>` still makes the holders run one at a time.

## Turn flow, striker recovery, scoreboard, aim line (2026-09-27)
- Nothing overlaps the next turn: the coins the rules put back (the queen included) slide in first (`EFFECTS_RETURN_SLIDE_TIME`, 0.9 s); then a pocketed
  striker slides from its pocket to the player whose turn it is; `app_resolve_shot` holds the game (`arrange_wait`, the phase counts as idle) until both
  are done. No coin moves during the striker's slide. The route is `striker_path_plan` (`game/striker_path.c`): a visibility graph over points around the
  coins (clearance `STRIKER_PATH_CLEAR`), the other three pockets and the board edge, searched with Dijkstra; the slide is drawn by `effects.c`
  (ease in and out, 0.5-2.5 s) and `board_view.c` hands the striker over on arrival (`effects_take_striker_slide_done`). If a coin sits on the spot on the
  baseline the goal moves along the baseline to the nearest free place. Rendered mode only. (Verified by frames of a forced case; a real striker pocket is
  rare with the expert AI.)
- Scoreboard: fixed 12 px font, every character in the same cell width, `RED  000 00G` (no "pts" text; points 000-999, games 00-99; out of range shows/resets to 0).
  The points are LIVE: `scoring_live_board_points` = coins of the pair's colour in a pocket on the current board (a coin put back stops counting at once)
  plus 3 for a covered queen, on top of the finished boards' points (`score_base`, banked at each new board). This is a coin tally, not the rules' game
  score (`GameState.scores`, the ICF board points that decide the 25-point game); the two are different numbers.
- Aim line: thin (1.5 px), a subdued amber (`(214,160,70)`, distinct from the striker's brighter polished gold; was bright yellow), grows from the middle of
  the striker to its final length (proportional to the strike force) over the first 85% of the aim preview, which is 2 s (`AIM_PREVIEW_SECONDS`). The line stops
  inside the arrowhead (it used to run to the tip and poke out as a tiny fork).
- Striker look: a polished-metal disc (`draw_striker_polished`, `render/striker_draw.h`) - a radial gradient (raylib `DrawCircleGradient`) from a bright warm
  highlight to a darker antique-gold edge, a thin dark rim, a small offset specular highlight. Used everywhere the striker is a solid disc: normal draw, falling
  into a pocket, sliding back to the next player. The THINKING-phase pulsing flash is unchanged.
- Investigated (2026-09-27): "the games counter did not go up at the end of the first game" - reproduced and instrumented; the counter DOES increment correctly
  the instant a game ends (checked in two independent runs, matching the rules engine's own count). A game only ends after 25 points or 8 boards, and each turn's
  thinking/placement/aim-preview phases run at a fixed real time regardless of playback speed, so a game can take several minutes of real play; likely a BOARD
  ending (which does not move the G counter) was mistaken for a game ending. No code change; told to the operator with the evidence.

## Open items (regenerated 2026-09-26, after the arrange/rotation/lock work)
Decisions for the operator:
- Confirm on real hardware that the Release exe (`-release`, the first optimised build ever shipped) plays like the old Debug ones; then decide whether Release is the only exe to hand out.
- `capture_test` is skipped on Windows and macOS CI (no window system on those runners); accept, or provide a virtual display/headless path there. Related bug: `--mode=capture` writes blank frames.
Known gaps (nothing decided needed):
- Aim preview holds 3 s per turn (the aim line grows over 85% of it).
- The scoreboard shows the live coin tally, not the ICF game score that ends a game at 25 points: confirm that is what is wanted.
- Dues: a due with no own coin on the stash to return stays counted only (never enforced later).
- `-Wno-unused-function` hides dead statics (for example `distance_to_board_boundary` in `board_view.c`); `Layout.figure_halo_base_r` is now unused (the halo is gone).
- Windows on ARM and macOS are verified only by CI builds and tests, not on real hardware.
- CI notices: Node 20 actions run on Node 24, and the `ubuntu-latest` and `windows-11-arm` labels change later in 2026 (`admit` and `verdict` use ubuntu-latest).
- The queue tickets are git refs under `refs/ci-lock/`; a job killed without releasing leaves one until the next request finds its run finished and takes it over.
- The operator said earlier there are "many issues": collect more from hands-on testing of the latest exe.

## Locked
The window is locked at 560x560 and NOT resizable (operator, 2026-09-26; no FLAG_WINDOW_RESIZABLE, --width/--height ignored in rendered mode).
Board, piece and striker dimensions are operator-locked (LOCKED_INVARIANTS.md, guard test `tests/test_locked_dimensions.c`): normalized radii piece 0.021, striker 0.028, pocket 0.030, cushion 0.025, board 1.0 = 74 cm.
- Board surface is tinted glass (visual only; physics untouched): COLOR_BOARD in board_view.c is a translucent dark navy over the tron backdrop.

## ICF Laws of Carrom (2026-09-26): `src/game/rules.c`, tests `tests/test_rules.c`, source in `reference/`
The Laws (PDF in `reference/`) are implemented rule by rule; code comments and test names cite the rule numbers.
- Mapping: four seats = doubles (N/S against E/W). The turn passes N, E, S, W; score, coins and the board result belong to the PAIR
  (`GameState.scores.white` = N/S, `.black` = E/W; the robots are red for N/S and blue for E/W). The breaker's pair plays white (ICF 43).
- Implemented: the break and its chances (44, 45; physics reports whether the striker touched a coin), the turn (48), the striker pocketed with or
  without coins, dues and their outstanding state, coins put back by the opponent inside the outer circle clear of the centre circle
  (72-75, 79, 84-89), the queen: right to her, cover, return (92-101), every end-of-board case (52-55, 102-112) with the 22-point and 12-point
  rules, games (25 points or eight boards, an extra board on a tie, 56), best of three (57), the break order between games (49).
- Not applicable to a simulation (never triggered): everything about hands, elbows, sitting, powder, umpires, time limits, technical fouls (63),
  improper strokes (72b, 76, 77, 98b-101b: every stroke is proper), coins jumping the board (65, 66, 116), 51, 91 and the conduct rules 121-143.
  Assumptions: a tie after eight boards plays an extra board with the normal rotation (no toss); the opponent places a due coin farthest from
  the pockets; the striker's additional point (87b) is always demanded; a few both-last-coins cases the Laws leave open are marked in the code.
- Break layout (ICF Rule 41(a), fixed 2026-09-26): queen in the centre circle; the first row (6 coins) alternates black and white; the second row (12 places) holds the Y (three white coins lined up behind the three white first-row coins) and alternates all the way round, so every TIP is white and every NOTCH is black (9 white + 9 black). Positions are unchanged, only colours moved (`board_setup_initial_formation`); `tests/test_icf_layout.c` checks the rule itself (first row alternates, second row alternates, the Y).
- Returned coins (the queen, dues) slide from the pocket they fell into to their spot (`effects_trigger_return`); the queen back on the board has
  her own sound (`queen_back_1.ogg`) and so has a foul (`foul_1.ogg`, Kenney interface error_006).
