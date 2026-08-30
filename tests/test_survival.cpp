#include "TestFramework.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "gameplay/FuelSystem.h"
#include "gameplay/SmokeSystem.h"
#include "gameplay/LifeSystem.h"
#include "gameplay/LuckyFlagBonusCalculator.h"
#include "core/InputManager.h"
#include "world/LevelLoader.h"

using namespace rx;

namespace {

const char* kRockRun =
    "name ROCKRUN\ntype NORMAL\nfuel 100\nfuelDrain 1\nplayerSpeed 2\nmaze\n"
    "##########\n"
    "#P...R..F#\n"
    "##########\n";

const char* kLuckyRun =
    "name LUCKY\ntype NORMAL\nfuel 100\nfuelDrain 1\nplayerSpeed 2\nmaze\n"
    "##########\n"
    "#P..L...F#\n"
    "##########\n";

LevelData parse(const char* text) {
    LevelData d;
    CHECK(LevelLoader::loadFromString(text, d));
    return d;
}

void hold(InputManager& in, Action a, bool smoke = false) {
    for (Action x : { Action::Up, Action::Down, Action::Left, Action::Right })
        in.setFromExternal(x, x == a);
    in.setFromExternal(Action::Smoke, smoke);
}

} // namespace

TEST(fuel_drains_at_the_configured_rate) {
    FuelSystem f;
    f.reset(100.f, 2.f);
    for (int i = 0; i < 60; ++i) f.update(static_cast<float>(FIXED_DT));
    CHECK_NEAR(f.fuel(), 98.0, 0.05);          // 2 units in one second
    CHECK(!f.empty());
}

TEST(fuel_stops_at_zero_and_reports_empty) {
    FuelSystem f;
    f.reset(1.f, 10.f);
    for (int i = 0; i < 120; ++i) f.update(static_cast<float>(FIXED_DT));
    CHECK_NEAR(f.fuel(), 0.0, 1e-5);
    CHECK(f.empty());
}

TEST(infinite_fuel_debug_flag_holds_the_tank_full) {
    FuelSystem f;
    f.reset(50.f, 100.f);
    f.setInfinite(true);
    for (int i = 0; i < 120; ++i) f.update(static_cast<float>(FIXED_DT));
    CHECK_NEAR(f.fuel(), 50.0, 1e-5);
    CHECK(f.consume(999.f));
}

TEST(running_out_of_fuel_kills_the_player) {
    LevelData d = parse(kRockRun);
    d.fuel = 0.5f;
    d.fuelDrain = 10.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in; hold(in, Action::Up);       // sit still against a wall
    Round::Events ev;
    for (int i = 0; i < 120 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.playerDied);
    CHECK(ev.cause == DeathCause::OutOfFuel);
    CHECK(!r.player().alive());
}

TEST(hitting_a_rock_kills_the_player) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kRockRun));
    CHECK_EQ(static_cast<int>(r.rocks().size()), 1);

    InputManager in; hold(in, Action::Right);
    Round::Events ev;
    for (int i = 0; i < 200 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.playerDied);
    CHECK(ev.cause == DeathCause::Rock);
    CHECK_EQ(r.flagsCollected(), 0);             // died before the flag
}

TEST(smoke_costs_fuel_and_leaves_puffs_behind_the_car) {
    Round r; ScoreSystem s; s.newGame();
    LevelData d = parse(kRockRun);
    d.fuelDrain = 0.f;                            // isolate the smoke cost
    r.load(d);

    InputManager in; hold(in, Action::Up, /*smoke=*/true);
    for (int i = 0; i < 30; ++i) r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(r.smoke().activeCount() > 0);
    // Exactly one burst was paid for, no matter that the button stayed down.
    CHECK_NEAR(r.fuel().fuel(),
               r.fuel().capacity() - 3 * SmokeSystem::FUEL_PER_PUFF, 0.01);
}

TEST(holding_the_smoke_button_cannot_drain_the_tank) {
    Round r; ScoreSystem s; s.newGame();
    LevelData d = parse(kRockRun);
    d.fuelDrain = 0.f;
    r.load(d);

    // Hold it down for a solid ten seconds: the old behaviour emptied the
    // tank, the burst behaviour costs three puffs and stops.
    InputManager in; hold(in, Action::Up, /*smoke=*/true);
    for (int i = 0; i < 600; ++i) r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK_NEAR(r.fuel().fuel(),
               r.fuel().capacity() - 3 * SmokeSystem::FUEL_PER_PUFF, 0.01);
    CHECK(r.fuel().fraction() > 0.9f);
}

