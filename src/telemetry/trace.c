#include <math.h>
#include "trace.h"
#include "common/types.h"
#include "platform/platform.h"
#include "game/events.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

/* -----------------------------------------------------------------------------
 * Internal Structures
 * --------------------------------------------------------------------------- */

struct TraceWriter {
    PlatformFile* jsonl_file;
    PlatformFile* log_file;
    char jsonl_path[512];
    char log_path[512];
    bool verbose;
    uint64_t seed;
    
    /* Ring buffer state */
    uint64_t write_offset;      /* Current write position (relative to data area start, i.e., after 8-byte index) */
    uint64_t file_size;         /* Current logical file size (data written, max TRACE_MAX_SIZE) */
    bool wrapped;               /* True if we've wrapped at least once */
    
    uint64_t shot_count;
};

/* -----------------------------------------------------------------------------
 * Helper Functions
 * --------------------------------------------------------------------------- */

static bool trace_init_file(TraceWriter* w) {
    /* Open file in read/write binary mode to allow seeking */
    FILE* f = fopen(w->jsonl_path, "r+b");
    if (!f) {
        /* Create new file */
        f = fopen(w->jsonl_path, "w+b");
        if (!f) return false;
        
        /* Write initial index (0) + zero-fill data area */
        uint64_t zero = 0;
        fwrite(&zero, 1, TRACE_INDEX_SIZE, f);
        
        /* Zero-fill the data area (8 MiB) - do in chunks */
        static char zero_buf[4096];
        memset(zero_buf, 0, sizeof(zero_buf));
        for (size_t i = 0; i < TRACE_MAX_SIZE; i += sizeof(zero_buf)) {
            size_t chunk = (i + sizeof(zero_buf) <= TRACE_MAX_SIZE) ? sizeof(zero_buf) : TRACE_MAX_SIZE - i;
            fwrite(zero_buf, 1, chunk, f);
        }
        fflush(f);
        
        w->write_offset = 0;
        w->file_size = 0;
        w->wrapped = false;
    } else {
        /* Read existing index */
        uint64_t index;
        if (fread(&index, 1, TRACE_INDEX_SIZE, f) == TRACE_INDEX_SIZE) {
            w->write_offset = index;
        } else {
            w->write_offset = 0;
        }
        
        /* Determine file size by seeking to end of data area */
        fseek(f, 0, SEEK_END);
        long total_size = ftell(f);
        if (total_size >= (long)TRACE_INDEX_SIZE) {
            w->file_size = (uint64_t)(total_size - TRACE_INDEX_SIZE);
            if (w->file_size > TRACE_MAX_SIZE) w->file_size = TRACE_MAX_SIZE;
        } else {
            w->file_size = 0;
        }
        
        w->wrapped = (w->write_offset == 0 && w->file_size == TRACE_MAX_SIZE);
        if (!w->wrapped && w->file_size == TRACE_MAX_SIZE) {
            w->wrapped = true;
        }
        
        fclose(f);
    }
    
    /* Reopen via platform layer for writing */
    w->jsonl_file = platform_fopen(w->jsonl_path, "r+b");
    return w->jsonl_file != NULL;
}

static void trace_update_index(TraceWriter* w) {
    if (!w->jsonl_file || !w->jsonl_file->handle) return;
    
    FILE* f = (FILE*)w->jsonl_file->handle;
    long pos = ftell(f);
    
    /* Seek to index position (start of file) */
    fseek(f, 0, SEEK_SET);
    fwrite(&w->write_offset, 1, TRACE_INDEX_SIZE, f);
    fflush(f);
    
    /* Restore position */
    fseek(f, pos, SEEK_SET);
}

