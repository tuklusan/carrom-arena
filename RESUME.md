# Carrom Arena: RESUME playbook

**Updated:** 2026-09-30 (UTC), commit `e16260e` (the release workflow's own screenshot-refresh bot commit, one past
`1.0.0` itself - see the dated sections near the end of this file). The project shipped its first stable
release, `1.0.0`, the same day: a full WebAssembly/browser port, two unrelated audio bugs found and fixed,
an AI scoring bug fixed, four rounds of visual polish on the Tron background, a README overhaul (video,
SEO badges, a Carrom Engine design section, two dedicated SEO passes), a license header added to all 92
project-owned source files, and the release itself. Development is direct and hands-on: the kimi "software
company" was fired on 2026-09-24. Nothing is running for carrom. Do not relaunch kimi or use `~/bin/relaunch.sh` / `~/bin/watchdog.sh`
unless the operator asks. The unrelated ZX-UX project on the same Linux box must not be touched. `~/.kimi-code` on the Linux box must be
RETAINED (operator: "that's where kimi lives") even though everything else kimi-era has been cleaned up.

## Repo state
- `main` head is `8ac691a` ("docs: drop duplicate attribution line from top of README", 2026-09-30), one commit past `c012aa1`
  ("Show which colour each pair currently holds on the scoreboard", 2026-09-30), which was four commits past the README-rewrite
  commit tagged `beta-0.0.16` (2026-09-29): `a9623ce` (a display-bug fix), `a856230` (this doc, corrected), `ad6b792` (arena
  kickoff coin toss), `c012aa1` (scoreboard colour-coin indicator), `8ac691a` (README touchup) - all detailed in the dated
  sections near the end of this file. Identical on the Linux box (`~/SOFTWARE-DEVELOPMENT/carrom`), GitHub
  (`tuklusan/carrom-arena`) and the Windows H: clone. Since `beta-0.0.14`:
  a full temp/dead-code cleanup across all three locations (see the dated section near the end of this file); a new
  `.github/workflows/release.yml` (build+zip+publish+verify the six canonical runners, plus a much wider
  screenshot-only matrix for README proof); three real upstream raylib bugs found and fixed via a maintained fork
  (`tuklusan/raylib`, branch `carrom-arena-fixes`, pinned in `build/pins.txt` in place of stock raysan5/raylib);
  `beta-0.0.15`, `beta-0.0.16`, then `beta-0.0.17` tagged and released; and `README.md` rewritten as a short player-facing
  document with a screenshot gallery. CI still only triggers on branch pushes, not tag pushes (see `.github/workflows/ci.yml`),
  so a fresh tag needs a manual `workflow_dispatch` right after if a cleanly-versioned exe matters. The next tag will be
  `beta-0.0.18` (only when the operator asks; never move existing tags).
- **Of the raylib fork's 3 commits ahead of the pinned 5.5 tag, checked today whether any belonged upstream as a PR:** two
  (`InitPlatform()`/`InitWindow()` null-check fixes) are cherry-picks of upstream raysan5/raylib PR #4803 and #4804, both
  already merged upstream back in March 2025 - nothing left to contribute there. The third (`NSOpenGLPFAAccelerated` bypass)
  is explicitly a local-only CI test commit, never meant for upstream. So there is currently no outstanding PR to open against
  `raysan5/raylib`; the fork exists purely to keep this project pinned to 5.5 while carrying fixes upstream already has past
  that point, plus the one local-only tweak. GitHub's fork "Sync" button only fast-forwards the fork's default branch
  (`master`) from upstream - it never touches `carrom-arena-fixes` (a separate branch), and the build pins by exact commit
  SHA in `build/pins.txt` regardless, so syncing `master` is always safe and changes nothing about the build.
- **raylib is no longer stock upstream.** `build/pins.txt`'s `raylib` line points at `https://github.com/tuklusan/raylib`
  (fork), branch/commit `carrom-arena-fixes`, not `raysan5/raylib`. Three commits ahead of the pinned stock 5.5 release:
  (1) `InitPlatform()` in `rcore_desktop_glfw.c` didn't check `glfwCreateWindow()`'s return before immediately using it
  in `GetCurrentMonitor()`, crashing instead of failing cleanly when window/GL-context creation fails; (2) `InitWindow()`
  in `rcore.c` never checked `InitPlatform()`'s own return value either (this is upstream raylib PR #4803, cherry-picked
  verbatim, the other half of upstream issue #4801); (3) GLFW's Cocoa backend (`nsgl_context.m`) unconditionally requests
  `NSOpenGLPFAAccelerated`, which excludes Apple's software renderer entirely - some CI/VM macOS environments have no
  hardware-accelerated GL path at all, so pixel format negotiation failed outright. Fix (3) is gated behind a
  `CARROM_CI_SCREENSHOT` env var real players never set, so this changes nothing about normal play on any platform,
  including real Retina Macs. If raylib ever needs bumping again, these three fixes need to be re-applied (or
  re-verified already-merged upstream) on the new base commit - see the dated section near the end of this file for
  exactly how each was found and why.