TEST(smoke_puffs_expire) {
    SmokeSystem sm; FuelSystem f;
    f.reset(100.f, 0.f);
    sm.emit(Vec2{10.f, 10.f}, f, true, 1.f);
    CHECK_EQ(sm.activeCount(), 1);
    CHECK(sm.contains(Vec2{12.f, 12.f}));

    for (int i = 0; i < 200; ++i) sm.update(static_cast<float>(FIXED_DT));
    CHECK_EQ(sm.activeCount(), 0);
    CHECK(!sm.contains(Vec2{12.f, 12.f}));
}

TEST(smoke_cannot_be_laid_without_fuel) {
    SmokeSystem sm; FuelSystem f;
    f.reset(0.5f, 0.f);                            // less than one puff costs
    CHECK(!sm.emit(Vec2{10.f, 10.f}, f, true, 1.f));
    CHECK_EQ(sm.activeCount(), 0);
}

TEST(one_press_lays_exactly_three_puffs_however_long_it_is_held) {
    SmokeSystem sm; FuelSystem f;
    f.reset(1000.f, 0.f);

    int puffs = 0;
    for (int i = 0; i < 600; ++i)                  // ten seconds of holding
        if (sm.emit(Vec2{10.f, 10.f}, f, true, static_cast<float>(FIXED_DT))) ++puffs;

    CHECK_EQ(puffs, SmokeSystem::PUFFS_PER_BURST);
    CHECK(!sm.bursting());
    // And the tank is barely touched: one burst, not a whole tank.
    CHECK_NEAR(f.fuel(), 1000.f - 3 * SmokeSystem::FUEL_PER_PUFF, 0.01);
}

TEST(the_button_must_be_released_before_it_fires_again) {
    SmokeSystem sm; FuelSystem f;
    f.reset(1000.f, 0.f);
    auto step = [&](bool held) { return sm.emit(Vec2{10.f, 10.f}, f, held,
                                                static_cast<float>(FIXED_DT)); };

    int first = 0;
    for (int i = 0; i < 120; ++i) if (step(true)) ++first;
    CHECK_EQ(first, 3);

    for (int i = 0; i < 10; ++i) step(false);      // let go

    int second = 0;
    for (int i = 0; i < 120; ++i) if (step(true)) ++second;
    CHECK_EQ(second, 3);                            // a second press, a second burst
}

TEST(a_burst_trails_out_behind_the_car_rather_than_all_at_once) {
    SmokeSystem sm; FuelSystem f;
    f.reset(1000.f, 0.f);
    // The three puffs are spaced, so they lay a short trail instead of
    // stacking on one spot.
    int firstStep = -1, lastStep = -1, puffs = 0;
    for (int i = 0; i < 120; ++i) {
        if (sm.emit(Vec2{10.f, 10.f}, f, true, static_cast<float>(FIXED_DT))) {
            if (firstStep < 0) firstStep = i;
            lastStep = i;
            ++puffs;
        }
    }
    CHECK_EQ(puffs, 3);
    CHECK(lastStep - firstStep >= 10);              // spread over ~0.2s, not one frame
}

TEST(a_burst_stops_early_when_the_tank_runs_dry) {
    SmokeSystem sm; FuelSystem f;
    f.reset(SmokeSystem::FUEL_PER_PUFF * 1.5f, 0.f);   // enough for one puff only
    int puffs = 0;
    for (int i = 0; i < 120; ++i)
        if (sm.emit(Vec2{10.f, 10.f}, f, true, static_cast<float>(FIXED_DT))) ++puffs;
    CHECK_EQ(puffs, 1);
    CHECK(!sm.bursting());
}

TEST(lucky_flag_pays_more_with_a_fuller_tank) {
    const int full = LuckyFlagBonusCalculator::bonusFor(100.f, 100.f);
    const int half = LuckyFlagBonusCalculator::bonusFor(50.f, 100.f);
    const int dry  = LuckyFlagBonusCalculator::bonusFor(0.f, 100.f);
    CHECK_EQ(full, LuckyFlagBonusCalculator::MAX_BONUS);
    CHECK_EQ(dry,  LuckyFlagBonusCalculator::MIN_BONUS);
    CHECK(half > dry && half < full);
    CHECK_EQ(full % LuckyFlagBonusCalculator::STEP, 0);
    CHECK_EQ(half % LuckyFlagBonusCalculator::STEP, 0);
}