static size_t trace_write_line_internal(TraceWriter* w, const char* line, size_t line_len) {
    if (!w->jsonl_file || !w->jsonl_file->handle) return 0;
    
    /* Integer overflow check for line_len + 1 */
    if (line_len > SIZE_MAX - 1) return 0;
    
    FILE* f = (FILE*)w->jsonl_file->handle;
    
    /* Ensure line ends with newline */
    char newline = '\n';
    bool has_newline = (line_len > 0 && line[line_len - 1] == '\n');
    size_t total_len = line_len + (has_newline ? 0 : 1);
    
    /* If line is larger than max buffer, truncate (shouldn't happen with JSONL) */
    if (total_len > TRACE_MAX_SIZE) {
        total_len = TRACE_MAX_SIZE;
    }
    
    uint64_t data_start = TRACE_INDEX_SIZE;
    uint64_t end_of_data = data_start + TRACE_MAX_SIZE;
    uint64_t current_pos = data_start + w->write_offset;
    uint64_t space_to_end = end_of_data - current_pos;
    
    if (total_len <= space_to_end) {
        /* Fits in remaining space - write directly */
        fseek(f, (long)current_pos, SEEK_SET);
        fwrite(line, 1, line_len, f);
        if (!has_newline) fwrite(&newline, 1, 1, f);
        w->write_offset += total_len;
    } else {
        /* Record spans the wrap point - split the write */
        /* Write first part at the end of the buffer */
        fseek(f, (long)current_pos, SEEK_SET);
        fwrite(line, 1, space_to_end, f);
        
        /* Write remaining part at the beginning of the data area */
        size_t remaining_len = total_len - space_to_end;
        fseek(f, (long)data_start, SEEK_SET);
        fwrite(line + space_to_end, 1, remaining_len, f);
        
        w->write_offset = remaining_len;
        w->wrapped = true;
    }
    
    /* Ensure we don't exceed TRACE_MAX_SIZE if we just wrote a line 
       that's exactly the size of the buffer (though unlikely) */
    if (w->write_offset >= TRACE_MAX_SIZE) {
        w->write_offset = 0;
        w->wrapped = true;
    }
    
    /* Update logical file size */
    if (!w->wrapped) {
        if (w->write_offset > w->file_size) {
            w->file_size = w->write_offset;
        }
    } else {
        w->file_size = TRACE_MAX_SIZE;
    }
    
    /* Update index on disk */
    trace_update_index(w);
    fflush(f);
    
    return total_len;
}

static char* shot_plan_to_json(const ShotPlan* plan, char* buf, size_t size) {
    const char* tactic_names[] = {
        "BREAK", "DIRECT", "CUT", "BANK", "QUEEN", "COVER", "DEFENSIVE", "FALLBACK"
    };
    
    int written = snprintf(buf, size,
        "{\"placement\":{\"x\":%.6f,\"y\":%.6f},\"aim_angle\":%.6f,\"power\":%.6f,"
        "\"tactic\":\"%s\",\"imperfection_draw\":%u}",
        plan->placement.x, plan->placement.y,
        plan->aim_angle, plan->power,
        tactic_names[plan->tactic], plan->rng_draw);
    
    if (written < 0 || (size_t)written >= size) {
        /* Truncated - but we return the buffer as is for telemetry */
    }
    return buf;
}

static char* shot_result_to_json(const ShotResult* result, char* buf, size_t size) {
    char pockets_json[2048] = "[";
    char* p = pockets_json + 1;
    size_t remaining = sizeof(pockets_json) - 2;
    
    for (int i = 0; i < result->pocketed_count; i++) {
        const char* color_str = (result->pocketed_colors[i] == PIECE_WHITE) ? "WHITE" :
                               (result->pocketed_colors[i] == PIECE_BLACK) ? "BLACK" : "QUEEN";
        int written = snprintf(p, remaining,
            "%s{\"piece_id\":%d,\"color\":\"%s\",\"pocket\":%d}",
            i > 0 ? "," : "",
            (int)result->pocketed_ids[i], color_str, result->pocketed_pocket_indices[i]);
        if (written < 0 || (size_t)written >= remaining) break;
        p += written;
        remaining -= (size_t)written;
    }
    
    /* Safely close the array */
    if (remaining > 0) {
        *p = ']';
        *(p + 1) = '\0';
    } else {
        pockets_json[sizeof(pockets_json) - 1] = ']';
    }

    char final_pos_json[2048] = "[";
    char* fp = final_pos_json + 1;
    size_t fp_rem = sizeof(final_pos_json) - 2;
    for (int i = 0; i < 20; i++) {
        int written = snprintf(fp, fp_rem,
            "%s{\"id\":%d,\"pos\":{\"x\":%.6f,\"y\":%.6f}}",
            i == 0 ? "" : ",", i, result->final_positions[i].x, result->final_positions[i].y);
        if (written < 0 || (size_t)written >= fp_rem) break;
        fp += written;
        fp_rem -= (size_t)written;
    }
    if (fp_rem > 0) { *fp = ']'; *(fp + 1) = '\0'; } else { final_pos_json[sizeof(final_pos_json)-1] = ']'; }
    
    /* Pocket indices for all pieces (including non-pocketed ones as -1) */
    char pocket_indices_json[512] = "[";
    char* pip = pocket_indices_json + 1;
    size_t pip_rem = sizeof(pocket_indices_json) - 2;
    for (int i = 0; i < 19; i++) {
        int written = snprintf(pip, pip_rem, "%s%d", i == 0 ? "" : ",", (int)result->pocketed_pocket_indices[i]);
        if (written < 0 || (size_t)written >= pip_rem) break;
        pip += written;
        pip_rem -= (size_t)written;
    }
    if (pip_rem > 0) { *pip = ']'; *(pip + 1) = '\0'; } else { pocket_indices_json[sizeof(pocket_indices_json)-1] = ']'; }

    int written = snprintf(buf, size,
        "{\"pockets\":%s,\"final_positions\":%s,\"pocket_indices\":%s,\"queen_pocketed\":%s,\"striker_pocketed\":%s,\"fouls\":%d,"
        "\"sim_time\":%.6f}",
        pockets_json, final_pos_json, pocket_indices_json,
        result->queen_pocketed ? "true" : "false",
        result->striker_pocketed ? "true" : "false",
        result->fouls, result->sim_time);
    
    if (written < 0 || (size_t)written >= size) {
        /* Truncated */
    }
    return buf;
}