- CANONICAL RULE (operator): all build and edit activities happen on the Linux clone, which is the canonical local repo. Changes go from it to GitHub, and then the H: clone is updated to match GitHub and Linux. Never edit or build source in the H: clone.
- Standing operator rule: after ANY change, commit on Linux, run `bash ~/clean_verify.sh`, `bash ~/bin/push_all.sh`, then `git pull --ff-only --tags` in the H: clone, without being asked. `HANDOFF.md` (next to the blog on H:) has the full procedure and the Windows exe build. Edits made on Windows must keep LF endings: the H: clone checks files out as CRLF, so never scp a Windows-side file over a Linux one without converting it.
- The Linux box is ephemeral. "Push" means `bash ~/bin/push_all.sh` (GitHub + the guard against secrets) and then fast-forwarding the H: clone. The blog lives at `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\SANYALnet-Labs-Dev-Blog.md` and is kept up to date as a story for a future blog post (no secrets).
- The Windows exe for the operator is built by CI (artifact `carrom-arena-windows-2022`), downloaded into `build_fresh\` on H: with `gh run download`. The Windows 11 build box is retired.

## How to work (the evidence discipline)
1. Edit on the Linux repo. Build with `cmake --build build_debug` (Debug + ASan/UBSan + -Werror), run `ctest` in `build_debug`.
2. Commit, then `bash ~/clean_verify.sh` (clones the committed HEAD, builds, runs all tests; needs `100% tests passed`, currently 24/24). Then `bash ~/bin/push_all.sh`, then fast-forward the H: clone, then check CI.
3. CI: see step 5; the old composite action `ci-cell` is gone.
4. Look at the real game: run `carrom_arena --mode=rendered` on Xvfb via a SCRIPT FILE (never inline in an ssh command: `scripts/kill-all-runs.sh` kills any process whose command line contains the binary name, including your own shell), screenshot with `import -window root`, and Read the PNGs. (2026-09-27: `--mode=capture` is REMOVED - this Xvfb + `import` approach fully superseded it; see below.)
5. Build and CI: `python build/build.py` (see `build/README.md`) is the ONLY build path, on the Linux clone and in GitHub Actions; `.github/workflows/ci.yml` just picks runners. Six runners cover all hosted architectures (ubuntu-24.04, ubuntu-24.04-arm, windows-2022, windows-11-arm, macos-15, macos-15-intel) and all pass. Queue rule: per runner kind one job runs and one may wait; `admit` rejects a third. Windows exe: `gh run download <run> -n carrom-arena-windows-2022`.
6. Never write the shared machine password anywhere. Never push kimi config. No attribution lines in commits. Do not squash GitHub history (the operator said hold off).

## What is done (2026-09-22 to 2026-09-24)
- Pockets work end to end: sensor events in physics, `rules_resolve` marks pieces pocketed, pocketed pieces are registered in the game state the moment physics pockets them and drawn in their 3x3 slot by the pocket (the "looping pieces" bug is fixed; a piece physics reports pocketed is never drawn from physics).
- Trace (redesigned 2026-09-28, see "One trace file" below): `traces/trace.jsonl`, one fixed file reused and continued by every run, 8 MiB circular, with `POCKET_IMMEDIATE`, `SHOT_PROGRESS` (every 2 s of sim time), `SHOT_INTERRUPTED` (flushed on close mid-shot), `RUN_START`, `LOG` and `APP_EVENT` records among others.
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
  All debug output (`platform_diag_logf`; raylib's log is routed there too) now goes into `traces/trace.jsonl` itself, as `LOG`
  records - see "One trace file" below; there is nothing left to prune at start-up.

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
- Scoreboard (2026-09-27, reworked): each side shows `G:00 B:00 P:000` (fixed font, every character the same cell width; out of range wraps to 0).
  G = games won this MATCH (`total_games`, unchanged from before). B = boards won THIS GAME. P = points THIS GAME (the coin tally: coins of
  the pair's colour pocketed, +3 for a covered queen; moves the instant a coin drops or returns). B and P both reset to 0 when a new game starts.
- G/B/GAME-SCORE FIX (2026-09-30, corrects the two entries this replaces): ICF 43 gives the BREAKER's pair the white coins for THAT BOARD
  ONLY, and ICF 49a(i) has the break (and so the colour) rotate to the other pair every board within a game (confirmed by
  `test_colour_rotation.c`) - that part of the original analysis was right. But `rules.c`'s `winner_pair` (feeding `boards_won_white/black`,
  `scores.white/black` and `games_won_white/black` alike) was never colour-keyed to begin with: `board_result()` always sets it from
  `pair_of_seat(seat)`, which depends only on which SEAT is on the move, never on `seats_swapped` - so all three fields were already
  correctly keyed by PHYSICAL pair (0 = north/south, 1 = east/west) across a colour-rotating game, and the ICF GAME/MATCH winner (G) was
  never actually wrong. The real bug ran the other way: `app_resolve_shot`'s B-counter took that already-correct `boards_won_white/black`
  delta and wrongly re-flipped it with a `seats_swapped` translation, corrupting B (not G) from the second board of every game onward - the
  "verified four-boards-in-a-row" run originally cited for that translation must have been misread or mis-set-up, since a seat's physical
  pair cannot change with a colour swap. Fixed by deleting the translation entirely (`app.c`: `pair_boards_won[0] += dwhite; pair_boards_won[1]
  += dblack;`, no `old_seats_swapped` involved) and adding `test_game_score_survives_a_colour_swap_mid_game` (`test_rules.c`) to lock in that
  a pair's running score/board-count survives a colour swap mid-game. Gated 100% (24/24). No rules-engine change was needed or made;
  `GameState.scores`/`MatchState.games_won_*` are unchanged.

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

## Capture mode removed (2026-09-27)
`--mode=capture` (PNG-per-frame dump), `capture_test`, `--frames`/`--capture-dir`/`--headless`, the renderer's off-screen `capture_texture`/`FLAG_WINDOW_HIDDEN`
path, and the orphaned (never built - no CMake target) `src/app/capture.c` are all gone: dead weight once the Xvfb + `import -window root` +
Read-the-PNG method (step 4 above) took over as how this project actually does visual QA and debugging. The one still-open bug this removes
(`--mode=capture` writing blank frames outside `--headless`) is moot now. 25 tests (was 26).

Dead code also removed while at it: `distance_to_board_boundary` (math.c/vecmath.h, computed but never called), `Layout.figure_halo_base_r`
(computed but never read since the turn halo was removed), `GameState.aim_preview_progress` (written in five places, never read - superseded by
`aim_line_progress`), and `Renderer.width`/`.height` (only existed to support capture-mode's window-resize detection).

## Title readability, app icon, audio-capability gate for the radio (2026-09-27)
- Title: two lines, letter-spaced ("tracked", `fit_tracking`) to spread across the width available, centred between the
  BOARD's own left and right edges (`L->board_x` +/- `board_size/2`), not the whole window and not the space beside the
  scoreboard - capped so it never reaches back under the scoreboard on the left. Font size 10 (smaller than the first
  attempt) and brighter (`(255,253,230)`): measured on a screenshot to leave about 5px clear above the north figure's
  antenna ball, which sits directly below since centring on the board also centres the title above that figure.
- App icon: `assets/icon/app_icon.png` (256px, the running window's icon on every platform, embedded like the audio via
  `src/CMakeLists.txt` and set with `SetWindowIcon` in `renderer_create`) and `assets/icon/app_icon.ico` (multi-size, the
  Windows .exe's own file icon via `assets/icon/app_icon.rc`, `enable_language(RC)` on WIN32 in the top-level CMakeLists).
  Original artwork (generated for this project, `assets/icon/CREDITS.md`): a polished-gold striker in front of a black
  coin on a dark rounded-square badge, echoing `draw_striker_polished`. Verified: raylib decodes the embedded PNG on
  Linux with no errors; the Windows resource compile itself is only proven by CI (windres on windows-2022).
- Radio: the button is now hidden completely (not just inert) when there is no audio device, not only when `--no-radio` is
  given (`renderer_set_radio`'s availability now requires `audio_ready()` too). `audio_init()` logs a line to the debug
  file when no device is found, so a missing radio button is traceable to "no audio" rather than looking like a bug.

## The shooting robot lines up the shot: telescoping arm and fingers (2026-09-27, corrected the same day)
The FIRST version moved the robot's whole body onto the board's cushion line - wrong (the operator caught it: "the robot's
body CANNOT ENTER THE BOARD"). Corrected: the body never leaves its own fixed outside-the-board line (`north_fixed_y` etc);
only where it stands ALONG that line changes, to wherever the aim line - extended BACKWARDS through the striker - crosses
that SAME fixed line (`ray_cross_fixed_line`, replacing the old board-edge projection), in exact lock-step with the line's
own growth (`game->aim_line_progress`). It still turns (a vertical-axis, top-down turn, per the operator) to face straight
down the line (shortest-path blend, `lerp_angle_shortest`).
Reaching the striker is now a real telescoping arm: three sliding segments (`draw_human_figure`, `render/board_view.c`)
extend from the shoulder, across the cushion, stopping just short of the striker's near side; a hand of three telescoping
fingers (thumb, index, middle) then continues from there. Two of the three extend on to the striker's FAR side to flick
it; which pair depends on the shot (`classify_strike_side`, comparing the shot's own direction to the seat's ordinary
square-on facing): a forward strike pinches with thumb+middle, a parallel/low strike to the left flicks with thumb+index,
to the right with index+middle; the third finger stays short and never touches the striker. Once the shot fires everything
eases back over about 0.4 s (`REACH_WITHDRAW_SPEED`), not an instant snap. Per-seat state
(`VisualState.reach/aim_pose_pos/aim_pose_angle/aim_pose_side`), so all four seats animate independently.
Verified with screenshots through a whole aim-preview-to-shot sequence: the body stays off the board and turns correctly;
the arm crosses the cushion and a finger tip is visible flicking past the striker's edge. Caveat: at this game's small
sprite scale (about a 9-10 px head radius) the three fingers are only a pixel or two wide each and read as one small dark
mark beside the striker, not as three separately readable digits; the mechanism (which pair extends, distances) is
implemented as described, but seeing three distinct fingers by eye would need the whole hand enlarged.
Two more corrections the same day:
- Position: no longer an unclamped single infinite line (the operator's first answer, superseded) - the four seats' fixed
  outside-the-board lines are now treated as ONE rectangle (`project_to_standing_rect`, a proper ray/box exit), so a shot
  too raking to exit through its own seat's side carries the robot round the corner onto the neighbouring side instead of
  off to the side indefinitely; still never on the board. Verified: a steep shot moved the robot right up against the
  corner marker.
- The right arm's spin: multiplying an ever-growing spin angle by `(1 - reach)` LOOKED like it kept whipping round right up
  to the last moment then snapped, because the amplitude shrank but the angular speed did not. Fixed: the live spin angle
  is captured the instant `reach` starts rising (`VisualState.arm_spin`, updated only while not reaching) and the arm eases
  from THAT captured angle to a straight rest as `reach` goes 0 to 1 - a true slowdown, not a shrinking wobble.
- The telescoping arm was drawn as a SEPARATE set of boxes starting near the body's centreline, while the ordinary resting
  right arm (still drawn every frame, just with its spin eased to 0) stayed at its own resting position off to the side -
  two disconnected pieces (the operator: "the telescopic arm...start disjoint"). Fixed: there is only ONE right arm now.
  Its forearm and hand boxes always rotate about the SAME fixed shoulder pivot (as they always did), and as `reach` rises
  their FAR edges (never the shoulder-side edge) interpolate from the resting shape to the outstretched one - straightening
  onto the shot line and stretching toward the striker - so at `reach` = 0 it is pixel-identical to the old resting arm, and
  it never leaves the shoulder at any point in between. The telescoping fingers now begin exactly at the stretched hand's
  own tip, in the same rotated frame, so there is no seam there either. Verified with screenshots at two consecutive
  moments: the arm runs continuously from the shoulder, across the cushion, to the striker.
A further correction, same feature (2026-09-27): the operator reported "the arm becomes disjoint from robot as robot
rotates" - a different trigger than the two fixes above (those were about the arm's own SHAPE; this one is about body
ROTATION). Root cause: the arm's stretch target (`u_to_striker`/`v_to_striker`, where the striker sits in the robot's own
u,v terms) was computed against the LIVE, still-turning body frame. For a shot needing a large turn (especially the
corner-wrap cases just above), the target swung around while the body was mid-rotation, which looked like the arm tearing
away from the shoulder as the robot turned. Fixed by computing that target against a separate frame built from the FINAL
orientation the body is turning to (`VisualState.aim_pose_angle`, already cached for the turn animation itself), not the
live blending angle; the target now stays fixed while only the arm's shape (still drawn through the live-rotating frame)
eases toward it, so it can never appear to leave the shoulder mid-turn. Verified with a seed=22 screenshot sequence
through a large corner-wrap rotation: the arm runs continuously from the shoulder to the hand at every sampled frame.
That fix was itself wrong (2026-09-27, same day): the operator reported the east seat specifically striking with a
detached arm on EVERY shot, not just during a transient. Root cause: the arm's target was computed in the FINAL-
orientation frame above, but drawn via `robot_rbox(&f, ...)` - the body's LIVE, still-rotating frame. Those are two
different rotation bases whenever the two angles differ, which for any shot needing a real turn is the entire approach,
not a moment; the (u,v) numbers, valid only in the final frame, were read directly as coordinates in the live frame, so
the arm pointed the wrong way for as long as the body had not yet finished turning - a genuine coordinate-system bug, not
a cosmetic wobble. Fixed per the operator's own diagnosis: the target is now recomputed every frame from the CURRENT live
frame `f`, the same frame everything else about the arm uses - so the target and the shape are always in the same
coordinate system, and the arm continuously re-aims at the striker's true position as the body turns. The `final_angle`
parameter and the separate `final_f` frame were removed entirely. Verified with a seed=7 screenshot sequence covering the
east seat's first turn (frames 049-052, the reach/rotation ramp through full reach): one continuous arm, no gap, at every
sampled frame.
The operator was still right (2026-09-27, same day, third pass): the arm kept detaching. This time, instead of guessing
from screenshots, a deep per-frame trace was added - logging the TRUE shoulder pivot against the arm's own rendered near
corner for every single frame the arm was reaching, across an entire board (seed=42, all four seats, 23 shots, 3191
sampled frames). The trace showed the real root cause at last: the gap between them grew CONTINUOUSLY with `reach` - not
a rotation transient, but on every single strike, up to 190-310px at full extension, on all four seats. Cause: the
reaching forearm/hand was an axis-aligned box in body (u,v) space whose v0/v1 - shared by BOTH the near (shoulder) edge
and the far (hand) edge, since a plain rectangle has one v-range for its whole length - were interpolated together
toward the striker's v-offset as reach grew. That slides the WHOLE box sideways, including the near edge, which is
supposed to stay at the shoulder; rotating that box by `flap_r` (eased to 0) never fixed this, because at flap_r=0 the
rotation is the identity and the box renders exactly where its drifted numbers put it. Rebuilt as it always should have
been: a RIGID shape - fixed length and width in its own local axes - that only ROTATES about the true, fixed shoulder
pivot and TELESCOPES along its own axis as reach grows; its near corner sits at zero u-offset and a small constant
v-offset from the pivot, so it stays within that same small, bounded distance after any rotation and can no longer drift.
One rotation angle now carries both the old jobs (easing the spin to a stop, then aiming) as a single continuous blend
from the live spin angle to the target bearing. Verified by re-running the IDENTICAL trace on the fixed build: max gap
dropped from 190-310px to 1.9px, bounded by the fixed pivot offset, not growing with reach at all, across every seat and
every sampled frame. Also confirmed visually on the east seat. The diagnostic trace logging was removed before shipping.
One more correction the same day: the operator noticed the fingers were reaching the striker's CENTRE, not flicking
through to the far side ("the fingers must end at the diametrically opposing side of the strike's direction"). Cause: the
near/far reach targets were built by shifting only the U-component of the striker's position by its radius, reusing the
SAME v-offset for both - which only lands on the striker's actual circle when the approach line happens to run parallel
to the body's own u-axis; at any other angle the point drifts off the circle, so the fingers closed on a point partway
in rather than the true rim. Fixed by measuring the real straight-line distance from the pivot to the striker's centre
and stepping the radius off ALONG THAT SAME LINE, so the near point lands on the near rim and the far point on the far
rim - diametrically opposite, through the centre - whatever angle the approach happens to be at. Verified with a
temporary numeric probe (a marker drawn independently at the computed distance, confirmed sitting exactly on the far
rim) and a pixel-exact zoom on the shipped build showing the finger reaching that same point.
Two more corrections the same week (2026-09-28), from a single operator report: "the telescoping arms...are extending
too much...crossing beyond the required exact distance" and a request to "have the robots use one of both arms...if the
left arm is easier to get to the striker".
- Overshoot: the arm's LENGTH (distinct from its rotation and drawn shape, which correctly track the live body frame)
  was being measured against the live, still-moving positions of the robot and the striker. While the body was still
  sliding into its final standing spot the raw distance swung with it, so the arm briefly overshot before settling;
  after the strike, the striker starts flying across the board while the arm eases back, and the same live-tracking
  made the retracting arm chase it. Fixed by measuring length against the shot's FROZEN final geometry instead: the
  already-cached final standing pose, plus a new `aim_pose_striker` snapshot of the striker's pre-strike resting spot,
  captured once when the aim preview starts and held through the whole reach and withdrawal. Verified with a per-frame
  probe across an entire board: the pivot-to-striker distance is now EXACTLY constant within every episode (zero
  spread), versus up to 349px of swing before.
- Handedness: the reaching arm was always the right one, so it could end up reaching across in front of the body for a
  striker on the left. Fixed by deciding, once per shot from the same frozen final pose, which side the striker is
  actually on, and reaching with that arm - the other arm just keeps its own flavour animation. Verified across the
  same board: both sides get used (roughly half and half), and a visual check on a left-arm reach renders cleanly.
A further correction the same week (2026-09-28): the operator reported the overshoot was STILL there, and asked for a
detailed trace of the striker's true closest point against the arm's actual max extent, rather than another guess.
That trace found a second, smaller overshoot source (up to ~14px, versus ~300px before): the length target above is a
stable constant measured against the body's FINAL position, but the arm is DRAWN from the body's LIVE, still-moving
position (needed to stay attached). While the body has not yet arrived, a length correct for the final pivot can carry
the rendered tip past the striker's TRUE near/far edge as measured from where the arm is actually drawn right now, even
though it always settles back to correct by reach = 1. Fixed with a hard clamp: the live pivot-to-striker distance is
computed fresh every frame and used only as a ceiling (never as the smooth growth target, so it does not reintroduce
the original jitter) - the near/far length targets are capped to whichever is smaller, the smooth frozen target or what
the live geometry currently allows. Verified with the same trace across an entire board: the signed overshoot is now
NEGATIVE at every single sampled frame in every episode - the arm always stays a fraction of a pixel short of the true
edge, never crosses it.
A deeper, more fundamental correction the same day (2026-09-28): the operator sent an actual screenshot - the hand was
reaching to the LEFT of the striker while the aim arrow pointed up-and-right, with the note "there is no realistic
physical way that hand can launch the striker in the direction of the arrowhead." Root cause, different from anything
above: the target the arm aimed at was "whichever point on the striker's circle is closest to the ARM'S PIVOT" (found
via atan2() from the pivot to the striker's CENTRE) - not the point fixed by the shot itself. Those only coincide when
the pivot sits almost exactly on the aim line; the arm attaches to the SIDE of the body, so it never quite does, and
for a close-in shot (a robot standing right next to the striker at a corner) the gap becomes large and obviously wrong.
Fixed by dropping the pivot-relative atan2() entirely: since the body already faces along the shot's own aim direction
by construction, the correct contact point is simply the striker's centre offset by its radius along the body frame's
own +u axis (v untouched) - true for every seat and angle, independent of where the pivot is. The pivot now only
decides the rotation and length needed to REACH that fixed point, never where the point is. The far point (for the
fingers) is not a second independently-aimed target either - a rigid arm cannot point at two spots at once - it is the
near point plus the striker's own diameter, continuing along the same ray. The two length fixes above still apply,
now built on the corrected point. Verified by reproducing close-in shots and comparing the contact point against the
aim arrow directly: in both a heavily angled close-range case and a nearly straight one, contact now sits exactly
opposite the arrow.
The fingers themselves were removed the same day, on the operator's explicit instruction (2026-09-28): two more
screenshots showed the flicking fingers crossing all the way through the striker to its far side, and the operator
confirmed that crossing IS the problem, not a side-effect of the geometry bugs above - "the closest point of the
striker to the robot is the point the arm can maximally extend to." Rather than cap the fingers at that point, the
operator asked to remove the whole finger mechanism outright: "take the fingers out; that logic is another whole
software evolution, we will push it to a future enhancement." Removed the three-finger draw block and the now-unused
`r_far` target; the hand alone (already verified above to reach the near rim exactly, with no overshoot) is now the
whole reaching mechanism. `strike_side` is still threaded through, unused, for whenever the finger mechanism is
rebuilt. The operator also sent a reference image of the real "Scissors Grip"/"Straight Grip" carrom techniques for
that future rebuild - noted in project memory (`project-finger-grip-reference.md`), not acted on now.

Two more corrections the same week (2026-09-28), both from the operator, both about the reaching-robot animation:

- "The robot's left and right arms are getting confused somewhere...the striking arm is suddenly swapping...in a jerky
  weird flipping." The arm-side decision was recomputed from scratch every frame; for a near-dead-straight shot that
  test sits right at its own +/- boundary, and while the geometry it read was itself stable, redoing the SAME
  borderline test every single frame left it exposed to flipping mid-motion. Fixed by LATCHING it: decided once, at
  the instant `reach` is still at rest for a shot, never touched again until the arm returns to rest for the next one.
- "Find the optimal algorithm for placing the robot...that causes the minimum mathematically possible rotation and arm
  extension." Worked out the actual optimum: the body's ROTATION is fixed by the shot itself (it must face the shot's
  reverse direction, regardless of where along the boundary it stands), so it is already at its one, trivially minimal
  value everywhere - the only thing position can affect is EXTENSION. Minimising extension is then just "stand at the
  closest point on the boundary to where the arm needs to reach," restricted to the seat's own side plus its two
  adjacent sides (never the opposite one) - a closed-form nearest-point-on-a-rectangle computation, replacing the old
  ray-cast (which did not minimise anything, it just went wherever a line in the reverse-shot direction happened to
  land). This standing position is ALSO now latched, at the same instant as the arm side and for the same reason -
  recomputing it from the still-settling striker mid-reach is almost certainly what caused the "jerky flip" report in
  the first place, not just noise in the side test alone. (Simplification, noted rather than hidden: this optimises the
  BODY's centre distance to the target, not the arm's own shoulder pivot, which sits a small, fixed offset to the side
  of centre - a few percent of a typical reach, not enough to change which side is closest.)

Verified with a per-frame probe across an entire board: 0 of 12 shots showed more than one arm-side value across their
whole reach-and-withdrawal cycle (previously every one was exposed to a possible flip); extension distances at settle
are all sane. Visual check confirms the arm still reaches correctly, attached, to the right contact point.

A third correction the same week (2026-09-28): two more screenshots, on two different seats, both "both arms...on the
same side after the last shot." Not the arm-side latch above (verified separately, stayed correct) - a different bug:
the game hands the turn to the NEXT seat as soon as a shot resolves, well before this seat's own ~0.4s visual
withdrawal (reach easing 1 back to 0) has actually finished, since that easing is purely cosmetic. The reaching arm's
rotation and the other arm's animation were both gated on `is_current_turn` alone, so the instant the game moved the
turn on - while the arm was often still visibly extended - it snapped straight from "pointing at the target" to
"idle waving" (an unrelated, independently-oscillating angle): a sudden jump in a still-long arm, easily read as it
swinging onto the same side as the other, already-idle arm. Fixed by driving the animation state from `reach` itself:
`reaching_active` stays true for as long as reach is meaningfully above 0, regardless of whose turn the game now says
it is. Verified with a per-frame trace: found the exact transition in the wild (reach still at 0.22 when the turn
changed) and confirmed the fix carries it through smoothly - a 6.5 degree shift, a continuation, not a snap.

A fourth correction the same week (2026-09-28), and the deepest one: two more screenshots, an east robot that "never
recovered its right arm" and, in the same shot, a west robot whose "extended arm does not match the striker's vector."
A long multi-board trace (700+ samples) first ruled out the obvious suspect - `reach` itself always decayed correctly -
then, at full settled reach, showed the arm's target angle averaging 93 degrees off zero, sometimes nearly 180. Root
cause: `game->computed_shot_plan.aim_angle` is a WORLD-space angle (confirmed by the aim arrow, which adds its
cosf/sinf directly to a world-space point before converting to screen space); `math_world_to_screen` flips Y, so a
world direction's screen equivalent negates its y-component - the angle needed once everything downstream is in screen
pixels is `PI - aim_angle`, not `aim_angle + PI`. The robot's own body-orientation system (`away`/`f.back`,
SEAT_DEFAULT_ANGLE) is never passed through that flip - it treats `angle` as already screen-native, which is why every
idle robot has always faced correctly at rest - but the shot's own target angle was the WORLD-space `aim_angle + PI`,
fed into that screen-native system unconverted. Small rotations barely showed the mismatch (why so many earlier
screenshot checks looked right); a real turn could point the arm up to 180 degrees off. Fixed by keeping two angles
where the code had conflated one: the existing world-space value stays for the one genuinely world-space use (the
contact point); the robot's own orientation, the arm-side decision, and the finger-selection helper now use the
correctly mirrored, screen-native one. A second, smaller, unrelated bug found in the same pass: yesterday's "optimal
standing position" exclusion guards (never place a robot on the board's opposite side) had the seat pairings backwards
- fixed too, though confirmed (identical before/after trace numbers on its own) not to be the cause of the angle
mismatch. Verified with the same trace, before/after: average angular error at settled reach dropped from 93 to 22
degrees, and the remaining 22 is the shoulder's own small, expected sideways offset from the body's centre (confirmed
against atan(offset/distance) directly), not a bug. Confirmed visually on two more shots: the arm lands exactly
opposite the aim arrow in both.

A fifth correction the same week (2026-09-28): the operator specified a canonical rule rather than reporting a specific
glitch - "At the end of a turn, a robot MUST collapse and return BOTH arms to their normal extents (lengths) and
positions...Only after returning arms to normal positions, the idle-robot hand animation will resume." LENGTH already
did this correctly (every reach-dependent length is `lerp(rest, target, reach)`, landing exactly on rest at reach=0 by
construction); ANGLE did not, in two ways: the reaching arm eased, during withdrawal, back toward the essentially
random angle it happened to be spinning at when reaching began, not toward neutral; and the other (flapping) arm's
ever-increasing "excited spin" simply stopped and handed off straight to idle-waving the instant it was cut off, with
no guarantee the two even agreed. Fixed with a proper three-state machine per seat (RESTING / GROWING-or-HOLDING /
WITHDRAWING, using the running peak of `reach` since last rest rather than a frame-to-frame comparison, so the flat
hold right before a shot fires is never mistaken for withdrawal already starting): the instant real withdrawal begins,
both arms' current angles are captured once and eased from there back to exactly 0 purely as a function of `reach`
itself falling to 0. Verified with a per-frame trace across six boards: at every one of 89 sampled withdrawal-tail
windows, both angles converge to under one degree of true neutral.

A sixth correction the same week (2026-09-28): the fifth fix's state machine held up under trace, but the operator sent
another screenshot - an east robot with both arms in an asymmetric, outward-splayed "V", well after its turn and after
two more turns had passed. Rather than re-suspect the withdrawal transition (already trace-verified 89/89), this time
the idle-waving that takes over once a seat is fully at rest was checked, and it had never been examined against the
canonical rule at all: `idle_wave()` (range roughly -1..1) was scaled by `0.9f` for each resting arm independently -
about 51 degrees of swing per arm. The state machine genuinely does land both arms at exact neutral the instant `reach`
hits 0; idle-waving then immediately took over and was free to swing either arm up to 51 degrees off the body, and at
an unlucky phase (both arms swung outward at once) that is indistinguishable, in a screenshot, from "arms not in their
normal positions" - even though nothing was stuck or broken in the transition logic itself. This is a different bug
from any of the previous five: not a transition/state-machine defect but the resting animation's own amplitude never
having been checked against "parallel to the correct side of the robot." Cut to `0.15f` (about 8 degrees) so a waiting
robot still reads as alive without ever looking untucked. Verified visually: captured frames across a played session
show all four seats, including east and west specifically, holding arms tucked parallel to the body while idle, and
the actively-reaching seat still extends correctly toward the striker mid-shot.

A seventh correction the same week (2026-09-28), and the one that finally matched what the operator actually asked for:
"the left arm should always start and end at the left of the robot; the right arm should always start and end at the
right side...the only exception is the transient frames where either arm is under use for a strike." The sixth fix's
amplitude cut made idle sway small enough to stop looking splayed, but did not touch the real bug underneath, which a
fresh screenshot of west and east robots exposed as a shapeless merged blob at the shoulder - both arms bunched at the
body's own centreline, not on either side at all. Two distinct causes, found by direct code reading and confirmed with
a diagnostic trace, not another screenshot: (1) the resting arm's rotation PIVOT has always correctly mirrored with
`side_sign` (which arm reaches this shot), but the resting arm's own BOX COORDINATES were a hardcoded literal that
never depended on `side_sign` at all - a leftover from before either arm could reach, when the resting arm was always
the fixed left one; whenever `side_sign` flipped, the pivot moved to the new side but the drawn shape stayed on the
old one. Fixed by mirroring the box's own coordinates with `side_sign` exactly the way the pivot already does. (2)
`g_vis.arm_side[seat]` (`side_sign`) is zero-initialized like every float in the struct, but 0.0f is not neutral here:
both arms' geometry is directly proportional to it, so at exactly 0 - true for any seat before its own first shot of
the whole game - both arms collapse to the centreline together. A trace confirmed it immediately: `side_sign=0,
idle_side=-0, pivot_v=0.000`, the idle arm's two corners landing on the exact same point, for every seat sampled
before its first shot. Fixed with a sane non-zero default, overwritten the instant a real shot decides the real side.
Verified visually across a full played session (100 sampled frames): west and east both show two arms correctly split
to opposite sides of the body from the very first frame - before either has taken a shot - and stay correctly split
through many turns afterward.

An eighth correction the same week (2026-09-28): the seventh fix held up under its own trace and its own screenshots,
but the operator sent one more - west and east both broken again, "even at the beginning." This time the operator's
own wording pointed straight at the answer: "the only exception is the transient frames where either arm is under use
for a strike" - EITHER, singular, the one actually striking. A fresh trace of the OTHER arm's own angle during a real
reach found it: `idle_ang` reaching into the THOUSANDS of degrees (6452.7, 7364.9, 8276.1 sampled live). The
non-reaching arm's angle was `10.5f * t + 2.0f`, fed straight into cosf/sinf with `t` running unbounded for the whole
session - a deliberate "excited flapping" flavour animation, written before the canonical rule existed, that the
operator's rule never actually exempted. Mathematically still a well-defined rotation, but at whatever instant a
screenshot lands on, that arm could be caught rotated to point along the body's own axis instead of out to the side,
tucking itself edge-on into the torso's own silhouette where it simply reads as gone - exactly "one arm visible, the
other isn't there." Fixed by giving the non-reaching arm the same small idle sway a resting arm gets instead of
spinning it at all, so it stays visibly parked on its own side for the whole time the other arm is out on its own
strike (and fixed the matching one-time capture at the start of withdrawal, which took the same unbounded value as its
own starting point). Verified visually this time, not just numerically: captured a full reach-to-withdrawal sequence
at normal speed and stepped through it frame by frame - the reaching arm correctly extends toward the striker, the
other stays small and visibly tucked the whole way through, never vanishes, never swings wildly.

A ninth investigation the same week (2026-09-28): two more screenshots, "at least one idle robot with incorrect arms."
This time it did NOT reproduce. An 80-second live session with a diagnostic trace sampled every 2 seconds per seat,
cross-checked against screenshots captured at the exact same score as the operator's own first image (P:004/004),
showed every sampled seat correctly split - one arm on each side, mathematically and visually - in every sample. The
two screenshots this round arrived without a saved file path (every earlier one in this whole saga had one), so they
could not be pixel-inspected directly; verification stopped there, honestly reported as unreproduced rather than
claimed fixed, pending the actual files.

## Trace file: audit and one-file redesign (2026-09-28)
Two separate operator requests, both about `traces/`, not about gameplay:

**A full line-by-line audit** (reconstructing the ring buffer's true chronological order in Python, mirroring
`trace_read_last_records()` exactly rather than trusting a plain read) found five real defects, all fixed the same day:
1. `trace_write_pocket()`'s immediate record and the unrelated `EVENT_POCKET` game event both serialized
   `"type":"POCKET"` with incompatible field sets - the immediate one renamed to `POCKET_IMMEDIATE`.
2. A `POCKET` event's `team` field held the POCKETED PIECE's own team (rules.c sets it from the piece's colour), while
   every other event's `team` holds the ACTING SEAT's own team - same field name, two silent meanings. Renamed to
   `piece_team` specifically for `POCKET`, in both the JSON writer and its (now-removed, see below) human-readable
   mirror.
3. `shot_end.result.final_positions` always carried a 20th, permanently-unused, always-`(0,0)` phantom entry (the
   writer looped to 20; `MAX_PIECES` is 19 and nothing ever writes index 19) - loop bound fixed to match every other
   reader of this array.
4. `shot_end.runtime_errors` was a hardcoded literal `"[]"` in the format string, not populated from any real error
   state - removed rather than leave a fake diagnostic channel in the schema.
5. `trace_read_last_records()`'s wrap detection inferred "has this ring actually wrapped" from the on-disk FILE SIZE,
   which is always the full 8 MiB+header from the instant the file is created (it zero-fills eagerly) regardless of
   how much real data has ever been written - so the flag was always true. Measured directly: a file that had written
   only 263,601 of 8,388,608 bytes still produced an 8,125,007-byte leading run of zero padding in the reconstruction.
   Harmless by luck, wrong and wasteful in principle. The index header is now 16 bytes - write offset plus a real,
   persisted wrapped flag - and the reader uses that instead of guessing. (Two related overrun bugs found while
   widening the header: both the writer's and the reader's old code read/wrote the new 16-byte header into/out of a
   single 8-byte `uint64_t`, which would have corrupted adjacent memory - fixed alongside.)

**"We must have EXACTLY ONE TRACE FILE... NO ADDITIONAL FILES"**, the operator, on seeing three separate files after
the above (`trace.jsonl`, a `flight_<seed>.bin`, a `debug_<seed>.log`, plus trace.c's own `seed_<seed>.log` mirror).
All folded into the one shared `traces/trace.jsonl`, and the separate systems removed rather than left disabled:
- The binary flight recorder (`telemetry/flight.c/.h`, `tools/flight_dump.c`, its own test) is gone entirely. Its
  discrete EVENT records (phase/turn/speed/pause/layout/sound/pocket/shot boundaries) are now `APP_EVENT` JSONL
  records (`trace_write_app_event()`), self-describing via a `kind_name` string, no decoder tool or enum needed to
  read them. Its other role - a full 19-piece binary snapshot of every single rendered frame - is deliberately NOT
  replicated: trace's own `PHYSICS_STATE` and `SHOT_PROGRESS`/`SHOT_INTERRUPTED` already cover physics state at a
  size the shared 8 MiB budget can sustain across a real session; a full-board JSON snapshot every frame would fill
  the ring in seconds.
- `platform_diag_open()`/`_close()` (its own `debug_<seed>.log`) is replaced by `platform_diag_set_sink()`:
  `platform_diag_logf()` hands its line to a caller-supplied callback instead of a file it owns. `app.c` wires that
  to `trace_diag_sink()` (new, in trace.c) right after `trace_open()` succeeds; it JSON-escapes the line and writes
  it into the same file as a `LOG` record. `platform.c` stays generic - it never learns what a "trace" is.
- trace.c's own optional human-readable mirror (`log_dir`/`verbose`, `seed_<seed>.log`, and the `events_log()`
  function that formatted it) is gone; `trace_open()` dropped those two parameters everywhere (app.c and four test
  files updated).
- The trace itself moved from a seed-suffixed name (`trace_<seed>.jsonl` - meaning any run with a different seed,
  the common case, silently started a brand-new file) to the single fixed `traces/trace.jsonl`, created if it does
  not exist and otherwise reopened and continued (that ring-continuation logic already existed; only the naming
  needed to change). `trace_open()` now writes a `RUN_START` marker - timestamp, build id, process id, this run's
  seed - on every open, so a reader can find exactly where the latest run's own data begins in a file many runs now
  share. `platform_prune_old_files()`, which kept the newest 20 of each per-run-named file, has nothing left to
  prune and is removed along with its call site.

Verified end to end on both platforms, not just against the source: cleared `traces/` entirely, ran the real game for
15 real seconds on Linux, confirmed exactly one file existed afterward, and reconstructed and parsed its full ring -
all 164 records valid JSON, one correct `RUN_START`, 91 `LOG` records (raylib's own startup banner included), 31
self-describing `APP_EVENT` records, every pre-existing record type still present and correctly formed alongside
them. Then downloaded and ran the actual Windows CI exe: it too produced exactly one well-formed `trace.jsonl`
(87 valid records, correct `RUN_START` first), confirming the design works identically cross-platform. Full test
suite: 24/24 (`flight_recorder_test` is gone with the subsystem it tested; the former log-file-size test in
`test_trace_circular.c` is replaced with one that verifies the diag-sink integration itself).

## The reaching-arm feature is GONE (2026-09-28) - robots now just wave
The operator, after a full day of this session chasing one arm-position bug report after another: "the whole idea of
the robots rotating and extending arms to strikers is becoming too complex and too hard to get right... let us keep
it simple. Idle robots wave their hands slowly, each robot in a different way. The active robot, when actually making
a strike, spins his hand rapidly during striker placement to striker launch, then goes back to lazy waving hands."

Rather than fix it a tenth time, it was deleted. Gone entirely: the robot sliding along its own outside-the-board
line and turning to face the shot as the aim line grew (`closest_standing_point()` and the whole per-seat
`aim_pose_pos`/`aim_pose_angle` latch); the telescoping arm reaching across the cushion to the striker (`theta_target`,
the live-distance clamp, the length/width lerp-to-target math); the "which arm reaches" side selection
(`arm_side`/`side_sign`) and the now-doubly-unused finger-flick classification that fed it
(`classify_strike_side` - the fingers themselves were removed even earlier); the three-state withdrawal machine that
eased both arms back to neutral after a reach; the "excited flapping" of the non-reaching arm and its later "parked"
fix. Every one of these had its own bug, its own fix, its own trace-verified confirmation, across nine separate
rounds this same day - the whole mechanism was simply too complex to keep getting right.

A robot now always stands at its one fixed seat position and its one fixed default facing angle, for the entire
game - no sliding, no rotating, ever. Both arms are always at the same resting length; only their rotation angle
changes, and only one of two ways: `idle_wave()` (unchanged - every seat's own irregular frequency/phase, so no two
robots ever move in lockstep), or, for whichever single seat is between striker placement and launch
(`is_placement || is_aim_preview` for the current-turn seat), a fast, matched spin on both arms at once (reusing the
old "excited" spin formula, now applied symmetrically instead of to whichever arm wasn't reaching). The moment the
shot fires or the turn moves on, that seat drops straight back to idle-waving - there is no transition left to ease,
because there is no longer a target angle to ease from.

`VisualState` shrank from thirteen per-seat animation fields to none beyond what pre-dated this whole feature
(`striker`/`fig`/`was_gone`/`appear` - unrelated position/fade-in tracking); `draw_human_figure()`'s signature
dropped six parameters (reach, striker_world, strike_side, arm_spin, final_pos, final_angle_param) for one
(`hands_spin_fast`). Net: `board_view.c` dropped from 1080 to 716 lines. Verified: clean build, 24/24 tests untouched
(none exercised the removed machinery directly); captured a played session and confirmed visually - all four robots
stay planted at their fixed corners/edges the whole time, and the currently-placing/aiming seat's arms visibly
rotate between consecutive 0.3s-apart frames (a second arm swings into view between two samples that would
otherwise look identical), confirming the fast spin is real motion, not a stuck frame, while every other seat shows
the slower per-seat idle sway throughout. This also closes out the still-unreproduced arm-position report from
just before it: there is no longer any position or rotation logic left for that report to have been about.

## Open items (regenerated 2026-09-30; the ICF GAME-score item above is long resolved)
- **Still queued (operator, 2026-09-30; prerequisite now satisfied): rename `.github/workflows/ci.yml`'s
  `name:` field from `CI` to `carrom arena verification`.** The blocking condition - the web build spike
  either merged into `release.yml` for real or discarded - is now done (`856ac6e`: folded into `ci.yml`'s
  own verify-only "web" job and `release.yml`'s build+publish "web" job; `web-build-spike.yml` deleted).
  Nobody has asked for the rename itself yet; do it whenever asked, not before.

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

## Cleanup across all three locations (2026-09-28)
Asked to clear temporary build/test/temp clutter everywhere: Linux `/tmp` fully wiped (old Xvfb/smoke-test debris,
confirmed nothing was in active use first), the project's own regenerable `out/` build directory removed (`deps/`,
the FetchContent cache, deliberately left alone - a re-fetch had already shown transient network flakiness this
session). A stray `nul` file on the H: side (Windows redirect-mishap debris, contents were just SSH host public
keys) deleted, along with a `traces/` folder that had wandered into the exe-staging area where it doesn't belong.
Everything kimi-era was removed except `~/.kimi-code` itself, which the operator was explicit must stay: a 405 MB
stale manual clone with `CEO_APPROVAL.md` and similar artefacts still in it (its deletion was first refused outright
by the coding assistant's own auto-mode safety layer as an irreversible bulk delete; the operator ran it themselves),
`kimi-code-src`, `kimi-upgrade.sh`, H:'s `linux_support/` directory, and the formal kimi-era "delivery agreement"
document. Dead branches on GitHub were also pruned down to just `main`.

## Release workflow, and a real cross-platform screenshot gallery (2026-09-28 to 2026-09-29)
A new `.github/workflows/release.yml`, separate from the ordinary push/PR `ci.yml`, added on request: given an
existing tag, it builds and zips the six canonical runners' executables, publishes them to a GitHub Release, then
sanity-checks the published assets by downloading and re-unzipping them. Alongside that, a much wider screenshot-only
matrix - the actual current catalogue of GitHub-hosted runner images across Ubuntu/Windows/macOS, landing on 21
labels once asked to reach that count deliberately - runs the game headless on every one of them and uploads
whatever it captures, purely as proof-of-life for the README, never gating the release itself. Getting the workflow
itself right took several rounds: Windows has no `zip` binary in its git-bash (`Compress-Archive` instead);
re-publishing to an already-released tag failed until publishing learned to update rather than only ever create; and
two runs fired close together raced each other's own `git push` from the screenshot-commit step, fixed with an
`admit` job that rejects a second concurrent run outright (same queue-ticket idea as `ci.yml`'s gate, adapted to
fail-fast rather than queue, since queuing still let the two collide).

**Windows screenshot fix.** Every Windows screenshot came back showing the CI agent's own console or (on the arm64
images) Windows's own first-boot setup screen, never the game - these headless runners have no real GPU driver, and
raylib's OpenGL context creation was failing with nothing downstream checking for it. Fixed by dropping Mesa's
prebuilt software-OpenGL DLL next to the executable, gated behind `CARROM_CI_SCREENSHOT` so no real player is ever
affected.

**macOS screenshot fix - the real story, and now covered above under "Repo state".** A first theory (a HiDPI window
flag needing an interactive WindowServer session) was wrong; disproving it needed a live debugger, which itself
needed macOS's Developer Mode enabled before it would attach to a non-interactive process at all - not discovered
until several rounds of a debugger that reported a crash but printed no backtrace. Once a real trace was obtained (on
an old-lab Intel Mac reachable directly over SSH, building locally with prebuilt cmake/ninja binaries since pip's
cmake wheel wasn't available for that exact OS/Python combination and was compiling from source), the true fault was
raylib's own: `InitPlatform()` never checked whether `glfwCreateWindow()` had actually succeeded before immediately
using the result. Fixing that surfaced a second, deeper bug behind it (`InitWindow()` not checking `InitPlatform()`'s
own return value either - matching an already-merged upstream fix, PR #4803), and fixing THAT surfaced the true root
cause: GLFW's Cocoa backend unconditionally demands a hardware-accelerated pixel format, with no software-renderer
fallback, so any environment without real GPU-backed OpenGL - the lab Mac (a VirtualBox VM, confirmed by its GPU's
PCI vendor ID) and, unexpectedly, some of GitHub's own real-Apple-Silicon-hardware runners too - failed outright.
All three fixes live in a maintained raylib fork now pinned in `build/pins.txt` (see "Repo state" above for the
commit-level detail). Proven twice before trusting it: a real game window confirmed on the lab Mac's physical screen
by the operator directly, then a genuine in-progress board captured as a screenshot artifact on an actual GitHub-
hosted `macos-15` Apple Silicon runner.

**Two runner images still not fixed, and not by us.** `windows-11-arm` and `windows-11-vs2026-arm` kept producing
Windows's own OOBE screen throughout all of the above. A proper diagnostic (checking the actual session the job runs
in, not just retrying) found zero attached displays reported at all - not an unreachable desktop, no desktop session
to reach - and a search turned up an already-closed GitHub-side issue for exactly this (`actions/runner-images#14677`,
reported 2026-09-03), evidently still mid-rollout to some runner pools weeks later. Dropped from the screenshot
matrix; see "Open items" above.

