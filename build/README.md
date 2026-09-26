# build/: tracked build inputs

This directory is tracked. Build OUTPUT is not: it goes to `out/` (and `build_*/`), which are git-ignored.

| File | Purpose |
|------|---------|
| `pins.txt` | The pinned dependencies (raylib, Box2D, Unity): name, git url, exact commit sha, release tag. `CMakeLists.txt` reads it, so there is one place to change. |
| `tools.txt` | The pinned CMake and Ninja versions CI installs (`cmake=`, `ninja=`), so every runner uses the same ones. |
| `fetch_deps.sh` | Pre-fetches exactly the pinned commits into `deps/` (depth 1). CMake uses `deps/<name>` when it exists, so nothing is downloaded during configure. CI caches `deps/`. |
| `ci_linux_deps.sh` | The Linux packages the build needs (X11, OpenGL, xvfb), installed without `apt-get update` unless needed. |

Local build:

```bash
bash build/fetch_deps.sh                       # optional: offline-friendly, same commits CMake would fetch
cmake -B out -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out --parallel
ctest --test-dir out
```

CI (`.github/workflows/ci.yml`, `.github/actions/ci-cell`) uses the same files: pinned tools, the `deps/` cache keyed on the hash of `pins.txt`,
and a ccache cache keyed on OS, compiler and build type. Third-party actions are pinned to commit shas. A cache miss only falls back to a full build.
