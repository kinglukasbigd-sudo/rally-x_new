#include "TestFramework.h"
#include "core/Game.h"
#include "gameplay/ChallengeStage.h"
#include "gameplay/Round.h"
#include "gameplay/SmokeSystem.h"
#include "gameplay/ScoreSystem.h"
#include "core/InputManager.h"
#include "world/LevelLoader.h"
#include "core/TouchControls.h"
#include <SDL.h>
#include <cstdlib>

using namespace rx;

namespace {

// The whole state machine runs headless against SDL's dummy video driver, so
// these are the real transitions the shipped build takes.
struct HeadlessGame {
    Game g;
    bool ok = false;
    explicit HeadlessGame(bool touch = false,
                          TouchScheme scheme = TouchScheme::Swipe) {
        setenv("SDL_VIDEODRIVER", "dummy", 1);
        setenv("SDL_AUDIODRIVER", "dummy", 1);
        ok = g.init(1, "levels", 1, false, touch, scheme);
    }
    ~HeadlessGame() { if (ok) g.shutdown(); }

    void run(int steps) {
        for (int i = 0; i < steps; ++i) {
            g.input().beginFrame();
            g.step(static_cast<float>(FIXED_DT));
        }
    }
    void press(Action a) {
        g.input().beginFrame();
        g.input().setFromExternal(a, true);
        g.step(static_cast<float>(FIXED_DT));
        g.input().beginFrame();
        g.input().setFromExternal(a, false);
        g.step(static_cast<float>(FIXED_DT));
    }
    // Advance until `s` is reached or the budget runs out.
    bool waitFor(GameState s, int maxSteps) {
        for (int i = 0; i < maxSteps; ++i) {
            if (g.state() == s) return true;
            run(1);
        }
        return g.state() == s;
    }
};

} // namespace

TEST(the_game_boots_to_the_start_screen) {
    HeadlessGame h;
    CHECK(h.ok);
    CHECK(h.g.state() == GameState::StartScreen);
}

TEST(pressing_start_enters_the_first_round) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.g.state() == GameState::Ready);
    CHECK_EQ(h.g.roundNumber(), 1);
    CHECK_EQ(h.g.score().score(), 0);
    CHECK_EQ(h.g.lifeSystem().lives(), START_LIVES);

    CHECK(h.waitFor(GameState::Playing, 300));
}

TEST(dying_costs_a_car_and_restarts_the_same_round) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    // Stand still: the pack will run the car down.
    CHECK(h.waitFor(GameState::PlayerDeath, 60 * 90));
    CHECK_EQ(h.g.lifeSystem().lives(), START_LIVES);   // not deducted until the count-out

    CHECK(h.waitFor(GameState::Ready, 60 * 5));
    CHECK_EQ(h.g.lifeSystem().lives(), START_LIVES - 1);
    CHECK_EQ(h.g.roundNumber(), 1);                    // same round again
}

TEST(running_out_of_cars_ends_the_game_and_returns_to_the_title) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);

    for (int life = 0; life < START_LIVES; ++life) {
        CHECK(h.waitFor(GameState::Playing, 60 * 10));
        CHECK(h.waitFor(GameState::PlayerDeath, 60 * 90));
        h.run(60 * 3);                                  // let the count-out finish
    }
    CHECK(h.g.state() == GameState::GameOver);
    CHECK(h.g.lifeSystem().gameOver());

    CHECK(h.waitFor(GameState::StartScreen, 60 * 6));
}

TEST(clearing_a_round_advances_to_the_next_one) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));
    CHECK_EQ(h.g.roundNumber(), 1);

    h.g.debugCollectAllFlags();          // bank all ten flags
    h.run(1);
    CHECK(h.g.state() == GameState::RoundComplete);
    CHECK_EQ(h.g.score().score(), 5500); // 100+200+...+1000

    CHECK(h.waitFor(GameState::Ready, 60 * 5));
    CHECK_EQ(h.g.roundNumber(), 2);
    CHECK_EQ(h.g.lifeSystem().lives(), START_LIVES);   // clearing costs nothing
}

