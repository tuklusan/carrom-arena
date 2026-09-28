# Carrom Arena: RESUME playbook

**Updated:** 2026-09-28 (UTC). Development is direct and hands-on: the kimi "software company" was fired on 2026-09-24. Nothing is running for carrom. Do not relaunch kimi or use `~/bin/relaunch.sh` / `~/bin/watchdog.sh` unless the operator asks. The unrelated ZX-UX project on the same Linux box must not be touched.

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
4. Look at the real game: run `carrom_arena --mode=rendered` on Xvfb via a SCRIPT FILE (never inline in an ssh command: `scripts/kill-all-runs.sh` kills any process whose command line contains the binary name, including your own shell), screenshot with `import -window root`, and Read the PNGs. (2026-09-27: `--mode=capture` is REMOVED - this Xvfb + `import` approach fully superseded it; see below.)
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
- Scoreboard (2026-09-27, reworked): each side shows `G:00 B:00 P:000` (fixed font, every character the same cell width; out of range wraps to 0).
  G = games won this MATCH (`total_games`, unchanged from before). B = boards won THIS GAME. P = points THIS GAME (the coin tally: coins of
  the pair's colour pocketed, +3 for a covered queen; moves the instant a coin drops or returns). B and P both reset to 0 when a new game starts.
- IMPORTANT for B (and P): ICF 43 gives the BREAKER's pair the white coins for THAT BOARD ONLY, and ICF 49a(i) has the break (and so the
  colour) rotate to the other pair every board within a game (confirmed by `test_colour_rotation.c`). A board win or a coin is therefore
  recorded by the rules engine under `boards_won_white`/`black` or `PIECE_WHITE`/`BLACK`, colours that can belong to EITHER physical pair
  from one board to the next. B is computed by resolving each board's own `seats_swapped` at the moment it is decided
  (`app_resolve_shot`'s `old_seats_swapped`, captured before the rules engine runs) and adding that board's win to the correct physical
  pair (`ctx->pair_boards_won[0/1]`); P already did this per board (`scoring_live_board_points`). Verified: in an instrumented run where the
  same PAIR won four boards in a row while the winning COLOUR bucket alternated white,black,white,black, B correctly read 4 for that pair
  and 0 for the other; a naive direct `boards_won_white -> red` mapping would have shown 2 and 2 - wrong.
- OPEN ICF-COMPLIANCE QUESTION (found while doing the above, not fixed): `GameState.scores.white/black`, which decides who wins a GAME
  (25 points or ahead after 8 boards, ICF 56a) and increments `match.games_won_white/black`, is accumulated by COLOUR across the whole game,
  the SAME colour bucket B was just found to misattribute. Since colour genuinely belongs to a different physical pair board to board
  (same ICF 43/49a-i basis as above), and ICF 52-56 speak of "the player"'s/pair's points, not a colour's, `game.scores.white` most likely
  MIXES the two physical pairs' points across a game's boards, so the GAME winner (and hence G) could be wrong whenever the breaker rotation
  matters, i.e. essentially always once a game runs more than one board. This is a pre-existing property of the rules engine (not introduced
  today) and fixing it is a rules-engine change (touches `finish_board`, `match.games_won_*`, and the tests that assert on them), which is
  bigger than a display change: NOT done without the operator's decision, given "ICF compliance is non-negotiable".

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

## Open items (regenerated 2026-09-26, after the arrange/rotation/lock work)
Decisions for the operator:
- Should the ICF GAME score (which decides G) be reattributed by physical pair the same way B and P now are (see the compliance
  question above)? This can change who wins games/matches once a game runs more than one board.
- Confirm on real hardware that the Release exe (`-release`, the first optimised build ever shipped) plays like the old Debug ones; then decide whether Release is the only exe to hand out.
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