static const char* seat_to_str(Seat seat) {
    switch (seat) {
        case SEAT_NORTH: return "NORTH";
        case SEAT_EAST:  return "EAST";
        case SEAT_SOUTH: return "SOUTH";
        case SEAT_WEST:  return "WEST";
        default: return "UNKNOWN";
    }
}

static const char* team_to_str(Seat seat) {
    return (seat == SEAT_NORTH || seat == SEAT_SOUTH) ? "WHITE" : "BLACK";
}

static const char* tactic_to_str(TacticType tactic) {
    switch (tactic) {
        case TACTIC_BREAK:      return "BREAK";
        case TACTIC_DIRECT:     return "DIRECT";
        case TACTIC_CUT:        return "CUT";
        case TACTIC_BANK:       return "BANK";
        case TACTIC_QUEEN:      return "QUEEN";
        case TACTIC_COVER:      return "COVER";
        case TACTIC_DEFENSIVE:  return "DEFENSIVE";
        case TACTIC_FALLBACK:   return "FALLBACK";
        default:                return "UNKNOWN";
    }
}

static const char* turn_decision_to_str(TurnDecision td) {
    switch (td) {
        case TURN_CONTINUE:   return "CONTINUE";
        case TURN_ADVANCE:    return "ADVANCE";
        case TURN_BOARD_OVER: return "BOARD_OVER";
        case TURN_GAME_OVER:  return "GAME_OVER";
        case TURN_MATCH_OVER: return "MATCH_OVER";
        default:              return "UNKNOWN";
    }
}

/* -----------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------- */

void trace_write_pocket_near_miss(TraceWriter* writer, uint8_t piece_id, uint8_t pocket_index, float distance, float speed) {
    if (!writer || !writer->jsonl_file) return;

    char json[512];
    int written = snprintf(json, sizeof(json),
        "{\"type\":\"POCKET_NEAR_MISS\",\"piece_id\":%d,\"pocket\":%d,\"distance\":%.6f,\"speed\":%.6f}",
        piece_id, pocket_index, distance, speed);
    
    if (written < 0 || (size_t)written >= sizeof(json)) {
        /* Truncated */
    }
    
    trace_write_line_internal(writer, json, strlen(json));

    if (writer->log_file) {
        platform_fprintf(writer->log_file, 
            "  [NEAR MISS] Piece %d near pocket %d (dist=%.4f, speed=%.4f)\n",
            piece_id, pocket_index, distance, speed);
        platform_fflush(writer->log_file);
    }
}

