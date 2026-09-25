#include "unity.h"
#include "common/types.h"
#include "game/board.h"
#include "physics/physics.h"
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

typedef struct { int count[SOUND_KIND_COUNT]; float max_speed[SOUND_KIND_COUNT]; } Seen;

static void empty_board(BoardState* b) {
    board_state_init(b);
    for (int i = 0; i < MAX_PIECES; i++) { b->pieces[i].on_board = false; b->pieces[i].pocketed = false; }
}

static void coin(BoardState* b, int id, float x, float y) {
    b->pieces[id].position = (Vec2){x, y};
    b->pieces[id].on_board = true;
    b->pieces[id].color = (id < 9) ? PIECE_WHITE : PIECE_BLACK;
}

static Seen run(PhysicsWorld* pw, int steps) {
    Seen s;
    memset(&s, 0, sizeof(s));
    SoundEvent ev[64];
    for (int i = 0; i < steps; i++) {
        physics_step(pw, PHYSICS_DT);
        int n = physics_drain_sound_events(pw, ev, 64);
        for (int k = 0; k < n; k++) {
            TEST_ASSERT_TRUE(ev[k].kind < SOUND_KIND_COUNT);
            s.count[ev[k].kind]++;
            if (ev[k].speed > s.max_speed[ev[k].kind]) s.max_speed[ev[k].kind] = ev[k].speed;
        }
    }
    return s;
}

void test_flick_and_striker_hits_coin(void) {
    BoardState b; empty_board(&b);
    coin(&b, 0, 0.0f, 0.0f);
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &b, SEAT_SOUTH);
    physics_place_striker(pw, SEAT_SOUTH, (Vec2){-0.2f, 0.0f});
    physics_apply_shot(pw, 0.0f, 0.5f);
    Seen s = run(pw, 240);
    physics_destroy(pw);
    printf("SOUNDS flick=%d striker_coin=%d (%.2f u/s)\n", s.count[SOUND_FLICK], s.count[SOUND_STRIKER_COIN], s.max_speed[SOUND_STRIKER_COIN]);
    TEST_ASSERT_EQUAL_INT(1, s.count[SOUND_FLICK]);
    TEST_ASSERT_TRUE(s.count[SOUND_STRIKER_COIN] >= 1);
    TEST_ASSERT_TRUE(s.max_speed[SOUND_STRIKER_COIN] > 0.5f);
}

void test_coin_hits_coin_and_walls(void) {
    BoardState b; empty_board(&b);
    coin(&b, 0, 0.0f, 0.0f);
    coin(&b, 1, 0.12f, 0.0f);
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &b, SEAT_SOUTH);
    physics_place_striker(pw, SEAT_SOUTH, (Vec2){-0.2f, 0.0f});
    physics_apply_shot(pw, 0.0f, 0.9f);
    Seen s = run(pw, 480);
    physics_destroy(pw);
    printf("SOUNDS coin_coin=%d coin_wall=%d\n", s.count[SOUND_COIN_COIN], s.count[SOUND_COIN_WALL]);
    TEST_ASSERT_TRUE(s.count[SOUND_COIN_COIN] >= 1);
    TEST_ASSERT_TRUE(s.count[SOUND_COIN_WALL] >= 1);
}

void test_striker_hits_wall(void) {
    BoardState b; empty_board(&b);
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &b, SEAT_SOUTH);
    physics_place_striker(pw, SEAT_SOUTH, (Vec2){0.2f, 0.0f});
    physics_apply_shot(pw, 0.0f, 0.8f);
    Seen s = run(pw, 240);
    physics_destroy(pw);
    printf("SOUNDS striker_wall=%d (%.2f u/s)\n", s.count[SOUND_STRIKER_WALL], s.max_speed[SOUND_STRIKER_WALL]);
    TEST_ASSERT_TRUE(s.count[SOUND_STRIKER_WALL] >= 1);
    TEST_ASSERT_EQUAL_INT(0, s.count[SOUND_STRIKER_COIN]);
}

void test_pocket_sounds_for_coin_and_striker(void) {
    BoardState b; empty_board(&b);
    coin(&b, 0, POCKET_CENTERS[3].x, POCKET_CENTERS[3].y);
    PhysicsWorld* pw = physics_create();
    physics_sync_from_board(pw, &b, SEAT_SOUTH);
    physics_place_striker(pw, SEAT_SOUTH, POCKET_CENTERS[2]);
    Seen s = run(pw, 10);
    physics_destroy(pw);
    TEST_ASSERT_EQUAL_INT(1, s.count[SOUND_COIN_POCKET]);
    TEST_ASSERT_EQUAL_INT(1, s.count[SOUND_STRIKER_POCKET]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_flick_and_striker_hits_coin);
    RUN_TEST(test_coin_hits_coin_and_walls);
    RUN_TEST(test_striker_hits_wall);
    RUN_TEST(test_pocket_sounds_for_coin_and_striker);
    return UNITY_END();
}
