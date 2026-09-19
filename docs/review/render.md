# Render Review Note

## Coverage Table

| File | Total Lines | Lines Reviewed | Chunk Ranges | Status |
| :--- | :--- | :--- | :--- | :--- |
| `src/render/board_view.c` | 614 | 614 | 1-250, 251-500, 501-614 | Done |
| `src/render/effects.c` | 129 | 129 | 1-129 | Done |
| `src/render/hud.c` | 92 | 92 | 1-92 | Done |
| `src/render/renderer.c` | 447 | 447 | 1-250, 251-447 | Done |
| `src/render/board_view.h` | 17 | 17 | 1-17 | Done |
| `src/render/effects.h` | 18 | 18 | 1-18 | Done |
| `src/render/hud.h` | 17 | 17 | 1-17 | Done |
| `src/render/renderer.h` | 74 | 74 | 1-74 | Done |

## Findings

### RND-01: Redundant HUD Logic duplication
- **Severity**: Low
- **File**: `src/render/renderer.c:126-190` vs `src/render/hud.c:18-92`
- **Defect Class**: Code Duplication / Maintenance Risk
- **Evidence**: `renderer.c` contains a private function `draw_hud_sidebar` which is almost an exact replica of `hud_draw` in `hud.c`.
- **Recommended Fix**: Remove `draw_hud_sidebar` from `renderer.c` and call `hud_draw` instead.

### RND-02: Inconsistent Angle Formatting Logic
- **Severity**: Low
- **File**: `src/render/renderer.c:115-123` vs `src/render/hud.c:8-16`
- **Defect Class**: Code Duplication
- **Evidence**: `format_angle_deg_min` is implemented identically in both files.
- **Recommended Fix**: Move `format_angle_deg_min` to a common utility header or just `hud.c`.

### RND-03: Screen Coordinate Casting Precision Loss
- **Severity**: Low
- **File**: `src/render/board_view.c:158, 160-161, 164-167`
- **Defect Class**: Rendering Artifacts
- **Evidence**: `DrawEllipse((int)shoulders_center.x, (int)shoulders_center.y, ...)`
- **Recommended Fix**: While Raylib's basic Draw functions take `int`, using `DrawEllipseV` or `DrawCircleV` with `Vector2` would avoid premature truncation and jitter during slow movements.

### RND-04: Per-frame Layout Recomputation Waste
- **Severity**: Low
- **File**: `src/render/renderer.c:345`
- **Defect Class**: Performance / Waste
- **Evidence**: `layout_compute(sw, sh, &r->current_layout);` is called every single frame.
- **Recommended Fix**: Only call `layout_compute` when `sw` or `sh` actually changes (which is already checked for the capture texture at lines 337-342).

### RND-05: Floating Point Precision in Aim Line calculation
- **Severity**: Low
- **File**: `src/render/board_view.c:233`
- **Defect Class**: Layout Math
- **Evidence**: `float clamped_len = my_fminf(natural_len, boundary_dist - 0.01f);`
- **Recommended Fix**: Ensure `0.01f` is consistent with the world-to-screen scale to avoid gaps or overlaps on different resolutions.

