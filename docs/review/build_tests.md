# Build & Tests Review Note

**Reviewer:** Programmer 7 (Tests/Build Reviewer)
**Date:** 2026-09-19
**Scope:** `tests/*`, `CMakeLists.txt`, `.github/workflows/ci.yml`, `scripts/*`

## Coverage Table

| File | Total Lines | Lines Reviewed | Chunk Ranges | Status |
| :--- | :---: | :---: | :---: | :---: |
| `tests/CMakeLists.txt` | 196 | 196 | 1-196 | Completed |
| `tests/test_ai.c` | 234 | 234 | 1-234 | Completed |
| `tests/test_capture.c` | 114 | 114 | 1-114 | Completed |
| `tests/test_integration.c` | 152 | 152 | 1-152 | Completed |
| `tests/test_physics.c` | 308 | 308 | 1-308 | Completed |
| `tests/test_regression.c` | 496 | 496 | 1-496 | Completed |
| `tests/test_rules.c` | 359 | 359 | 1-359 | Completed |
| `tests/test_trace_circular.c` | 415 | 415 | 1-415 | Completed |
| `CMakeLists.txt` | 167 | 167 | 1-167 | Completed |
| `src/CMakeLists.txt` | 231 | 231 | 1-231 | Completed |
| `.github/workflows/ci.yml` | 122 | 122 | 1-122 | Completed |
| `scripts/clean_build.sh` | 21 | 21 | 1-21 | Completed |
| `scripts/kill-all-runs.sh` | 131 | 131 | 1-131 | Completed |
| `scripts/soak_verification.sh` | 176 | 176 | 1-176 | Completed |
| `scripts/git-hooks/pre-push` | 21 | 21 | 1-21 | Completed |
| **Total** | **3111** | **3111** | - | **100%** |

## Findings

### [ID-01] Low | `tests/test_ai.c:184` | Test Stub
- **Defect Class:** Gap in Test Coverage
- **Evidence:** `TEST_ASSERT_TRUE(true); // This is a stub test` in `test_shot_evaluator_scoring`.
- **Recommended Fix:** Implement a mock board state and evaluate specific shot scenarios to verify scoring logic.

### [ID-02] Medium | `tests/test_physics.c:184` | Shallow Verification
- **Defect Class:** False-passing / Insufficient Verification
- **Evidence:** `TEST_ASSERT_TRUE(true); // Just ensure it doesn't crash` in `test_physics_pocket_capture`.
- **Recommended Fix:** Implement a test that places a piece near a pocket, applies a force, and verifies `physics_collect_pocketed` identifies it.

### [ID-03] Low | `tests/test_integration.c:19` | Removed Tests
- **Defect Class:** Gap in Test Coverage
- **Evidence:** `test_app_config_default` and `test_app_parse_args_help` are stubs.
- **Recommended Fix:** Move these tests to a separate target that doesn't link against raylib, or mock the CLI/Config interface.

### [ID-04] Low | `.github/workflows/ci.yml:65` | CI Infrastructure Gap
- **Defect Class:** Portability / CI Hole
- **Evidence:** `Skip capture test on Windows CI until GPU runners are available.` (Note in `test_capture.c:65`).
- **Recommended Fix:** Investigate software OpenGL/Mesa for Windows runners or use a dedicated GPU-enabled runner to verify capture mode on Windows.

## Summary
The build system is robust and the test suite covers core logic well. The primary issues are "stub" tests in the AI and Physics modules that provide no actual verification beyond "doesn't crash". Determinism and regression boundaries (per-shot step limits) are well-implemented in `test_regression.c`.