TraceWriter* trace_open(const char* path, const char* log_dir, bool verbose, uint64_t seed) {

    /* Integer overflow check for TraceWriter allocation */
    if (sizeof(TraceWriter) > SIZE_MAX) {
        return NULL;
    }
    TraceWriter* w = calloc(1, sizeof(TraceWriter));
    if (!w) return NULL;
    
    w->verbose = verbose;
    w->seed = seed;
    
    /* JSONL path */
    strncpy(w->jsonl_path, path, sizeof(w->jsonl_path) - 1);
    w->jsonl_path[sizeof(w->jsonl_path) - 1] = '\0';
    
    /* Initialize the circular file */
    if (!trace_init_file(w)) {
        free(w);
        return NULL;
    }
    
    /* Human-readable log file (also circular, separate 8 MiB ring) */
    if (verbose && log_dir) {
        platform_mkdir(log_dir);
        
        snprintf(w->log_path, sizeof(w->log_path), "%s/seed_%" PRIu64 ".log", log_dir, seed);
        
        /* For log file, use same circular approach but simpler - just append with size check */
        w->log_file = platform_fopen(w->log_path, "w");
        if (w->log_file) {
            /* Write header */
            platform_fprintf(w->log_file, "# Carrom Arena Log - Seed: %" PRIu64 " - Build: %s\n", seed, PLATFORM_BUILD_ID);
            platform_fflush(w->log_file);
        }
    }
    
    /* Write initial header comment to JSONL (only on fresh file) */
    if (w->file_size == 0 && !w->wrapped) {
        char header[256];
        snprintf(header, sizeof(header), "# Carrom Arena Trace - Build: %s - Seed: %" PRIu64 " - Date: %s\n", 
                 PLATFORM_BUILD_ID, seed, __DATE__);
        trace_write_line_internal(w, header, strlen(header));
    }
    
    w->shot_count = 0;
    return w;
}

void trace_close(TraceWriter* writer) {
    if (!writer) return;
    
    trace_flush(writer);
    
    if (writer->jsonl_file) platform_fclose(writer->jsonl_file);
    if (writer->log_file) platform_fclose(writer->log_file);
    free(writer);
}

void trace_flush(TraceWriter* writer) {
    if (!writer || !writer->jsonl_file) return;
    platform_fflush(writer->jsonl_file);
    if (writer->log_file) platform_fflush(writer->log_file);
}

void trace_write_shot_start(TraceWriter* writer, const MatchState* match, const GameState* game, 
                            uint64_t shot_number, Seat seat, const ShotPlan* plan) {
    if (!writer) return;
    
    writer->shot_count++;
    
    /* Pre-state hash */
    uint64_t pre_hash = 0;
    pre_hash += (uint64_t)(match->boards_won_white) * 1000000ULL;
    pre_hash += (uint64_t)(match->boards_won_black) * 10000ULL;
    
    uint64_t white_score = (uint64_t)game->scores.white;
    uint64_t black_score = (uint64_t)game->scores.black;
    pre_hash += white_score * 100ULL;
    pre_hash += black_score;
    pre_hash += shot_number;
    
    char plan_json[512];
    shot_plan_to_json(plan, plan_json, sizeof(plan_json));
    
    /* Build JSONL record */
    char json[2048];
    int written = snprintf(json, sizeof(json),
        "{"
        "\"build_id\":\"%s\","
        "\"seed\":%" PRIu64 ","
        "\"game_id\":%d,"
        "\"board_id\":%d,"
        "\"shot_number\":%" PRIu64 ","
        "\"active_player\":\"%s\","
        "\"active_team\":\"%s\","
        "\"pre_state_hash\":\"%016" PRIx64 "\","
        "\"shot_plan\":%s,"
        "\"planner_meta\":{\"candidates_evaluated\":0,\"best_score\":0.0,\"search_budget_used\":0}"
        "}",
        PLATFORM_BUILD_ID,
        writer->seed,
        (int)(match->games_won_white + match->games_won_black),
        (int)(match->boards_won_white + match->boards_won_black),
        shot_number,
        seat_to_str(seat),
        team_to_str(seat),
        pre_hash,
        plan_json);
    
    if (written < 0 || (size_t)written >= sizeof(json)) {
        /* Truncated */
    }
    
    trace_write_line_internal(writer, json, strlen(json));
    
    /* Human-readable log */
    if (writer->log_file) {
        platform_fprintf(writer->log_file, 
            "[SHOT %" PRIu64 "] %s (%s) - Plan: pos=(%.3f,%.3f) aim=%.3f power=%.3f tactic=%s\n",
            shot_number,
            seat_to_str(seat),
            team_to_str(seat),
            plan->placement.x, plan->placement.y,
            plan->aim_angle, plan->power,
            tactic_to_str(plan->tactic));
        platform_fflush(writer->log_file);
    }
}

