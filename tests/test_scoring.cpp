#include "TestFramework.h"
#include "gameplay/ScoreSystem.h"

using namespace rx;

TEST(flag_values_climb_100_to_1000) {
    ScoreSystem s;
    s.newGame(); s.newRound();
    const int expect[10] = {100,200,300,400,500,600,700,800,900,1000};
    int total = 0;
    for (int i = 0; i < 10; ++i) {
        const int got = s.awardNormalFlag();
        CHECK_EQ(got, expect[i]);
        total += expect[i];
    }
    CHECK_EQ(s.score(), total);          // 5500 for a perfect round
    CHECK_EQ(s.score(), 5500);
}

TEST(flag_value_is_capped_at_the_tenth_step) {
    ScoreSystem s; s.newGame(); s.newRound();
    for (int i = 0; i < 10; ++i) s.awardNormalFlag();
    CHECK_EQ(s.awardNormalFlag(), 1000);
}

TEST(the_sequence_restarts_each_round) {
    ScoreSystem s; s.newGame(); s.newRound();
    s.awardNormalFlag(); s.awardNormalFlag();
    s.newRound();
    CHECK_EQ(s.awardNormalFlag(), 100);
}

TEST(special_flag_doubles_everything_after_it) {
    ScoreSystem s; s.newGame(); s.newRound();
    CHECK_EQ(s.awardNormalFlag(), 100);
    s.activateSpecialFlag();
    CHECK_EQ(s.awardNormalFlag(), 400);   // 2nd flag = 200, doubled
    CHECK_EQ(s.awardNormalFlag(), 600);
    CHECK_EQ(s.multiplier(), 2);
}

TEST(losing_a_life_ends_the_special_flag_effect) {
    ScoreSystem s; s.newGame(); s.newRound();
    s.activateSpecialFlag();
    CHECK_EQ(s.multiplier(), 2);
    s.onPlayerDeath();
    CHECK_EQ(s.multiplier(), 1);
    CHECK_EQ(s.awardNormalFlag(), 100);
}

TEST(high_score_tracks_the_best_run) {
    ScoreSystem s; s.newGame();
    s.addBonus(4200);
    CHECK_EQ(s.highScore(), 4200);
    s.newGame();
    CHECK_EQ(s.score(), 0);
    CHECK_EQ(s.highScore(), 4200);
    s.addBonus(100);
    CHECK_EQ(s.highScore(), 4200);
}