**Tagged `beta-0.0.15`, then `beta-0.0.16`.** The first tag went out once the release workflow itself looked ready;
testing against it then showed the Windows/macOS fixes above were still sitting on the branch, not yet in any tagged
commit, so a second tag followed once they had actually landed. Full pipeline against `beta-0.0.16`: all six platform
zips built, published, verified; sixteen of eighteen attempted screenshot runners produced real, distinct in-progress
boards (every Linux distribution/architecture tried, every macOS version/architecture including a preview Xcode
image, three of five Windows images), each given its own random seed so no two screenshots match - the fullest
cross-platform proof this project has had. Screenshots live in `docs/screenshots/`, named `screenshot-<runner>.png`.

**README rewritten** (2026-09-29): the long-standing formal, kimi-era document (stale `--mode` flags, CEO-delivery
language) replaced with a short one for players - what the game is, a direct link to the latest GitHub Release, the
handful of spectator keys, and the sixteen-runner screenshot gallery, each image labelled with its OS and CPU
architecture. Verified by loading the actual rendered page on GitHub, not just the markdown source.

## A real arena-kickoff coin toss, a scoreboard colour indicator, and a moved tag (2026-09-30)
Three small, separate operator requests after the games-counter fix above (`a9623ce`/`a856230`):

**The first breaker is now a genuine coin toss** (`ad6b792`). Every match previously had north deterministically
break (and hold white for) board 1 - no randomness at all, discovered while explaining the breaker-rotation formula
to the operator. Real carrom decides this with a toss, and the codebase already had a documented "no toss" simplifying
assumption elsewhere (the eight-board tie-break), so this was agreed as a genuine fix, not scope creep.
`match_randomize_first_breaker()` draws one value 0..3 from a brand new, dedicated `RNGContext.match_coin` stream -
kept entirely separate from `global`/the four per-seat streams so drawing it can never perturb formation rotation or
AI shot planning - and stores it as `MatchState.break_offset`, which shifts the existing N-E-S-W rotation formula
(`match_start_board`) by that many seats. It is strictly opt-in: `match_state_init()` alone still leaves
`break_offset` at 0, so every pre-existing test kept passing unchanged with zero updates needed. Wired into
`app_init_match` (covers interactive play AND the soak/selfplay batch loop) and the seamless next-match continuation
path in `app_resolve_shot`. New test `test_arena_kickoff_coin_toss_can_pick_any_seat_to_break_first`
(`test_colour_rotation.c`): draws 40 seeds, asserts all 4 seats get picked at least once, and asserts the toss never
changes board 1's piece layout versus an un-tossed match with the same seed (proving the new stream truly is
isolated from the formation-rotation draw).

