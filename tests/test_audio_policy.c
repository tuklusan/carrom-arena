#include "unity.h"
#include "audio/audio_policy.h"
#include "physics/physics.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_every_physics_sound_kind_has_its_own_cue(void) {
    TEST_ASSERT_EQUAL_INT(CUE_FLICK, audio_cue_for_sound_kind(SOUND_FLICK));
    TEST_ASSERT_EQUAL_INT(CUE_STRIKER_COIN, audio_cue_for_sound_kind(SOUND_STRIKER_COIN));
    TEST_ASSERT_EQUAL_INT(CUE_COIN_COIN, audio_cue_for_sound_kind(SOUND_COIN_COIN));
    TEST_ASSERT_EQUAL_INT(CUE_STRIKER_WALL, audio_cue_for_sound_kind(SOUND_STRIKER_WALL));
    TEST_ASSERT_EQUAL_INT(CUE_COIN_WALL, audio_cue_for_sound_kind(SOUND_COIN_WALL));
    TEST_ASSERT_EQUAL_INT(CUE_STRIKER_POCKET, audio_cue_for_sound_kind(SOUND_STRIKER_POCKET));
    TEST_ASSERT_EQUAL_INT(CUE_COIN_POCKET, audio_cue_for_sound_kind(SOUND_COIN_POCKET));
}

void test_harder_hits_are_louder_and_volume_is_bounded(void) {
    for (int c = 0; c < CUE_COUNT; c++) {
        float soft = audio_policy_loudness((AudioCue)c, 0.15f);
        float hard = audio_policy_loudness((AudioCue)c, 4.0f);
        TEST_ASSERT_TRUE(hard >= soft);
        TEST_ASSERT_TRUE(soft > 0.0f && hard <= 1.0f);
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, audio_policy_loudness(CUE_COIN_COIN, 50.0f));   /* clamped */
}

void test_impacts_too_soft_to_hear_are_skipped(void) {
    AudioPolicy p; audio_policy_init(&p);
    float v; int var;
    TEST_ASSERT_FALSE(audio_policy_admit(&p, CUE_COIN_COIN, 0.02f, 1.0, 4, &v, &var));
    TEST_ASSERT_TRUE(audio_policy_admit(&p, CUE_COIN_COIN, 0.5f, 1.0, 4, &v, &var));
}

void test_rate_limit_drops_a_machine_gun_but_lets_later_hits_through(void) {
    AudioPolicy p; audio_policy_init(&p);
    float v; int var;
    TEST_ASSERT_TRUE(audio_policy_admit(&p, CUE_COIN_COIN, 1.0f, 10.000, 4, &v, &var));
    TEST_ASSERT_FALSE(audio_policy_admit(&p, CUE_COIN_COIN, 1.0f, 10.010, 4, &v, &var));   /* 10 ms later */
    TEST_ASSERT_TRUE(audio_policy_admit(&p, CUE_COIN_COIN, 1.0f, 10.200, 4, &v, &var));
    /* a different cue is independent */
    TEST_ASSERT_TRUE(audio_policy_admit(&p, CUE_COIN_WALL, 1.0f, 10.011, 3, &v, &var));
}

void test_variants_rotate_and_stay_in_range(void) {
    AudioPolicy p; audio_policy_init(&p);
    float v; int var, seen[4] = {0, 0, 0, 0};
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_TRUE(audio_policy_admit(&p, CUE_STRIKER_COIN, 2.0f, 1.0 + i, 4, &v, &var));
        TEST_ASSERT_TRUE(var >= 0 && var < 4);
        seen[var]++;
    }
    for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_INT(2, seen[i]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_every_physics_sound_kind_has_its_own_cue);
    RUN_TEST(test_harder_hits_are_louder_and_volume_is_bounded);
    RUN_TEST(test_impacts_too_soft_to_hear_are_skipped);
    RUN_TEST(test_rate_limit_drops_a_machine_gun_but_lets_later_hits_through);
    RUN_TEST(test_variants_rotate_and_stay_in_range);
    return UNITY_END();
}