void trace_write_shot_end(TraceWriter* writer, const ShotResult* result, const RulesOutcome* outcome) {
    if (!writer) return;
    
    char result_json[2048];
    shot_result_to_json(result, result_json, sizeof(result_json));
    
    char json[4096];
    int written = snprintf(json, sizeof(json),
        "{"
        "\"result\":%s,"
        "\"score_delta\":{\"white\":%d,\"black\":%d},"
        "\"turn_decision\":\"%s\","
        "\"post_state_hash\":\"%016" PRIx64 "\","
        "\"runtime_errors\":[]"
        "}",
        result_json,
        outcome->score_delta.white,
        outcome->score_delta.black,
        turn_decision_to_str(outcome->turn_decision),
        (uint64_t)(outcome->next_game_state.scores.white * 100 + outcome->next_game_state.scores.black));
    
    if (written < 0 || (size_t)written >= sizeof(json)) {
        /* Truncated */
    }
    
    trace_write_line_internal(writer, json, strlen(json));
    
    if (writer->log_file) {
        platform_fprintf(writer->log_file, 
            "  -> Result: pockets=%d queen=%s striker=%s fouls=0x%X score=(%d,%d) turn=%s\n",
            result->pocketed_count,
            result->queen_pocketed ? "YES" : "NO",
            result->striker_pocketed ? "YES" : "NO",
            result->fouls,
            outcome->score_delta.white, outcome->score_delta.black,
            turn_decision_to_str(outcome->turn_decision));
        platform_fflush(writer->log_file);
    }
}

void trace_write_event(TraceWriter* writer, const GameEvent* evt) {
    if (!writer || !writer->jsonl_file) return;
    
    char json[512];
    event_to_json(evt, json, sizeof(json));
    
    trace_write_line_internal(writer, json, strlen(json));
    
    if (writer->log_file) {
        events_log(evt, writer->log_file);
        platform_fflush(writer->log_file);
    }
}

void trace_write_physics_state(TraceWriter* writer, uint64_t frame, uint64_t shot_number, 
                               float sim_time, const Vec2* striker_vel, const Vec2* striker_pos,
                               const char* phase) {
    if (!writer || !writer->jsonl_file) return;
    
    float speed = sqrtf(striker_vel->x * striker_vel->x + striker_vel->y * striker_vel->y);
    float angle = atan2f(striker_vel->y, striker_vel->x);
    
    char json[512];
    int written = snprintf(json, sizeof(json),
        "{"
        "\"type\":\"PHYSICS_STATE\","
        "\"frame\":%" PRIu64 ","
        "\"shot_number\":%" PRIu64 ","
        "\"sim_time\":%.6f,"
        "\"phase\":\"%s\","
        "\"striker\":{"
        "\"pos\":{\"x\":%.6f,\"y\":%.6f},"
        "\"vel\":{\"x\":%.6f,\"y\":%.6f},"
        "\"speed\":%.6f,"
        "\"angle\":%.6f"
        "}"
        "}",
        frame, shot_number, sim_time, phase,
        striker_pos->x, striker_pos->y,
        striker_vel->x, striker_vel->y,
        speed, angle);
    
    if (written < 0 || (size_t)written >= sizeof(json)) {
        /* Truncated */
    }
    
    trace_write_line_internal(writer, json, strlen(json));
}

/* -----------------------------------------------------------------------------
 * Validation & Reading
 * --------------------------------------------------------------------------- */

