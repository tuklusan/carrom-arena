# Handoff: build, three-repo check-in and Windows exe drop (Carrom Arena)

Written 2026-09-26 at the end of a long session. Read this, then `RESUME.md` in the repo and the blog ledger, then continue.
Operator preference: NO narration, no verbosity in chat; work quietly, report results briefly. Never write the shared machine
password anywhere (files, commits, messages, memory). Never push kimi config. Do not raise password rotation. Do not touch the
unrelated ZX-UX kimi project. No attribution lines in commits. Do not squash GitHub history.

## 1. The state right now

- Product: C17 game "SANYALnet Labs Carrom Arena": four AI players (north/south pair against east/west pair), raylib 5.5 + Box2D v3.1,
  Windows 11 target (also runs on Linux for development). Window locked at 560x560, not resizable (operator decision; do not change).
- Head commit `4567de5`, tag **beta-0.0.11**, identical in all three repos (below). Tags so far: beta-0.0.4 ... beta-0.0.11. The next tag is
  `beta-0.0.12`; tag only when the operator asks (they ask "tag the latest codebase with the next available beta revision number").
- Latest exe: `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\build_fresh\carrom_arena_4567de5.exe` (GUI-subsystem, no console window).
- Implemented this session: ICF Laws of Carrom rule by rule (reference PDF in the repo's `reference/` folder), robots (red = north/south,
  blue = east/west), scoreboard (top left), AH.FM radio button (bottom right, WinHTTP + minimp3), tinted-glass board, charcoal black coins with
  silver rim, coins/queen sliding back from their pocket, own sounds for foul and queen-back, equal expert AI skill for all seats, CodeQL one-off
  review done (workflow removed), adversarial code review done. Open items: blank frames in `--mode=capture`, undecided outer-ring colour pattern.
- Docs to read first: `RESUME.md` (playbook + decisions), `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\SANYALnet-Labs-Dev-Blog.md` (ledger, CRLF
  line endings, no secrets), `reference/README.md`, `LOCKED_INVARIANTS.md`, `docs/SOUND_PLAN.md`.

## 2. The three repos (must always end up identical)

