#ifndef CARROM_TYPES_H
#define CARROM_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* =============================================================================
 * Carrom Arena - Authoritative Data Model (Appendix A.3)
 * Single header with ALL type definitions - no forward declarations needed
 * ============================================================================= */

/* -----------------------------------------------------------------------------
 * Basic Math Types (normalized units: board side = 1.0)
 * --------------------------------------------------------------------------- */
typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;

static inline Vec2 vec2(float x, float y) { return (Vec2){x, y}; }
static inline Vec2 vec2_add(Vec2 a, Vec2 b) { return (Vec2){a.x + b.x, a.y + b.y}; }
static inline Vec2 vec2_sub(Vec2 a, Vec2 b) { return (Vec2){a.x - b.x, a.y - b.y}; }
static inline Vec2 vec2_mul(Vec2 a, float s) { return (Vec2){a.x * s, a.y * s}; }
static inline float vec2_dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static inline float vec2_len2(Vec2 a) { return vec2_dot(a, a); }

/* -----------------------------------------------------------------------------
 * Normalized Unit Constants (board side = 1.0)
 * --------------------------------------------------------------------------- */
#define BOARD_SIDE_NORM       1.0f
#define POCKET_RADIUS_NORM    0.030f
#define PIECE_RADIUS_NORM     0.021f
#define STRIKER_RADIUS_NORM   0.028f

#define CUSHION_THICKNESS     0.025f
#define CUSHION_INNER_MARGIN  (CUSHION_THICKNESS + PIECE_RADIUS_NORM)

/* POCKET_CENTERS defined in math.c */
extern const Vec2 POCKET_CENTERS[4];

#define BASELINE_Y_NORTH  (0.5f - CUSHION_THICKNESS - STRIKER_RADIUS_NORM)
#define BASELINE_Y_SOUTH  (-0.5f + CUSHION_THICKNESS + STRIKER_RADIUS_NORM)
#define BASELINE_X_EAST   (0.5f - CUSHION_THICKNESS - STRIKER_RADIUS_NORM)
#define BASELINE_X_WEST   (-0.5f + CUSHION_THICKNESS + STRIKER_RADIUS_NORM)

#define BASELINE_MIN_OFFSET 0.15f
#define BASELINE_MAX_OFFSET 0.35f

#define MAX_PIECES 19
#define QUEEN_ID 18
#define STRIKER_ID 19

/* -----------------------------------------------------------------------------
 * Enumerations
 * --------------------------------------------------------------------------- */
typedef enum { TEAM_WHITE, TEAM_BLACK } Team;
typedef enum { SEAT_NORTH, SEAT_EAST, SEAT_SOUTH, SEAT_WEST } Seat;
typedef enum { PIECE_WHITE, PIECE_BLACK, PIECE_QUEEN, PIECE_STRIKER } PieceColor;

typedef enum {
    PHASE_IDLE, PHASE_PLACEMENT, PHASE_AIMING, PHASE_SHOT_EXECUTION,
    PHASE_SETTLING, PHASE_RESOLVING, PHASE_BOARD_OVER, PHASE_GAME_OVER, PHASE_MATCH_OVER
} GamePhase;

typedef enum {
    TACTIC_BREAK, TACTIC_DIRECT, TACTIC_CUT, TACTIC_BANK,
    TACTIC_QUEEN, TACTIC_COVER, TACTIC_DEFENSIVE, TACTIC_FALLBACK
} TacticType;

typedef enum {
    FOUL_NONE = 0, FOUL_STRIKER_POCKETED = 1<<0, FOUL_WRONG_POCKET = 1<<1,
    FOUL_NO_CONTACT = 1<<2, FOUL_ILLEGAL_PLACEMENT = 1<<3, FOUL_PIECE_OFF_BOARD = 1<<4
} FoulFlags;

typedef enum {
    QUEEN_STATE_ON_BOARD, QUEEN_STATE_POCKETED_NO_COVER,
    QUEEN_STATE_COVERED, QUEEN_STATE_DUE
} QueenState;

typedef enum { DUE_NONE, DUE_PIECE, DUE_QUEEN } DueType;

typedef enum {
    TURN_CONTINUE, TURN_ADVANCE, TURN_BOARD_OVER, TURN_GAME_OVER, TURN_MATCH_OVER
} TurnDecision;

typedef enum {
    EVENT_POCKET, EVENT_FOUL, EVENT_QUEEN_POCKETED, EVENT_QUEEN_COVERED,
    EVENT_TURN_CHANGE, EVENT_BOARD_START, EVENT_BOARD_END,
    EVENT_GAME_START, EVENT_GAME_END, EVENT_MATCH_START, EVENT_MATCH_END
} GameEventType;

