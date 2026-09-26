# build/: tracked build inputs and the build script

This directory is tracked. Build OUTPUT is not: it goes to `out/` (and `build_*/`), which are git-ignored.

| File | Purpose |
|------|---------|
| `build.py` | The build driver. CI runs exactly this, and you can run it by hand (see below). |
| `pins.txt` | Pinned dependencies (raylib, Box2D, Unity): name, git url, exact commit sha, release tag. `CMakeLists.txt` reads it too. |
| `tools.txt` | Pinned CMake and Ninja versions, installed by `build.py` with pip into `out/.tools`. |

Manual build (Python 3.8+ and git are the only prerequisites; needs a C compiler: gcc or clang, MinGW-w64 gcc on Windows):

```bash
python build/build.py                        # tools, packages, deps, configure, build, test (Debug) into out/
python build/build.py --build-type Release --no-test
python build/build.py --out /tmp/carrom-out --no-tools      # use the cmake and ninja already installed
python build/build.py fetch                  # only pre-fetch the pinned dependencies into deps/
```

`.github/workflows/ci.yml` only chooses runners and calls `build/build.py`. The runner list (`RUNNERS` in `build.py`) is the smallest set that covers
every hosted architecture: Linux x64 and arm64, Windows x64 and arm64, macOS arm64 and Intel.

Queue rule: per runner kind at most one job runs (a GitHub concurrency group `carrom-<runner>`) and at most one waits. The `admit` job
(`build.py admit`) rejects a run that would become the second waiting job for a kind, and that run fails. The built game is uploaded by CI as the
artifact `carrom-arena-<runner>` (the Windows exe comes from `windows-2022`).