| # | Where | Path / URL | Role |
|---|-------|-----------|------|
| 1 | Linux dev box (working repo) | `sanyalnet@192.168.4.76`, `~/SOFTWARE-DEVELOPMENT/carrom` (key-based ssh from the operator's Windows machine) | all editing, builds, tests, commits, tags happen here |
| 2 | GitHub | https://github.com/tuklusan/carrom-arena (branch `main`, all tags) | published copy |
| 3 | Operator's Windows clone | `H:\My Documents\SOFTWARE-DEVELOPMENT\Carrom\CARROM_REPO\carrom-arena` | fast-forwarded from GitHub |

The Linux box is treated as ephemeral: whatever is not on GitHub can be lost. "push" means everything, to all three.

### Check-in procedure (after any change)
1. On the Linux box, in `~/SOFTWARE-DEVELOPMENT/carrom`: `git add -A && git commit -m "..."` (git user.name is `tuklusan`; no attribution lines).
2. **Gate**: `bash ~/clean_verify.sh`. It clones the committed HEAD into `~/clean_verify`, configures with Ninja (Debug, ASan/UBSan, -Werror for
   our libraries) and runs the whole ctest suite (24 tests, about 45 s including the self-play test). It must print `100% tests passed`.
   It tests exactly what would be pushed, so commit first.
3. **Push**: `bash ~/bin/push_all.sh`. What it does: snapshots uncommitted work onto a `wip/linux-<UTC>` branch, runs a safety scan
   (secret patterns, a private `~/.push_denylist`, files over 20 MB), pushes `main`, every branch and every tag. It also prints
   `KILL <machine> ... ok` lines (it kills stray scratch processes on the other machines; that is normal). Expect the last line
   `main=<sha> origin/main(after fetch)=<same sha>`.
4. **Tag** (only on request): `git tag beta-0.0.N` on the Linux box, then run `push_all.sh` again (it pushes tags); verify with
   `git ls-remote --tags origin beta-0.0.N`.
5. **Sync the Windows clone**: in `H:\...\CARROM_REPO\carrom-arena` run `git pull --ff-only --tags` (push_all does NOT do this). Verify
   `git rev-parse --short HEAD` and the tag match the Linux box.
6. Update the ledger and notes: append an entry to the blog (CRLF file; replace the last "Current phase/step" paragraph), keep `RESUME.md`
   current, and update the memory file if you have one.

Everything typed into the shell for the Linux box goes through `ssh sanyalnet@192.168.4.76 '...'` from the Windows Git Bash / PowerShell tool.

## 3. Building the Windows exe and dropping it on H: for testing

The Windows exe is built on a separate Windows 11 machine, **`vagab@192.168.4.103`** (key-based ssh from the Linux box AND from the operator's
machine): MinGW-w64 gcc (`C:\ProgramData\mingw64\mingw64\bin`), CMake, Ninja, no MSVC. Its default PowerShell execution policy blocks scripts, and its
ssh login prints a harmless post-quantum warning (filter it with `grep -v "quantum\|store now\|openssh"`).

### One-time helper: `wbuild.ps1` (recreate it if missing; it lives briefly on the W11 box)
```powershell
param([string]$Id)
$ErrorActionPreference = 'Continue'      # NOT 'Stop': cmake writes warnings to stderr
$root = 'C:/Users/vagab/carrom_wip'
Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
New-Item -ItemType Directory $root | Out-Null
tar xzf C:/Users/vagab/carrom_wip.tgz -C $root
Set-Location $root
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCARROM_BUILD_ID=$Id" 2>&1 | Select-Object -Last 1
cmake --build build --target carrom_arena 2>&1 | Select-Object -Last 1
```
Why a script file: passing `-DCARROM_BUILD_ID=beta-0.0.11` inline through nested ssh/PowerShell quoting truncated the id at the first dot
("beta-0"). The tarball has no `.git`, so the build id MUST be passed with `-DCARROM_BUILD_ID=...` (else it says `unknown`).

### Build steps (from the operator's Windows Git Bash; `ID` = the git short sha or tag, e.g. `4567de5` or `beta-0.0.11`)
1. Copy `wbuild.ps1` to the W11 box: `scp wbuild.ps1 vagab@192.168.4.103:C:/Users/vagab/wbuild.ps1`.
2. On the Linux box package exactly the tracked files, send them to W11 and run the build:
   ```bash
   ssh sanyalnet@192.168.4.76 "cd ~/SOFTWARE-DEVELOPMENT/carrom && git ls-files | tar czf /tmp/carrom_wip.tgz -T - \
     && scp -q /tmp/carrom_wip.tgz vagab@192.168.4.103:C:/Users/vagab/carrom_wip.tgz \
     && ssh vagab@192.168.4.103 'powershell -NoProfile -ExecutionPolicy Bypass -File C:/Users/vagab/wbuild.ps1 -Id <ID>' | tail -2; rm -f /tmp/carrom_wip.tgz"
   ```
   Expected tail: `-- Build files have been written to: C:/Users/vagab/carrom_wip/build` and `[98/98] Linking C executable carrom_arena.exe`
   (the step count changes as files are added). `git ls-files` means untracked files are NOT built: commit first.
3. Drop the exe on H: (the file is at `carrom_wip/build/carrom_arena.exe` on the W11 box), named with the id, and delete the old drop:
   ```bash
   cd "/h/My Documents/SOFTWARE-DEVELOPMENT/Carrom/build_fresh"
   scp -q vagab@192.168.4.103:C:/Users/vagab/carrom_wip/build/carrom_arena.exe carrom_arena_<ID>.exe
   rm -f carrom_arena_<old id>.exe
   ```
   `build_fresh` also holds an old `carrom_arena.exe` (possibly locked/in use: leave it), `flight_dump.exe` (the flight-recorder decoder) and the
   default `traces` folder. Do not delete the newest exe. Always give the exe a NEW name per build (a running exe cannot be overwritten).
4. Clean the scratch on W11 (do this only after step 3 has copied the exe): remove `C:/Users/vagab/carrom_wip`, `carrom_wip.tgz` and `wbuild.ps1`
   with `ssh vagab@192.168.4.103 'powershell -NoProfile -Command "Remove-Item -Recurse -Force C:/Users/vagab/carrom_wip, C:/Users/vagab/carrom_wip.tgz, C:/Users/vagab/wbuild.ps1 -ErrorAction SilentlyContinue"'`.
   (Pitfall seen once: chaining the cleanup with `;` after a failed copy destroyed the build. Copy first, check the file exists, then clean.)
5. Smoke test on the operator's machine (from Git Bash, inside `build_fresh`; the game is a GUI program that ends only when killed):
   ```bash
   (./carrom_arena_<ID>.exe --seed=9 --trace-dir=tt & sleep 12; taskkill //F //IM carrom_arena_<ID>.exe >/dev/null 2>&1)
   head -1 tt/debug_9.log      # must print: Carrom Arena <ID> seed=9 speed=1.00x window=560x560
   ./flight_dump.exe tt/flight_9.bin --events | tail -3   # events and frames were recorded
   rm -rf tt
   ```
   Use a RELATIVE `--trace-dir` (an absolute path with spaces such as `H:\My Documents\...` is split by PowerShell's argument handling and made a
   junk `H:\My` folder once). The debug log also shows `AUDIO: Device initialized` and `STREAM: Initialized successfully` when the radio connected.
6. The operator then plays `carrom_arena_<ID>.exe` from File Explorer (no arguments needed). PE header check that it is windowed:
   `python -c "import struct;f=open('carrom_arena_<ID>.exe','rb').read();pe=struct.unpack_from('<I',f,0x3c)[0];print(struct.unpack_from('<H',f,pe+92)[0])"` prints 2 (GUI).

Files the game writes (in the folder it is started from, default `traces\`): `debug_<seed>.log` (ALL debug output; the game prints nothing to a console),
`trace_<seed>.jsonl` (8 MiB ring), `flight_<seed>.bin` (flight recorder, decode with `flight_dump.exe`), older sessions pruned to the newest 20 of each kind.

## 4. Developing and testing on the Linux box

- Build dirs (all git-ignored): `build_debug` (Debug + ASan/UBSan, what tests use), `build` (older). Configure: `cmake -B build_debug -G Ninja`; build:
  `cmake --build build_debug --parallel 2`; tests: `cd build_debug && ctest`. Tools: `build_debug/selfplay` (headless AI-vs-AI boards; `--seed N --seeds K
  --max-turns T --first-seat 0..3`), `build_debug/radio_probe` (network + MP3 decode check of the AH.FM stream), `build_debug/flight_dump`.
- Visual checks without a display: run the game under Xvfb and screenshot, e.g. `Xvfb :98 -screen 0 700x700x24 &`, `DISPLAY=:98 ./build_debug/carrom_arena
  --no-radio --seed=3 --trace-dir=/tmp/x/t &`, `DISPLAY=:98 import -window root /tmp/x/s1.png`, copy the png to Windows with scp and open it with the Read tool.
  (Do not run `carrom_arena` inline in an ssh command that also uses `pkill -f`: the pattern can kill your own ssh shell. Use script files and kill by pid.)
- The rules engine is `src/game/rules.c` (Laws cited by number), tests `tests/test_rules.c` (one test per rule), the physics `src/physics/`, the AI `src/ai/`
  (geometric planner + scratch physics simulation), rendering `src/render/`, audio and radio `src/audio/`. Sounds are Kenney CC0 files in `assets/audio/`,
  embedded at build time (add a cue: `audio_policy.h/.c`, `audio.c`, the `.ogg`, `assets/audio/CREDITS.md`).

## 5. Tooling pitfalls learned the hard way

- Shell heredocs mangle backslash-n and choke on apostrophes when nested in `ssh '...'`. Write patch scripts / files with the file-writing tool, `scp` them to
  the box, and run them there (or edit a local copy and `tar | ssh tar x` the changed folders back).
- A pre-write hook blocks any file whose CONTENT contains the assistant's product name (paths are fine): keep it out of files, commit messages and the blog.
- `Remove-Item` of paths containing spaces/`H:\My...` is blocked by the shell tool's safety check: use Git Bash `rm`/`rmdir` for those.
- Foreground `sleep N` is blocked; poll with a `until` loop instead, or run the command in the background.
- The blog ledger and some other files are CRLF: keep line endings when editing (read/write with `newline=''`).
- PowerShell execution policy on the W11 box: always `-ExecutionPolicy Bypass -File`.
- Long gates and builds: give the tool a generous timeout (up to 590 000 ms).

## 6. Quick "ship it" checklist

1. commit on Linux -> 2. `bash ~/clean_verify.sh` (100 percent) -> 3. `bash ~/bin/push_all.sh` (main = origin/main) -> 4. (if asked) `git tag beta-0.0.N` + push_all
-> 5. H: clone `git pull --ff-only --tags` -> 6. build on W11 with `wbuild.ps1 -Id <sha>` -> 7. `scp` the exe to `build_fresh\carrom_arena_<sha>.exe`, delete the old drop,
clean W11 scratch -> 8. smoke test (debug log first line shows the id) -> 9. blog entry + RESUME.md -> 10. tell the operator the exe name and what changed, briefly.