TEST(round_three_is_the_challenging_stage) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);

    for (int round = 1; round <= 2; ++round) {
        CHECK(h.waitFor(GameState::Playing, 60 * 10));
        h.g.debugCollectAllFlags();
        h.run(1);
        CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
    CHECK_EQ(h.g.roundNumber(), 3);
    CHECK(h.g.round().isChallenge());
    CHECK_EQ(static_cast<int>(h.g.round().enemies().size()), 0);

    CHECK(h.waitFor(GameState::ChallengingStage, 60 * 5));

    const int before = h.g.score().score();
    h.g.debugCollectAllFlags();
    h.run(1);
    CHECK(h.g.state() == GameState::RoundComplete);
    // Flag scores plus the perfect bonus.
    CHECK_EQ(h.g.score().score(), before + 5500 + ChallengeStage::PERFECT_BONUS);
    CHECK_EQ(h.g.lifeSystem().lives(), START_LIVES);
}

TEST(a_challenge_stage_never_costs_a_car) {
    ChallengeStage cs;
    ScoreSystem score; score.newGame();
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level03.lvl", d));
    Round r; r.load(d);

    cs.begin();
    CHECK(cs.running());
    const auto out = cs.update(r, /*playerDied=*/true, /*roundComplete=*/false, score);
    CHECK(out.finished);
    CHECK(!out.perfect);
    CHECK_EQ(out.bonus, 0);
    CHECK_EQ(score.score(), 0);
    CHECK(!ChallengeStage::costsALife());
}

TEST(clearing_a_challenge_stage_pays_the_perfect_bonus) {
    ChallengeStage cs;
    ScoreSystem score; score.newGame();
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level03.lvl", d));
    Round r; r.load(d);

    cs.begin();
    const auto out = cs.update(r, false, /*roundComplete=*/true, score);
    CHECK(out.finished);
    CHECK(out.perfect);
    CHECK_EQ(out.bonus, ChallengeStage::PERFECT_BONUS);
    CHECK_EQ(score.score(), ChallengeStage::PERFECT_BONUS);
}

TEST(a_finished_challenge_stage_stops_reporting) {
    ChallengeStage cs;
    ScoreSystem score; score.newGame();
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level03.lvl", d));
    Round r; r.load(d);

    cs.begin();
    cs.update(r, false, true, score);
    CHECK(!cs.running());
    const auto again = cs.update(r, false, true, score);
    CHECK(!again.finished);
    CHECK_EQ(score.score(), ChallengeStage::PERFECT_BONUS);   // paid exactly once
}

TEST(every_shipped_round_can_be_loaded_in_sequence) {
    HeadlessGame h; CHECK(h.ok);
    // Rounds cycle past the authored set with a stiffer difficulty each lap.
    LevelData a, b;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", a));
    CHECK(LevelLoader::loadFromFile("levels/level05.lvl", b));
    CHECK(a.map.width() == b.map.width());
    CHECK(b.enemySpeed > a.enemySpeed);
    CHECK(b.enemyCount > a.enemyCount);
}

// ---------------------------------------------------------------------------
// Background music: normal rounds only.
// ---------------------------------------------------------------------------

TEST(both_music_loops_are_loaded_at_start_up) {
    HeadlessGame h; CHECK(h.ok);
    CHECK(h.g.audio().musicLoaded(MusicTrack::Normal));
    CHECK(h.g.audio().musicLoaded(MusicTrack::Challenge));
    // Nothing plays on the title screen.
    CHECK(!h.g.audio().musicPlaying());
}

