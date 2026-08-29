#include "events.h"
#include "common/types.h"
#include "platform/platform.h"
#include <stdio.h>

void events_log(const GameEvent* evt, PlatformFile* log_file) {
    if (!log_file) return;
    
    const char* type_str = "UNKNOWN";
    switch (evt->type) {
        case EVENT_POCKET: type_str = "POCKET"; break;
        case EVENT_FOUL: type_str = "FOUL"; break;
        case EVENT_QUEEN_POCKETED: type_str = "QUEEN_POCKETED"; break;
        case EVENT_QUEEN_COVERED: type_str = "QUEEN_COVERED"; break;
        case EVENT_TURN_CHANGE: type_str = "TURN_CHANGE"; break;
        case EVENT_BOARD_START: type_str = "BOARD_START"; break;
        case EVENT_BOARD_END: type_str = "BOARD_END"; break;
        case EVENT_GAME_START: type_str = "GAME_START"; break;
        case EVENT_GAME_END: type_str = "GAME_END"; break;
        case EVENT_MATCH_START: type_str = "MATCH_START"; break;
        case EVENT_MATCH_END: type_str = "MATCH_END"; break;
    }
    
    platform_fprintf(log_file, "[%llu] %s seat=%s team=%s piece=%d color=%s pocket=%d score=(%d,%d) turn=%d\n",
           (unsigned long long)evt->tick, type_str,
           (evt->seat == SEAT_NORTH) ? "NORTH" : (evt->seat == SEAT_EAST) ? "EAST" : 
           (evt->seat == SEAT_SOUTH) ? "SOUTH" : "WEST",
           (evt->team == TEAM_WHITE) ? "WHITE" : "BLACK",
           evt->piece_id,
           (evt->piece_color == PIECE_WHITE) ? "WHITE" : (evt->piece_color == PIECE_BLACK) ? "BLACK" : "QUEEN",
           evt->pocket_index,
           evt->score_delta_white, evt->score_delta_black,
           evt->turn_decision);
}

char* event_to_json(const GameEvent* evt, char* buffer, size_t size) {
    const char* type_str = "UNKNOWN";
    switch (evt->type) {
        case EVENT_POCKET: type_str = "POCKET"; break;
        case EVENT_FOUL: type_str = "FOUL"; break;
        case EVENT_QUEEN_POCKETED: type_str = "QUEEN_POCKETED"; break;
        case EVENT_QUEEN_COVERED: type_str = "QUEEN_COVERED"; break;
        case EVENT_TURN_CHANGE: type_str = "TURN_CHANGE"; break;
        case EVENT_BOARD_START: type_str = "BOARD_START"; break;
        case EVENT_BOARD_END: type_str = "BOARD_END"; break;
        case EVENT_GAME_START: type_str = "GAME_START"; break;
        case EVENT_GAME_END: type_str = "GAME_END"; break;
        case EVENT_MATCH_START: type_str = "MATCH_START"; break;
        case EVENT_MATCH_END: type_str = "MATCH_END"; break;
    }
    
    const char* seat_str = (evt->seat == SEAT_NORTH) ? "NORTH" : (evt->seat == SEAT_EAST) ? "EAST" : 
                           (evt->seat == SEAT_SOUTH) ? "SOUTH" : "WEST";
    const char* team_str = (evt->team == TEAM_WHITE) ? "WHITE" : "BLACK";
    const char* color_str = (evt->piece_color == PIECE_WHITE) ? "WHITE" : (evt->piece_color == PIECE_BLACK) ? "BLACK" : "QUEEN";
    const char* turn_str = "ADVANCE";
    switch (evt->turn_decision) {
        case TURN_CONTINUE: turn_str = "CONTINUE"; break;
        case TURN_ADVANCE: turn_str = "ADVANCE"; break;
        case TURN_BOARD_OVER: turn_str = "BOARD_OVER"; break;
        case TURN_GAME_OVER: turn_str = "GAME_OVER"; break;
        case TURN_MATCH_OVER: turn_str = "MATCH_OVER"; break;
    }
    
    snprintf(buffer, size,
        "{\"type\":\"%s\",\"tick\":%llu,\"seat\":\"%s\",\"team\":\"%s\",\"piece_id\":%d,\"piece_color\":\"%s\","
        "\"pocket\":%d,\"score_delta_white\":%d,\"score_delta_black\":%d,\"turn_decision\":\"%s\"}",
        type_str, (unsigned long long)evt->tick, seat_str, team_str,
        evt->piece_id, color_str, evt->pocket_index,
        evt->score_delta_white, evt->score_delta_black, turn_str);
    
    return buffer;
}

void events_emit_trace(const GameEvent* evt, TraceWriter* trace) {
    // Implemented in trace.c
    (void)evt;
    (void)trace;
}