**The scoreboard now shows which colour each pair currently holds** (`c012aa1`). Directly prompted by the operator
watching a board where "blue" (east/west) visibly cleared their own coins while the queen sat uncovered on the
board - ICF Rule 107a then correctly credited the win to the OPPONENT (red/north-south) instead, which looked like a
bug from the scoreboard alone (it wasn't - see below). Since colour rotates board to board (ICF 43/49a-i) with no
on-screen indicator of who currently holds what, a small coin - same palette as the real board pieces, off-white
with a dark rim or charcoal with a light rim - now sits right after each row's `P:000` points, white or black
per `renderer_set_scoreboard`'s new `seats_swapped` argument. Verified visually: an Xvfb + `import -window root`
screenshot of a fresh board 1 shows red's coin white and blue's coin black, correctly spaced clear of the title bar,
matching the real coins' colours exactly. `renderer.h`'s stale comment claiming "red = the white team, blue = the
black team" (a fixed colour mapping) is also corrected - red/blue are fixed to north-south/east-west seats, never to
a colour.

**Root-cause investigation for the operator's original bug report**, which prompted both features above: read
`traces/trace.jsonl` (the ring buffer's own reconstruction logic, `write_offset`/`wrapped` header, matching
`trace_read_last_records()` exactly) to find the actual board where "blue seemed to win but red's board count went
up." Traced it precisely: west (blue) pocketed the queen alone, then missed their one chance to cover her, so she
returned to the board's centre untouched by either side; east (blue) later cleared their own last remaining coin,
but because the queen was sitting unclaimed at that exact moment, ICF Rule 107a awards the board to the OPPONENT
pair - crediting red (north/south), not blue, exactly as the (already-correct, separately-tested) rules engine
recorded. Not a bug; the new scoreboard coin above exists specifically so this kind of moment reads correctly to a
viewer without needing to reconstruct a trace file to explain it.

**Tag moved** (operator's explicit instruction - the standing rule is otherwise "never move existing tags"):
`BEFORE-WEB-ADDITION` re-pointed from `692c902` (its original commit, the RESUME.md update through `beta-0.0.16`) to
`c012aa1` (current head), force-pushed to GitHub, and re-fetched on the H: clone. `git tag -n99 BEFORE-WEB-ADDITION`
confirms the same annotation message; `git log -1 --oneline BEFORE-WEB-ADDITION` confirms the new target on all
three locations.

## A README touchup and the `beta-0.0.17` release (2026-09-30, later)
Two small operator-requested items, handled back to back.

**README duplicate attribution line removed** (`8ac691a`). The line `*Based on original work by Supratim Sanyal of
SANYALnet Labs.*` appeared twice - once right under the `# Carrom Arena` title, once again (correctly) in the License
section near the bottom, as part of the attribution clause itself. The top-of-file copy was redundant and is gone;
the License-section copy is untouched.

**`beta-0.0.17` cut and released.** Tagged on `8ac691a` (the README fix above being the only change since
`beta-0.0.16`'s four-commit run), pushed, then `.github/workflows/release.yml` dispatched by hand via
`gh workflow run release.yml -f tag=beta-0.0.17` (tag pushes still don't auto-trigger it). All six canonical-runner
builds and the `publish` job succeeded; `gh release view beta-0.0.17` confirms all six zips attached
(`ubuntu-24.04`, `ubuntu-24.04-arm`, `windows-2022`, `windows-11-arm`, `macos-15`, `macos-15-intel`), published,
not a draft. The wider screenshot-only matrix and final `verify` sanity job are non-gating and run independently of
whether the release itself is good.

**Raylib-fork PR question, answered without any code change:** the operator asked why no "open a pull request"
option shows up for the `tuklusan/raylib` fork's corrections. Answer (recorded in Repo state above): both real fixes
are already merged upstream (raysan5/raylib PR #4803 and #4804, March 2025); the third commit is explicitly local-only
CI test code. Nothing to upstream currently. Also clarified that GitHub's "Sync fork" only fast-forwards the fork's
default branch (`master`), never `carrom-arena-fixes`, and the build pins by commit SHA regardless - so syncing the
fork is always safe.

## Web build: from a discardable spike to a first-class release deliverable (2026-09-30)
A temporary `.github/workflows/web-build-spike.yml` (`f9e2e7d`) proved the existing C17/raylib/Box2D codebase
would actually run under Emscripten before committing to it for real: five small CMake accommodations (GNU C
extensions for raylib's miniaudio EM_ASM glue, an empty `PLATFORM_LIBS` branch for Emscripten, a genuine
upstream Box2D v3.1.0 CMake bug patched via `string(REPLACE ...)`, `.html`-output plus ASYNCIFY/
ALLOW_MEMORY_GROWTH/EXIT_RUNTIME link flags so the existing blocking game loop runs unmodified, and CMake's
`"SHELL:"` prefix to stop repeated bare `-s` flags silently collapsing into one). Once proven, folded for real
(`856ac6e`) into `ci.yml` (a verify-only "web" job, every push, never touching `gh-pages`) and `release.yml`
(a build-and-publish "web" job, a first-class deliverable alongside the six native zips - a broken web build
now fails the release the same as a broken native one); the spike workflow deleted, nothing it did lost.
`build(ci): publish .nojekyll alongside the web build` (`269c614`) fixed GitHub Pages' legacy Jekyll builder
choking on Emscripten's own generated JS.

## Two audio bugs, one after the other, both on the web build (2026-09-30)
First symptom (operator): the deployed page needed a click before any sound, and the phone build specifically
stayed silent even after tapping "Tap to start." Root cause: `main()` - and `InitAudioDevice()` inside it - ran
the instant the wasm module loaded, before any tap; the tap was cosmetic. Fixed with `Module.noInitialRun` plus
a deferred `Module.callMain()` inside the tap handler, so the game only starts inside a genuine user gesture
(`EXPORTED_RUNTIME_METHODS=[callMain]` needed to expose `callMain` to JS at all). Tagged `ALL-GOOD-EXCEPT-
BROWSER-AUDIO` at this point - accurate at the time, superseded below.

Second, deeper bug (`72ac1a7`): the operator reported it was STILL silent, on desktop too, where the gesture-
gating never even applied. Found by reading the browser console instead of guessing again: every audio callback
threw `Cannot read properties of undefined (reading 'buffer')` inside miniaudio's own Emscripten glue, which
reads `Module.HEAPF32.buffer` directly. `EXPORTED_RUNTIME_METHODS` is an allowlist, not additive to
Emscripten's own defaults - adding `callMain` for the click-to-play fix had silently dropped `HEAPF32` along
with everything else normally exposed. Fixed: `EXPORTED_RUNTIME_METHODS=[callMain,HEAPF32]`. Confirmed
properly this time via `window.miniaudio.devices[0].webaudio.state` reading "running" with `currentTime`
visibly advancing, not just "no more console errors."

## Real AH.FM radio in the browser, and click-to-play goes universal (2026-09-30)
Initially shipped with `--no-radio` on web, on the assumption the native pipeline (a raw HTTP socket feeding a
hand-rolled minimp3 decoder) has no browser equivalent - true, but the operator asked for it to work "exactly
like the desktop user experiences." Solved differently (`f1a5276`): a plain HTML5 `<audio>` element pointed at
the same AH.FM mirror URLs streams and decodes a live MP3 itself, confirmed to need no CORS headers for plain
playback. New `audio/radio_web.c` (compiled in place of `radio.c` on Emscripten only; `radio_stream.c`/
`radio_mp3.c` excluded from that build entirely), implementing the existing `radio.h` contract via `EM_JS`-
defined JS helpers, backed by a hidden `<audio>` element with mirror fallback on error. Tagged `RADIO-STREAM-OK`.

Separately (`f7378d2`), the original phone-only click-to-play gating (`max-width:430px` media query) turned out
to be solving the wrong problem for the wrong platform - every browser enforces the same gesture requirement,
not just phones. Simplified to one unconditional gate; overlay text now three centred lines matching the
in-game title bar. Tagged `ALL-GOOD-BUT-NO-WEB-RADIO-STREAM` right before the radio fix above landed.

## Font swap, and four rounds of background visual correction (2026-09-30)
Sono Medium (SIL OFL 1.1) replaced raylib's default bitmap font across every `DrawText`/`MeasureText` call site
in `renderer.c` (`4554773`), with a `text_spacing()` helper reproducing `DrawText`'s own spacing formula so the
font swap didn't also change letter-spacing; the scoreboard's fixed-width cell math already derived its width
from the font's own "W" advance, so digit alignment kept working with no extra changes.

The Tron background went through several operator-driven rounds the same day (`62af570`, `cd41747`, `4daecd3`):
doubled scroll speed and halved radio volume together; then reversing direction "frequently" (replaced a
steady-drift-plus-wobble formula, which could dip negative but never really read as reversing, with a pure sum
of three incommensurate sine terms and no net drift - checked numerically, not just eyeballed: a reversal every
~8.6s on average over a 120s span); then stars and sparser distant galaxies added, first orbiting the vanishing
point - which the operator correctly called out as not reading as motion ("cannot be static...like travelling
through space") - redesigned to genuinely travel outward from the vanishing point at a per-object speed
proportional to closeness (`u = fmod(phase + t*speed, 1.0)`, `depth = u*u`, `radius = depth*max_r`), wrapping
via fmod; finally the spoke rotation's own time base slowed to 25% (`rot_t = t * 0.25f`) while the travel rate
stayed exactly fixed, pulled into one named constant (`WORMHOLE_TRAVEL_RATE = 0.48f`) specifically so rotation
and travel speed can't drift out of sync again after this many rounds of tuning.

## README Controls-table validation surfaces two real bugs, neither about the table (2026-09-30)
Asked to confirm Space/+/-/M/R/Q/Esc all actually worked (`15ec674`) - R ("Restart with a new seed") and Q
("Quit") were pure fiction, zero code anywhere, confirmed by grep; Esc worked only via raylib's own default
exit key. Implemented both: R reseeds the RNG and reruns the same controller/match init sequence
`app_run_soak` already uses between seeds; Q sets a flag OR'd into the existing `WindowShouldClose()` check.
A real Debug+ASan build caught an actual memory leak the first time R was pressed twice: `app_init_controllers()`
unconditionally overwrote the controller pointers with no cleanup of the previous ones - fixed by calling the
already-existing `app_cleanup_controllers()` first. A second, unrelated bug surfaced on inspection: M's mute
calls raylib's `SetMasterVolume()`, which never reaches the web radio's independently-created `<audio>`
element - fixed by having web-only `radio_update()` poll `audio_is_muted()` and mirror it onto `.muted`.

## The AI was avoiding the queen, and the reason was in the scoring math (2026-09-30)
Operator report: robots sometimes ignore an easily-pocketable queen in favour of an equally-easy cover piece,
or take the cover first. Root cause (`cf3fdf4`, found by reading `shot_evaluator.c` rather than guessing): an
uncovered queen pocket scored at only 0.3x `weight_queen`. Worked the actual numbers across all four strategy
profiles (`strategy_profiles.h`: `AGGRESSIVE`/`BALANCED`/`DEFENSIVE`/`TRICKSTER`) - a plain coin pocket beat
taking the queen alone in every single profile, by 2-3x, even AGGRESSIVE, whose own comment says it "favors...
queen attempts." The evaluator only looks one shot ahead (a scratch physics rollout per candidate, see the
Carrom Engine section added to README.md below), so "pocket the queen now, cover it next turn" - the normal,
low-risk way this plays out under ICF Rule 49 (pocketing anything earns another shot) - was structurally
invisible to it; it could only compare this shot's reduced queen value against this shot's full-value coin
alternative, and always took the coin. Fix: an uncovered queen is worth the full `weight_queen` now, not a
fraction of it; only the separate cover bonus stays conditional on covering in the same shot. Also fixed in
passing: AGGRESSIVE and TRICKSTER (`weight_queen > weight_pocket` by design) now actually prefer the queen when
available, which the old multiplier silently defeated for every profile including the two built to want it.
`test_shot_evaluator_scoring` had been a literal stub (`TEST_ASSERT_TRUE(true)`) the whole time - replaced with
real coverage proving the fix and guarding the exact regression.

## Rich social-media link previews (2026-09-30)
`16539c3` added Open Graph (read by Facebook, WhatsApp, LinkedIn, Discord, Slack, Telegram, iMessage in one
pass) and Twitter/X Card meta tags to `web/shell.html`, prompted by a request for "the maximum number of
social sharing sites." Crawlers don't execute JS/WASM, so the live canvas can't be screenshotted on the fly -
built a real `web/og-image.png` (1200x630) by cropping the letterboxing out of a clean Xvfb capture (Linux
screenshots are just the app; Windows captures the whole desktop, taskbar included, useless for this) and
centering it on a fill matching the background's own gradient. Verified for real: fetched the live deployed
page's raw HTML and the image's own HTTP headers directly, confirming the exact referenced URL serves a 200
with the right content-type and byte count.

## Release cadence and checkpoint tags, beta-0.0.18 through beta-1.0.0 (2026-09-30)
`beta-0.0.18` (`856ac6e`, the integrated CI/release pipeline, proven on real GitHub infrastructure) through
`beta-0.0.19` (`4554773`, font/click-to-play/HEAPF32), `beta-0.0.20` (`cf3fdf4`, wormhole round one, Q/R/leak/
mute fixes, the AI queen fix), `beta-0.0.21` (`4daecd3`, wormhole rounds two through four), and `beta-1.0.0`
(`16539c3`, social previews) - explicitly the last beta before a full `1.0.0`, per the operator. Each release
ran the full six-native-platform-plus-web pipeline and the wider best-effort screenshot matrix; `docs: refresh
runner screenshots` commits after each tag (`28fc692`, `69530b0`, `91e2f22`, `3d75736`) are the release
workflow's own bot pushes, picked up by fast-forwarding both the Linux clone and the H: clone afterward every
time, never authored directly.

## README overhaul: video, SEO, a Carrom Engine design section, license headers (2026-09-30)
Four separate operator requests, same session, same file mostly:
- **A gameplay video embedded at the top** (`ef31903`), via a GitHub `user-attachments` asset URL (the operator
  uploaded it manually after automation hit a real wall: GitHub's file-attach widget only creates its file
  input after a real click, which pops a native OS file-chooser dialog no browser-automation tool can drive -
  documented here so the next attempt doesn't retry the same approach). The original 105MB source clip was
  re-encoded with ffmpeg (960x540, two-pass ~535kbps video, 64kbps audio) to 8.7MB to clear GitHub's upload
  limit before the operator dropped it in.
- **SEO badges, a keyword-forward opening paragraph, and Download moved right under it** (`ef31903`), plus a
  new "Part of an Ongoing Series" section naming the blog and linking Part 7
  (`supratim-sanyal.blogspot.com/2026/09/fire-ai-software-company-finish-carrom-arena-game.html`).
- **A "Carrom Engine" design section** (`e4279b1`), written to cover high-intent search terms (carrom engine,
  artificial intelligence, machine intelligence, machine learning) under an explicit house rule from this point
  on: spell out "artificial intelligence"/"machine learning" in full, never as bare "AI"/"ML", in this project's
  own prose (quoted external titles are exempt - the blog post title keeps its own real wording verbatim).
  Content is grounded in the real pipeline: candidate generation (`shot_candidates.c`/`geometry_planner.c`,
  ghost-ball aiming and cushion mirror images), a physics rollout per candidate against a scratch Box2D world
  under a 250ms budget (`shot_evaluator.c`), then six-factor utility scoring - explicitly contrasted with
  machine learning (no neural network, no training data, no learned weights, by design). Built via two
  deliberate passes on a disk copy before ever touching the live file: pass one for keyword placement/hook/
  fixing three pre-existing bare "AI" mentions elsewhere in the README for consistency; pass two for heading
  structure, internal links to the three source files, and de-stuffing repeated exact phrases. Verified via a
  scripted grep that zero bare "AI"/"ML" abbreviations survived except the one intentional verbatim quote.
- **A license header added to all 92 project-owned `.c`/`.h` files** under `src/` and `tests/` (`1b52c92`):
  a two-line `/* Copyright (c) 2026 Supratim Sanyal of SANYALnet Labs. Licensed under the SANYALnet Labs
  Non-Commercial License; see LICENSE. */` block as the first lines of every file, before any include guard.
  Deliberately excludes `deps/raylib`, `third_party/minimp3` (vendored code keeps its own upstream license)
  and the three build-generated `.c` files written to the CMake binary dir (never tracked in git). Verified
  with a full 100% (24/24) test pass with the headers already in place, before committing - pure comment
  insertion, zero behavioural change. Tagged `BEFORE-HEADER-COMMENT-UPDATES` at `e4279b1`, the commit
  immediately before this change, on the operator's explicit request.

## The `1.0.0` release: a full fresh rebuild, tagged and shipped (2026-09-30)
Tagged `1.0.0` at `1b52c92` (annotated, `git tag -a`, matching the project's existing tag style - no `v`
prefix, consistent with every `beta-X.Y.Z` tag before it) and dispatched `release.yml` by hand
(`gh workflow run release.yml -f tag=1.0.0`), same as every prior release: a genuinely fresh build on all six
canonical runners (`ubuntu-24.04`/`-arm`, `windows-2022`/`windows-11-arm`, `macos-15`/`macos-15-intel`) plus
the WebAssembly target, watched to completion (`gh run watch --exit-status`), all green.

Release notes were written deliberately different from the README rather than restating it: the README
answers "what is this," the release page answers "how do I get it running" - a one-line description, the
zero-install web link first, then per-platform download-and-run steps including the real first-run friction
(Windows SmartScreen, macOS Gatekeeper's right-click-Open dance, both accurate to the actual unsigned
binaries - confirmed by grep: no codesign/signtool/notarization step exists anywhere in the build), a pointer
to the blog for the full story, and a changelog diff link. Published via `gh release edit` after letting the
workflow create the release normally with its default placeholder notes, rather than pre-empting it.

Verified, not assumed: downloaded and played the live web build in-browser (board renders, robots, wormhole
background, zero console errors); downloaded the Windows x64 zip, extracted it, launched the real exe, and
confirmed it survived 10 seconds of runtime without crashing before closing it. Both `build_fresh` directories
(the repo's own and the parent folder's) had the stale `beta-1.0.0` exe removed and the fresh `1.0.0` one
copied in. The release workflow's own screenshot-refresh bot pushed a follow-up commit straight to `main`
again afterward (`e16260e`, same pattern as every prior release) - picked up by fast-forwarding the Linux
clone and the H: clone immediately after, same as always.