TEST(music_plays_during_a_normal_round_and_nowhere_else) {
    HeadlessGame h; CHECK(h.ok);
    CHECK(h.g.audio().musicLoaded(MusicTrack::Normal));

    h.press(Action::Start);
    CHECK(h.g.state() == GameState::Ready);
    CHECK(!h.g.audio().musicPlaying());        // silent on the READY card

    CHECK(h.waitFor(GameState::Playing, 300));
    CHECK(h.g.audio().musicPlaying());         // and running once play begins
    CHECK(h.g.audio().currentTrack() == MusicTrack::Normal);

    // It stops the moment the round stops, whatever ended it.
    h.g.debugCollectAllFlags();
    h.run(1);
    CHECK(h.g.state() == GameState::RoundComplete);
    CHECK(!h.g.audio().musicPlaying());
}

TEST(music_stops_when_the_player_dies) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));
    CHECK(h.g.audio().musicPlaying());

    CHECK(h.waitFor(GameState::PlayerDeath, 60 * 90));
    CHECK(!h.g.audio().musicPlaying());
}

TEST(a_challenging_stage_plays_its_own_track) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);

    for (int round = 1; round <= 2; ++round) {
        CHECK(h.waitFor(GameState::Playing, 60 * 10));
        CHECK(h.g.audio().currentTrack() == MusicTrack::Normal);
        h.g.debugCollectAllFlags();
        h.run(1);
        CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
    CHECK_EQ(h.g.roundNumber(), 3);
    CHECK(h.g.round().isChallenge());

    CHECK(h.waitFor(GameState::ChallengingStage, 60 * 5));
    CHECK(h.g.audio().musicPlaying());
    CHECK(h.g.audio().currentTrack() == MusicTrack::Challenge);
}

TEST(music_comes_back_for_the_round_after_a_challenging_stage) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);

    for (int round = 1; round <= 3; ++round) {
        const bool challenge = h.g.round().isChallenge();
        CHECK(h.waitFor(challenge ? GameState::ChallengingStage : GameState::Playing, 60 * 10));
        h.g.debugCollectAllFlags();
        h.run(1);
        CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
    CHECK_EQ(h.g.roundNumber(), 4);
    CHECK(h.waitFor(GameState::Playing, 60 * 5));
    CHECK(h.g.audio().musicPlaying());
    // Back to the ordinary track, not still on the challenge one.
    CHECK(h.g.audio().currentTrack() == MusicTrack::Normal);
}

TEST(the_track_swaps_cleanly_between_round_kinds) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);

    // Round 1 normal -> round 3 challenge -> round 4 normal, checking the
    // track each time and that it always stops in between.
    const MusicTrack expect[4] = { MusicTrack::Normal, MusicTrack::Normal,
                                   MusicTrack::Challenge, MusicTrack::Normal };
    for (int round = 1; round <= 4; ++round) {
        const bool challenge = h.g.round().isChallenge();
        CHECK(h.waitFor(challenge ? GameState::ChallengingStage : GameState::Playing, 60 * 10));
        CHECK(h.g.audio().musicPlaying());
        CHECK(h.g.audio().currentTrack() == expect[round - 1]);

        h.g.debugCollectAllFlags();
        h.run(1);
        CHECK(h.g.state() == GameState::RoundComplete);
        CHECK(!h.g.audio().musicPlaying());     // silent between rounds
        if (round < 4) CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
}

// ---------------------------------------------------------------------------
// Swipe mode: a tap on the screen is the smoke button.
//
// There is no button to press in swipe mode, so tapping has to be it.  These
// push real SDL touch events through Game::pumpInput, so they exercise the
// same path a finger does.
// ---------------------------------------------------------------------------

namespace {

void pushFinger(uint32_t type, int64_t finger, float nx, float ny) {
    SDL_Event e{};
    e.type = type;
    e.tfinger.type     = type;
    e.tfinger.timestamp= SDL_GetTicks();
    e.tfinger.touchId  = 1;
    e.tfinger.fingerId = finger;
    e.tfinger.x = nx;
    e.tfinger.y = ny;
    e.tfinger.pressure = 1.0f;
    SDL_PushEvent(&e);
}

// A press and release that does not move: the smoke gesture.
void tapScreen(HeadlessGame& h, float nx = 0.5f, float ny = 0.5f) {
    pushFinger(SDL_FINGERDOWN, 1, nx, ny);
    h.g.pumpInput();
    h.g.step(static_cast<float>(FIXED_DT));
    pushFinger(SDL_FINGERUP, 1, nx, ny);
    h.g.pumpInput();
    h.g.step(static_cast<float>(FIXED_DT));
}

void idle(HeadlessGame& h, int steps) {
    for (int i = 0; i < steps; ++i) {
        h.g.pumpInput();
        h.g.step(static_cast<float>(FIXED_DT));
    }
}

} // namespace