TEST(collecting_the_lucky_flag_awards_a_fuel_bonus) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kLuckyRun));

    InputManager in; hold(in, Action::Right);
    Round::Events ev; int bonus = 0;
    for (int i = 0; i < 200; ++i) {
        ev = r.update(in, s, static_cast<float>(FIXED_DT));
        if (ev.bonusAwarded) bonus = ev.bonusAwarded;
    }
    CHECK(bonus >= LuckyFlagBonusCalculator::MIN_BONUS);
    CHECK_EQ(s.score(), bonus + 100);        // lucky bonus + the one normal flag
}

TEST(lives_run_out_and_then_the_game_is_over) {
    LifeSystem l;
    l.reset(3);
    CHECK_EQ(l.lives(), 3);
    CHECK(l.loseLife());  CHECK_EQ(l.lives(), 2);
    CHECK(l.loseLife());  CHECK_EQ(l.lives(), 1);
    CHECK(!l.loseLife()); CHECK_EQ(l.lives(), 0);
    CHECK(l.gameOver());
}

TEST(the_bonus_car_is_awarded_once) {
    LifeSystem l;
    l.reset(3);
    CHECK(!l.checkBonusLife(LifeSystem::BONUS_LIFE_SCORE - 1));
    CHECK_EQ(l.lives(), 3);
    CHECK(l.checkBonusLife(LifeSystem::BONUS_LIFE_SCORE));
    CHECK_EQ(l.lives(), 4);
    CHECK(!l.checkBonusLife(999999));
    CHECK_EQ(l.lives(), 4);
}

TEST(restarting_a_round_refills_the_tank_and_clears_the_smoke) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kRockRun));

    InputManager in; hold(in, Action::Up, true);
    for (int i = 0; i < 60; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(r.fuel().fuel() < r.fuel().capacity());

    r.restart();
    CHECK_NEAR(r.fuel().fuel(), r.fuel().capacity(), 1e-5);
    CHECK_EQ(r.smoke().activeCount(), 0);
    CHECK(r.player().alive());
}

// ---------------------------------------------------------------------------
// A lost life costs a car, not the round's flag progress.
// ---------------------------------------------------------------------------

TEST(flags_already_collected_survive_a_lost_life) {
    Round r; ScoreSystem s; s.newGame(); s.newRound();
    r.load(parse(kLuckyRun));               // "#P..L...F#" -- one normal flag

    InputManager in; hold(in, Action::Right);
    for (int i = 0; i < 200; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    const int banked = r.flagsCollected();
    const int score  = s.score();
    CHECK(banked > 0);

    r.restartAfterDeath();

    CHECK_EQ(r.flagsCollected(), banked);           // progress kept
    CHECK_EQ(r.flagsRemaining(), r.flagsRequired() - banked);
    CHECK_EQ(s.score(), score);                     // and the score with it

    int stillCollected = 0;
    for (const auto& f : r.flags()) if (f.collected) ++stillCollected;
    CHECK(stillCollected >= banked);
}

TEST(a_lost_life_still_returns_the_car_and_refills_the_tank) {
    Round r; ScoreSystem s; s.newGame();
    LevelData d = parse(kRockRun);
    r.load(d);

    InputManager in; hold(in, Action::Right);
    Round::Events ev;
    for (int i = 0; i < 300 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(ev.playerDied);

    r.restartAfterDeath();
    CHECK(r.player().alive());
    CHECK_NEAR(r.player().position().x, 1 * TILE + 8.f, 0.001);
    CHECK_NEAR(r.fuel().fuel(), r.fuel().capacity(), 1e-4);
    CHECK_EQ(r.smoke().activeCount(), 0);
}

TEST(a_brand_new_round_does_put_every_flag_back) {
    Round r; ScoreSystem s; s.newGame(); s.newRound();
    r.load(parse(kLuckyRun));

    InputManager in; hold(in, Action::Right);
    for (int i = 0; i < 200; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(r.flagsCollected() > 0);

    r.restart();                                    // a new round, not a new car
    CHECK_EQ(r.flagsCollected(), 0);
    for (const auto& f : r.flags()) CHECK(!f.collected);
}