/* -----------------------------------------------------------------------------
 * Viewport for screen/world conversion (render only)
 * --------------------------------------------------------------------------- */
typedef struct {
    int screen_width, screen_height;
    float board_size_px;
    Vec2 board_center_px;
    float world_to_screen;
} Viewport;

/* -----------------------------------------------------------------------------
 * Piece / Board State
 * --------------------------------------------------------------------------- */
typedef struct {
    uint8_t id;
    PieceColor color;
    Vec2 position, velocity;
    bool pocketed, on_board;
} PieceState;

typedef struct {
    Vec2 position, velocity;
    bool pocketed, on_baseline;
    Seat owner_seat;
} StrikerState;

typedef struct {
    PieceState pieces[MAX_PIECES];
    StrikerState striker;
    QueenState queen_state;
    uint8_t white_on_board, black_on_board;
    bool queen_on_board;
    uint8_t white_dues, black_dues, queen_dues;
} BoardState;

/* -----------------------------------------------------------------------------
 * Score & Match State
 * --------------------------------------------------------------------------- */
typedef struct { int white, black; } TeamScores;

typedef struct {
    uint64_t id; Team team; Seat seat; uint8_t strategy_index;
} PlayerState;

typedef struct {
    GamePhase phase;
    PlayerState active_player;
    TeamScores scores;
    Seat turn_seat;
    uint8_t consecutive_turns;
    BoardState board;
} GameState;

typedef struct {
    uint8_t boards_won_white, boards_won_black;
    uint8_t games_won_white, games_won_black;
    uint8_t target_boards_per_game, target_games_per_match;
} MatchState;

/* -----------------------------------------------------------------------------
 * Shot Planning & Results
 * --------------------------------------------------------------------------- */
typedef struct {
    Vec2 placement; float aim_angle, power; TacticType tactic; uint32_t rng_draw;
} ShotPlan;

typedef struct {
    uint8_t pocketed_ids[19], pocketed_count, pocketed_colors[19];
    bool queen_pocketed, striker_pocketed; FoulFlags fouls;
    Vec2 final_positions[19]; float sim_time;
} ShotResult;

/* -----------------------------------------------------------------------------
 * Game Events (Appendix A.8)
 * --------------------------------------------------------------------------- */
typedef struct {
    GameEventType type; uint64_t tick; Seat seat; Team team;
    uint8_t piece_id; PieceColor piece_color; int8_t pocket_index;
    int score_delta_white, score_delta_black; TurnDecision turn_decision;
} GameEvent;

/* -----------------------------------------------------------------------------
 * RNG State (PCG32)
 * --------------------------------------------------------------------------- */
typedef struct { uint64_t state, inc; } PCG32;

typedef struct {
    PCG32 streams[4];
    PCG32 global;
    uint64_t master_seed;
} RNGContext;

typedef struct { uint64_t state, inc; } RNGSnapshot;

/* -----------------------------------------------------------------------------
 * Physics World & Snapshot (for AI scratch simulation)
 * Forward declarations - full definition in physics/physics_snapshot.h
 * --------------------------------------------------------------------------- */
typedef struct PhysicsWorld PhysicsWorld;
typedef struct PhysicsSnapshot PhysicsSnapshot;

struct PhysicsWorld;
struct PhysicsSnapshot;

/* -----------------------------------------------------------------------------
 * Decision Snapshot (AI Controller Input)
 * --------------------------------------------------------------------------- */
typedef struct {
    const MatchState* match; const GameState* game;
    const BoardState* board; const PhysicsSnapshot* physics; Seat active_seat;
} DecisionSnapshot;

/* -----------------------------------------------------------------------------
 * Strategy Profile (D5 - CEO Approved)
 * --------------------------------------------------------------------------- */
typedef struct {
    float weight_pocket, weight_queen, weight_cover;
    float weight_striker_risk, weight_opponent_leave, weight_positional;
    float aim_noise_std, power_noise_std;
} StrategyProfile;

/* -----------------------------------------------------------------------------
 * Rules Engine Types
 * --------------------------------------------------------------------------- */
typedef struct {
    uint8_t pocketed_ids[19], pocketed_count, pocketed_colors[19];
    bool queen_pocketed, striker_pocketed; FoulFlags fouls;
    Seat active_seat; QueenState queen_state;
    uint8_t white_dues, black_dues, queen_dues;
} ShotFacts;

typedef struct {
    GameState next_game_state; MatchState next_match_state;
    TeamScores score_delta;
    uint8_t due_actions_white, due_actions_black, due_actions_queen;
    TurnDecision turn_decision; GameEvent events[16]; int event_count;
} RulesOutcome;

#ifdef __cplusplus
}
#endif

#endif // CARROM_TYPES_H