TEST(a_tap_in_swipe_mode_fires_a_smoke_burst) {
    HeadlessGame h(/*touch=*/true, TouchScheme::Swipe);
    CHECK(h.ok);

    tapScreen(h);                                   // tap to start
    for (int i = 0; i < 300 && h.g.state() != GameState::Playing; ++i) idle(h, 1);
    CHECK(h.g.state() == GameState::Playing);

    const float before = h.g.round().fuel().fuel();
    tapScreen(h);                                   // and now: smoke
    idle(h, 40);
    const float used = before - h.g.round().fuel().fuel();

    // One burst of fuel, on top of whatever the engine burned in those frames.
    const float burst = SmokeSystem::PUFFS_PER_BURST * SmokeSystem::FUEL_PER_PUFF;
    CHECK(used >= burst * 0.95f);
    CHECK(h.g.round().smoke().activeCount() > 0);
}

TEST(every_tap_in_swipe_mode_fires_another_burst) {
    HeadlessGame h(/*touch=*/true, TouchScheme::Swipe);
    CHECK(h.ok);
    tapScreen(h);
    for (int i = 0; i < 300 && h.g.state() != GameState::Playing; ++i) idle(h, 1);
    CHECK(h.g.state() == GameState::Playing);

    const float burst = SmokeSystem::PUFFS_PER_BURST * SmokeSystem::FUEL_PER_PUFF;
    int fired = 0;
    for (int t = 0; t < 5; ++t) {
        const float before = h.g.round().fuel().fuel();
        tapScreen(h);
        idle(h, 40);
        if (before - h.g.round().fuel().fuel() >= burst * 0.95f) ++fired;
    }
    CHECK_EQ(fired, 5);      // not just the first one
}

TEST(a_flick_in_swipe_mode_steers_without_spending_fuel) {
    HeadlessGame h(/*touch=*/true, TouchScheme::Swipe);
    CHECK(h.ok);
    tapScreen(h);
    for (int i = 0; i < 300 && h.g.state() != GameState::Playing; ++i) idle(h, 1);
    CHECK(h.g.state() == GameState::Playing);

    const float before = h.g.round().fuel().fuel();
    const int   puffs  = h.g.round().smoke().activeCount();

    // A real swipe: down, well across the screen, up.
    pushFinger(SDL_FINGERDOWN, 2, 0.50f, 0.50f);
    h.g.pumpInput(); h.g.step(static_cast<float>(FIXED_DT));
    pushFinger(SDL_FINGERMOTION, 2, 0.80f, 0.50f);
    h.g.pumpInput(); h.g.step(static_cast<float>(FIXED_DT));
    pushFinger(SDL_FINGERUP, 2, 0.80f, 0.50f);
    h.g.pumpInput(); h.g.step(static_cast<float>(FIXED_DT));
    idle(h, 20);

    // Steering must not cost a burst -- otherwise every turn wastes fuel.
    const float burst = SmokeSystem::PUFFS_PER_BURST * SmokeSystem::FUEL_PER_PUFF;
    CHECK((before - h.g.round().fuel().fuel()) < burst * 0.5f);
    CHECK_EQ(h.g.round().smoke().activeCount(), puffs);
}

TEST(a_tap_on_the_title_screen_still_starts_the_game) {
    HeadlessGame h(/*touch=*/true, TouchScheme::Swipe);
    CHECK(h.ok);
    CHECK(h.g.state() == GameState::StartScreen);
    tapScreen(h);
    CHECK(h.g.state() == GameState::Ready);
}
