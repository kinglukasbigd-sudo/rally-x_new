#include "TestFramework.h"
#include "gameplay/LifeSystem.h"
#include "core/Types.h"

using namespace rx;

namespace {
constexpr int M1 = 20000, M2 = 60000, M3 = 110000;
}

TEST(a_run_starts_with_three_cars_and_nothing_claimed) {
    LifeSystem l;
    l.reset(START_LIVES);
    CHECK_EQ(l.lives(), 3);
    CHECK_EQ(l.bonusesAwarded(), 0);
    for (int i = 0; i < LifeSystem::BONUS_LIFE_COUNT; ++i) CHECK(!l.bonusClaimed(i));
}

TEST(the_three_milestones_each_add_a_car) {
    LifeSystem l;
    l.reset(3);
    CHECK_EQ(l.checkBonusLife(19999), 0);  CHECK_EQ(l.lives(), 3);
    CHECK_EQ(l.checkBonusLife(M1),     1);  CHECK_EQ(l.lives(), 4);
    CHECK_EQ(l.checkBonusLife(M2),     1);  CHECK_EQ(l.lives(), 5);
    CHECK_EQ(l.checkBonusLife(M3),     1);  CHECK_EQ(l.lives(), 6);
    CHECK_EQ(l.bonusesAwarded(), 3);
}

TEST(sitting_above_a_milestone_never_pays_twice) {
    LifeSystem l;
    l.reset(3);
    l.checkBonusLife(M1);
    CHECK_EQ(l.lives(), 4);

    // Every score between the first and second milestone, one after another:
    // not one of them may add a car.
    for (int score = M1; score < M2; score += 137) {
        CHECK_EQ(l.checkBonusLife(score), 0);
        CHECK_EQ(l.lives(), 4);
    }
    CHECK_EQ(l.checkBonusLife(M2), 1);
    CHECK_EQ(l.lives(), 5);
    for (int score = M2; score < M3; score += 311) {
        CHECK_EQ(l.checkBonusLife(score), 0);
        CHECK_EQ(l.lives(), 5);
    }
}

// --- crossing, not landing on ---------------------------------------------

TEST(a_score_that_jumps_over_a_milestone_still_pays) {
    // The case that a plain equality test gets wrong: the score never once
    // reads 20,000, because a bonus took it straight past.
    struct Case { int before, after, milestone; };
    const Case cases[] = { { 19950,  20150, M1 },
                           { 59900,  60100, M2 },
                           { 109900, 110200, M3 } };

    for (const auto& c : cases) {
        LifeSystem l;
        l.reset(3);
        // Wind the earlier milestones out of the way so each case is tested
        // on its own.
        l.checkBonusLife(c.before);
        const int lives = l.lives();

        CHECK_EQ(l.checkBonusLife(c.after), 1);
        CHECK_EQ(l.lives(), lives + 1);
        CHECK(c.after > c.milestone && c.before < c.milestone);
    }
}

TEST(a_single_jump_over_two_milestones_pays_both) {
    LifeSystem l;
    l.reset(3);
    // A lucky flag on a big multiplier really can do this.
    CHECK_EQ(l.checkBonusLife(65000), 2);
    CHECK_EQ(l.lives(), 5);
    CHECK(l.bonusClaimed(0));
    CHECK(l.bonusClaimed(1));
    CHECK(!l.bonusClaimed(2));
    // ...and neither pays again afterwards.
    CHECK_EQ(l.checkBonusLife(109999), 0);
    CHECK_EQ(l.lives(), 5);
}

TEST(a_score_high_enough_to_clear_everything_pays_three_and_stops) {
    LifeSystem l;
    l.reset(3);
    CHECK_EQ(l.checkBonusLife(999999), 3);
    CHECK_EQ(l.lives(), 6);
    CHECK_EQ(l.checkBonusLife(999999), 0);
    CHECK_EQ(l.lives(), 6);
}

// --- crashing --------------------------------------------------------------

TEST(losing_a_car_does_not_put_a_claimed_milestone_back) {
    LifeSystem l;
    l.reset(3);

    l.checkBonusLife(M1);
    CHECK_EQ(l.lives(), 4);

    l.loseLife();
    CHECK_EQ(l.lives(), 3);
    // Still above 20,000, still not paying.
    CHECK_EQ(l.checkBonusLife(25000), 0);
    CHECK_EQ(l.lives(), 3);

    // The next milestone still works normally from wherever the lives are.
    CHECK_EQ(l.checkBonusLife(M2), 1);
    CHECK_EQ(l.lives(), 4);
}

TEST(a_milestone_car_is_a_real_car_and_can_be_spent) {
    LifeSystem l;
    l.reset(1);
    l.checkBonusLife(M1);
    CHECK_EQ(l.lives(), 2);
    CHECK(l.loseLife());        // the bonus car is the one keeping it alive
    CHECK_EQ(l.lives(), 1);
    CHECK(!l.gameOver());
    CHECK(!l.loseLife());
    CHECK(l.gameOver());
}

// --- a new run -------------------------------------------------------------

TEST(a_new_run_puts_every_milestone_back_on_the_table) {
    LifeSystem l;
    l.reset(3);
    l.checkBonusLife(999999);
    CHECK_EQ(l.lives(), 6);
    CHECK_EQ(l.bonusesAwarded(), 3);

    l.reset(START_LIVES);
    CHECK_EQ(l.lives(), 3);
    CHECK_EQ(l.bonusesAwarded(), 0);
    CHECK_EQ(l.checkBonusLife(M1), 1);      // and they all pay again
    CHECK_EQ(l.checkBonusLife(M2), 1);
    CHECK_EQ(l.checkBonusLife(M3), 1);
    CHECK_EQ(l.lives(), 6);
}

TEST(the_milestone_table_is_the_only_place_the_numbers_live) {
    // A guard on the configuration rather than the behaviour: if somebody
    // retunes the table, it must stay sorted and stay the length it claims.
    CHECK_EQ(LifeSystem::BONUS_LIFE_COUNT, 3);
    CHECK_EQ(LifeSystem::BONUS_LIFE_SCORES[0], 20000);
    CHECK_EQ(LifeSystem::BONUS_LIFE_SCORES[1], 60000);
    CHECK_EQ(LifeSystem::BONUS_LIFE_SCORES[2], 110000);
    for (int i = 1; i < LifeSystem::BONUS_LIFE_COUNT; ++i)
        CHECK(LifeSystem::BONUS_LIFE_SCORES[i] > LifeSystem::BONUS_LIFE_SCORES[i - 1]);
    CHECK_EQ(LifeSystem::BONUS_LIFE_SCORE, LifeSystem::BONUS_LIFE_SCORES[0]);
}