TraceRecordArray trace_read_last_records(const char* path, size_t max_records) {
    TraceRecordArray arr = {0};
    char* data = NULL;
    char* logical_buf = NULL;
    size_t* line_starts = NULL;
    FILE* f = NULL;
    bool success = false;

    arr.capacity = max_records > 0 ? max_records : 100;
    if (arr.capacity > 0 && sizeof(char*) > SIZE_MAX / arr.capacity) {
        return arr;
    }
    arr.lines = calloc(arr.capacity, sizeof(char*));
    if (!arr.lines) {
        arr.capacity = 0;
        return arr;
    }
    arr.count = 0;

    f = fopen(path, "rb");
    if (!f) goto cleanup;

    uint64_t write_offset;
    if (fread(&write_offset, 1, TRACE_INDEX_SIZE, f) != TRACE_INDEX_SIZE) {
        goto cleanup;
    }

    /* DEBUG: Dump state */
    // printf("[DEBUG] path=%s, write_offset=%lu\n", path, (unsigned long)write_offset);

    fseek(f, 0, SEEK_END);
    long actual_file_size = ftell(f);

    fseek(f, TRACE_INDEX_SIZE, SEEK_SET);
    data = calloc(1, TRACE_MAX_SIZE);
    if (!data) goto cleanup;
    size_t read_bytes = fread(data, 1, TRACE_MAX_SIZE, f);
    fclose(f);
    f = NULL;

    if (read_bytes == 0) goto cleanup;

    /* The buffer is wrapped if the file size is at least index + max size */
    bool is_wrapped = (actual_file_size >= (long)(TRACE_INDEX_SIZE + TRACE_MAX_SIZE));

    logical_buf = calloc(1, TRACE_MAX_SIZE * 2 + 1);
    if (!logical_buf) goto cleanup;

    size_t logical_data_len = 0;
    if (is_wrapped) {
        /* To find the last records, we want the data just before write_offset.
         * The circular buffer contains: [0, write_offset) <--- NEWEST | [write_offset, TRACE_MAX_SIZE) <--- OLDEST
         * In chronological order: [write_offset, TRACE_MAX_SIZE) then [0, write_offset)
         */
        size_t part1_len = TRACE_MAX_SIZE - write_offset;
        memcpy(logical_buf, data + write_offset, part1_len);
        memcpy(logical_buf + part1_len, data, write_offset);
        logical_data_len = TRACE_MAX_SIZE;
    } else {
        /* Not wrapped: [0, write_offset) */
        size_t copy_len = write_offset;
        if (copy_len > (size_t)read_bytes) copy_len = (size_t)read_bytes;
        memcpy(logical_buf, data, copy_len);
        logical_data_len = copy_len;
    }

    /* Trim trailing nulls/zeros if the buffer isn't fully utilized */
    while (logical_data_len > 0 && logical_buf[logical_data_len - 1] == 0) {
        logical_data_len--;
    }

    /* 
     * Robust Line Extraction:
     * 1. Find all possible line starts.
     * 2. Filter out comments and partial records.
     * 3. Ensure we only take complete records.
     */
    size_t max_possible_lines = logical_data_len + 1;
    line_starts = malloc(max_possible_lines * sizeof(size_t));
    if (!line_starts) goto cleanup;

    size_t found_lines = 0;
    if (logical_data_len > 0) {
        /* 
         * If we wrapped, the first byte of logical_buf might be in the middle of a record.
         * The first record is only valid if it starts with '{'.
         */
        if (logical_buf[0] == '{') {
            line_starts[found_lines++] = 0;
        }
        for (size_t i = 0; i < logical_data_len; i++) {
            if (logical_buf[i] == '\n') {
                /* Only mark as a start if the next char is '{' */
                if (i + 1 < logical_data_len && logical_buf[i + 1] == '{') {
                    line_starts[found_lines++] = i + 1;
                }
            }
        }
    }

    /* Extract last max_records */
    size_t start_idx = (found_lines > max_records) ? (found_lines - max_records) : 0;
    for (size_t i = start_idx; i < found_lines && arr.count < arr.capacity; i++) {
        size_t line_start = line_starts[i];
        size_t line_end = logical_data_len;
        for (size_t j = line_start; j < logical_data_len; j++) {
            if (logical_buf[j] == '\n') {
                line_end = j;
                break;
            }
        }
        size_t line_len = line_end - line_start;
        if (line_len == 0) continue;
        char* line = malloc(line_len + 1);
        if (!line) goto cleanup;
        memcpy(line, logical_buf + line_start, line_len);
        line[line_len] = '\0';
        
        arr.lines[arr.count++] = line;
    }

    success = true;

cleanup:
    if (f) fclose(f);
    free(line_starts);
    free(data);
    free(logical_buf);
    if (!success && arr.lines) {
        for (size_t i = 0; i < arr.count; i++) free(arr.lines[i]);
        free(arr.lines);
        arr.lines = NULL;
        arr.count = 0;
        arr.capacity = 0;
    }
    return arr;
}

void trace_record_array_free(TraceRecordArray* arr) {
    if (!arr || !arr->lines) return;
    for (size_t i = 0; i < arr->count; i++) {
        free(arr->lines[i]);
    }
    free(arr->lines);
    arr->lines = NULL;
    arr->count = 0;
    arr->capacity = 0;
}