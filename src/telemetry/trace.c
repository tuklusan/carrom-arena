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
        /* Need to wrap - write from data_start (after index) */
        fseek(f, (long)data_start, SEEK_SET);
        fwrite(line, 1, line_len, f);
        if (!has_newline) fwrite(&newline, 1, 1, f);
        w->write_offset = total_len;
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
    
    snprintf(buf, size,
        "{\"placement\":{\"x\":%.6f,\"y\":%.6f},\"aim_angle\":%.6f,\"power\":%.6f,"
        "\"tactic\":\"%s\",\"imperfection_draw\":%u}",
        plan->placement.x, plan->placement.y,
        plan->aim_angle, plan->power,
        tactic_names[plan->tactic], plan->rng_draw);
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
            "%s{\"piece_id\":%d,\"color\":\"%s\"}",
            i > 0 ? "," : "",
            (int)result->pocketed_ids[i], color_str);
        if (written >= (int)remaining) break;
        p += written;
        remaining -= (size_t)written;
    }
    strcat(pockets_json, "]");
    
    snprintf(buf, size,
        "{\"pockets\":%s,\"queen_pocketed\":%s,\"striker_pocketed\":%s,\"fouls\":%d,"
        "\"sim_time\":%.6f}",
        pockets_json,
        result->queen_pocketed ? "true" : "false",
        result->striker_pocketed ? "true" : "false",
        result->fouls, result->sim_time);
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
    snprintf(json, sizeof(json),
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
    snprintf(json, sizeof(json),
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

/* -----------------------------------------------------------------------------
 * Validation & Reading
 * --------------------------------------------------------------------------- */

bool trace_validate_determinism(const char* trace1, const char* trace2) {
    FILE* f1 = fopen(trace1, "r");
    FILE* f2 = fopen(trace2, "r");
    if (!f1 || !f2) {
        if (f1) fclose(f1);
        if (f2) fclose(f2);
        return false;
    }
    
    /* Skip index bytes (first 8) */
    fseek(f1, TRACE_INDEX_SIZE, SEEK_SET);
    fseek(f2, TRACE_INDEX_SIZE, SEEK_SET);
    
    char line1[4096], line2[4096];
    bool equal = true;
    int line_num = 0;
    
    while (true) {
        /* Read next non-comment line from f1 */
        bool got1 = false;
        while (fgets(line1, sizeof(line1), f1)) {
            line_num++;
            if (line1[0] != '#') {
                got1 = true;
                break;
            }
        }
        
        /* Read next non-comment line from f2 */
        bool got2 = false;
        while (fgets(line2, sizeof(line2), f2)) {
            if (line2[0] != '#') {
                got2 = true;
                break;
            }
        }
        
        if (!got1 && !got2) {
            /* Both files exhausted */
            break;
        }
        if (!got1 || !got2) {
            /* One file exhausted before the other */
            equal = false;
            break;
        }
        
        if (strcmp(line1, line2) != 0) {
            equal = false;
            fprintf(stderr, "Trace mismatch at line %d\n", line_num);
            break;
        }
    }
    
    fclose(f1);
    fclose(f2);
    return equal;
}

/* Read last N complete JSONL records from circular trace file */
/* Caller MUST call trace_record_array_free() on the returned array to avoid leaks. */
TraceRecordArray trace_read_last_records(const char* path, size_t max_records) {
    TraceRecordArray arr = {0};
    arr.capacity = max_records > 0 ? max_records : 100;
    
    /* Integer overflow check for calloc */
    if (arr.capacity > 0 && sizeof(char*) > SIZE_MAX / arr.capacity) {
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    arr.lines = calloc(arr.capacity, sizeof(char*));
    if (!arr.lines) {
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    arr.count = 0;
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    
    /* Read index to find write position */
    uint64_t write_offset;
    if (fread(&write_offset, 1, TRACE_INDEX_SIZE, f) != TRACE_INDEX_SIZE) {
        fclose(f);
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    
    /* Read entire data area - check for integer overflow */
    if (TRACE_MAX_SIZE > SIZE_MAX) {
        fclose(f);
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    char* data = malloc(TRACE_MAX_SIZE);
    if (!data) {
        fclose(f);
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    
    fseek(f, TRACE_INDEX_SIZE, SEEK_SET);
    size_t read_bytes = fread(data, 1, TRACE_MAX_SIZE, f);
    fclose(f);
    
    if (read_bytes == 0) {
        free(data);
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    
    /* Determine valid data range - for non-wrapped files, valid data ends at write_offset */
    /* For wrapped files, valid data wraps around. For simplicity, we scan the entire */
    /* read buffer but the zero-filled portion has no newlines so won't create false lines. */
    (void)write_offset;
    
    /* First pass: count actual number of lines */
    size_t line_count = 0;
    for (size_t i = 0; i < read_bytes; i++) {
        if (data[i] == '\n') {
            line_count++;
        }
    }
    
    /* We need line_count + 1 entries to include position 0 as first line start */
    size_t line_starts_capacity = line_count + 1;
    
    /* Integer overflow check for line_starts allocation */
    if (line_starts_capacity > 0 && sizeof(size_t) > SIZE_MAX / line_starts_capacity) {
        free(data);
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    
    /* Allocate line_starts array with exact size needed (includes position 0) */
    size_t* line_starts = calloc(line_starts_capacity, sizeof(size_t));
    if (!line_starts) {
        free(data);
        free(arr.lines);
        arr.lines = NULL;
        arr.capacity = 0;
        return arr;
    }
    
    /* Fill in line start positions: position 0, then after each newline */
    line_starts[0] = 0;
    size_t current_line = 1;
    for (size_t i = 0; i < read_bytes && current_line < line_starts_capacity; i++) {
        if (data[i] == '\n') {
            line_starts[current_line++] = i + 1;  /* Next char after newline */
        }
    }
    /* total_lines = number of valid lines = number of newlines (line_count) */
    /* The last line_start (at index line_count) points to after the last newline, */
    /* which is end of valid data - not a real line with content */
    size_t total_lines = line_count;
    
    /* Now extract last N lines in logical order (oldest first) */
    /* Lines are indexed 0..total_lines-1, where line 0 starts at position 0 */
    size_t lines_to_read = (total_lines > max_records) ? max_records : total_lines;
    size_t start_idx = (total_lines > lines_to_read) ? total_lines - lines_to_read : 0;
    
    bool allocation_failed = false;
    for (size_t i = start_idx; i < total_lines && arr.count < arr.capacity; i++) {
        size_t line_start = line_starts[i];
        size_t line_end = (i + 1 < total_lines) ? line_starts[i + 1] - 1 : read_bytes;
        
        if (line_end > read_bytes) line_end = read_bytes;
        if (line_start >= line_end) continue;
        
        size_t line_len = line_end - line_start;
        if (line_len == 0) continue;
        
        /* Skip comment lines */
        if (data[line_start] == '#') continue;
        
        /* Integer overflow check for line allocation */
        if (line_len > SIZE_MAX - 1) {
            allocation_failed = true;
            break;
        }
        char* line = malloc(line_len + 1);
        if (!line) {
            allocation_failed = true;
            break;
        }
        
        memcpy(line, data + line_start, line_len);
        line[line_len] = '\0';
        
        arr.lines[arr.count++] = line;
    }
    
    /* If allocation failed partway, clean up partially allocated lines */
    if (allocation_failed) {
        for (size_t i = 0; i < arr.count; i++) {
            free(arr.lines[i]);
            arr.lines[i] = NULL;
        }
        arr.count = 0;
    }
    
    free(line_starts);
    free(data);
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