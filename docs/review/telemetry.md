# Telemetry Review Report

**Reviewer:** Programmer 6 (Telemetry Reviewer)
**Date:** 2026-09-19
**Scope:** `src/telemetry/*`

## Coverage Table

| File | Total Lines | Lines Reviewed | Chunk Ranges | Status |
| :--- | :--- | :--- | :--- | :--- |
| `src/telemetry/trace.h` | 62 | 62 | 1-62 | Done |
| `src/telemetry/trace.c` | 684 | 684 | 1-250, 251-500, 501-684 | Done |
| `src/telemetry/replay.c` | 148 | 148 | 1-148 | Done |

## Findings

### [TEL-01] Circular Buffer Logic Defect - Wrapping Overwrite
- **Severity:** High
- **Location:** `src/telemetry/trace.c:135-142`
- **Defect Class:** Circular Trace Wraparound Bug
- **Evidence:**
```c
135	    } else {
136	        /* Need to wrap - write from data_start (after index) */
137	        fseek(f, (long)data_start, SEEK_SET);
138	        fwrite(line, 1, line_len, f);
139	        if (!has_newline) fwrite(&newline, 1, 1, f);
140	        w->write_offset = total_len;
141	        w->wrapped = true;
142	    }
```
- **Description:** When the trace wraps, it starts writing from `data_start`. However, if a record was partially written at the end of the buffer, the wrap-around doesn't handle the "split" record or clear the stale data remaining after the new `write_offset`. While JSONL is line-based, the current implementation just jumps to the start, potentially leaving a fragment of a previous record at the end of the file that could be misinterpreted by readers as a valid (but corrupted) record if they don't strictly follow the `write_offset`.
- **Recommended Fix:** When wrapping, ensure the remaining space at the end of the buffer is zeroed out or marked as invalid to prevent partial record corruption.

### [TEL-02] Trace Reading Logical Error - Ignored Wrap-around
- **Severity:** High
- **Location:** `src/telemetry/trace.c:577`
- **Defect Class:** Circular Trace Wraparound Bug
- **Evidence:**
```c
577	    (void)write_offset;
```
- **Description:** In `trace_read_last_records`, the code reads the entire `TRACE_MAX_SIZE` into memory but explicitly ignores `write_offset`. For a wrapped file, the logical order of records is `[write_offset ... TRACE_MAX_SIZE-1]` followed by `[0 ... write_offset-1]`. By scanning from `0` to `read_bytes`, the function reads records in physical order rather than logical temporal order.
- **Recommended Fix:** Use `write_offset` to correctly linearize the circular buffer before extracting the last $N$ records.

### [TEL-03] Potential Buffer Overflow in JSON Generation
- **Severity:** Medium
- **Location:** `src/telemetry/trace.c:175-190`
- **Defect Class:** Memory Bounds
- **Evidence:**
```c
175	    char pockets_json[2048] = "[";
...
190	    strcat(pockets_json, "]");
```
- **Description:** `pockets_json` is a fixed-size buffer. While there is a check `if (written >= (int)remaining) break;` at line 186, the final `strcat(pockets_json, "]")` at line 190 does not check if there is space for the closing bracket if the buffer was exactly filled.
- **Recommended Fix:** Ensure `remaining` accounts for the closing bracket or use `snprintf` for the final assembly.

### [TEL-04] Use of Uninitialized Pointer in Replay
- **Severity:** Medium
- **Location:** `src/telemetry/replay.c:88`
- **Defect Class:** Memory Bounds / Logic
- **Evidence:**
```c
87	        if (!match_validate_shot(&game, &plan)) {
88	            plan = controller_fallback_shot(controller, &snap, &rng.streams[seat]);
89	        }
```
- **Description:** If `match_validate_shot` fails, `controller_fallback_shot` is called. However, `controller` was created at line 81 and used at line 83, but if `controller_create` had failed (though not checked here), this would crash. More importantly, the logic depends on `controller` being valid, but the `controller_destroy(controller)` call at line 84 happens *before* the fallback check at line 88.
- **Recommended Fix:** Move `controller_destroy(controller)` to after the `controller_fallback_shot` call.
