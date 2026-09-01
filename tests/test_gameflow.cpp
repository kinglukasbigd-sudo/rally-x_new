#include "TestFramework.h"
#include "core/Game.h"
#include "gameplay/ChallengeStage.h"
#include "gameplay/Round.h"
#include "gameplay/SmokeSystem.h"
#include "audio/AudioManager.h"
#include "gameplay/ScoreSystem.h"
#include "core/InputManager.h"
#include "world/LevelLoader.h"
#include "core/TouchControls.h"
#include <SDL.h>
#include <algorithm>
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

// ---------------------------------------------------------------------------
// Flags are re-scattered per round -- but never mid-round.
// ---------------------------------------------------------------------------

namespace {

// Where the flags currently are, as a comparable fingerprint.
std::vector<std::pair<int,int>> flagTiles(const Round& r) {
    std::vector<std::pair<int,int>> t;
    for (const auto& f : r.flags())
        t.push_back({ TileMap::toTile(f.pos.x), TileMap::toTile(f.pos.y) });
    std::sort(t.begin(), t.end());
    return t;
}

} // namespace

TEST(each_new_round_scatters_the_flags_somewhere_new) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    std::vector<std::vector<std::pair<int,int>>> seen;
    for (int round = 1; round <= 4; ++round) {
        const bool challenge = h.g.round().isChallenge();
        CHECK(h.waitFor(challenge ? GameState::ChallengingStage : GameState::Playing, 60 * 10));

        const auto tiles = flagTiles(h.g.round());
        CHECK_EQ(static_cast<int>(tiles.size()), FLAGS_PER_ROUND + 2);
        for (const auto& prior : seen) CHECK(tiles != prior);
        seen.push_back(tiles);

        h.g.debugCollectAllFlags();
        h.run(1);
        if (round < 4) CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
}

TEST(losing_a_life_leaves_the_flags_exactly_where_they_are) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    const auto before = flagTiles(h.g.round());

    CHECK(h.waitFor(GameState::PlayerDeath, 60 * 90));
    CHECK(h.waitFor(GameState::Ready, 60 * 5));

    // Same round, same layout: a lost car must not reshuffle the maze under
    // the player, and must not undo the flags already banked.
    CHECK_EQ(h.g.roundNumber(), 1);
    CHECK(flagTiles(h.g.round()) == before);
}

TEST(replaying_the_same_round_number_still_differs_between_games) {
    // Round 1 of one game and round 1 of the next must not be identical, or
    // the shuffle is only per-round and not per-game.
    HeadlessGame a; CHECK(a.ok);
    a.press(Action::Start);
    CHECK(a.waitFor(GameState::Playing, 300));
    const auto first = flagTiles(a.g.round());

    HeadlessGame b; CHECK(b.ok);
    b.press(Action::Start);
    CHECK(b.waitFor(GameState::Playing, 300));

    CHECK(flagTiles(b.g.round()) != first);
}

TEST(a_fixed_seed_replays_the_same_layout) {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    setenv("SDL_AUDIODRIVER", "dummy", 1);

    auto layoutWithSeed = [](uint32_t seed) {
        Game g;
        g.init(1, "levels", 1, false, false, TouchScheme::Swipe, seed);
        g.input().beginFrame();
        g.input().setFromExternal(Action::Start, true);
        g.step(static_cast<float>(FIXED_DT));
        for (int i = 0; i < 300 && g.state() != GameState::Playing; ++i) {
            g.input().beginFrame();
            g.step(static_cast<float>(FIXED_DT));
        }
        auto t = flagTiles(g.round());
        g.shutdown();
        return t;
    };

    const auto a = layoutWithSeed(20250831u);
    const auto b = layoutWithSeed(20250831u);
    const auto c = layoutWithSeed(11111111u);
    CHECK(a == b);        // --seed makes a game repeatable
    CHECK(a != c);        // a different seed lays out differently
}

// ---------------------------------------------------------------------------
// Music mute.
// ---------------------------------------------------------------------------

TEST(music_starts_unmuted_and_toggles) {
    HeadlessGame h; CHECK(h.ok);
    CHECK(!h.g.audio().musicMuted());
}

TEST(the_mute_toggle_flips_and_reports_the_new_state) {
    AudioManager a;
    CHECK(!a.musicMuted());
    CHECK(a.toggleMusicMute());        // returns the state it moved to
    CHECK(a.musicMuted());
    CHECK(!a.toggleMusicMute());
    CHECK(!a.musicMuted());
}

TEST(setting_the_same_mute_state_twice_is_harmless) {
    AudioManager a;
    a.setMusicMuted(true);
    a.setMusicMuted(true);
    CHECK(a.musicMuted());
    a.setMusicMuted(false);
    a.setMusicMuted(false);
    CHECK(!a.musicMuted());
}

TEST(muting_does_not_stop_the_music_playing_underneath) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));
    CHECK(h.g.audio().musicPlaying());

    h.press(Action::MuteMusic);
    CHECK(h.g.audio().musicMuted());
    // Still "playing", just silent: unmuting rejoins rather than restarts.
    CHECK(h.g.audio().musicPlaying());

    h.press(Action::MuteMusic);
    CHECK(!h.g.audio().musicMuted());
    CHECK(h.g.audio().musicPlaying());
}

TEST(mute_survives_the_switch_between_round_kinds) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    h.press(Action::MuteMusic);
    CHECK(h.g.audio().musicMuted());

    // Through a normal round, a challenging stage, and out the other side.
    for (int round = 1; round <= 3; ++round) {
        const bool challenge = h.g.round().isChallenge();
        CHECK(h.waitFor(challenge ? GameState::ChallengingStage : GameState::Playing, 60 * 10));
        CHECK(h.g.audio().musicMuted());       // never quietly turns itself back on
        h.g.debugCollectAllFlags();
        h.run(1);
        CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
    CHECK(h.waitFor(GameState::Playing, 60 * 5));
    CHECK(h.g.audio().musicMuted());
    CHECK(h.g.audio().currentTrack() == MusicTrack::Normal);
}

TEST(mute_survives_losing_a_life_and_a_new_game) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));
    h.press(Action::MuteMusic);

    CHECK(h.waitFor(GameState::PlayerDeath, 60 * 90));
    CHECK(h.g.audio().musicMuted());
    CHECK(h.waitFor(GameState::Ready, 60 * 5));
    CHECK(h.g.audio().musicMuted());
}

// ---------------------------------------------------------------------------
// Sound-effect mute, and pause.
// ---------------------------------------------------------------------------

TEST(sound_effects_mute_independently_of_the_music) {
    AudioManager a;
    CHECK(!a.sfxMuted());
    CHECK(!a.musicMuted());

    CHECK(a.toggleSfxMute());
    CHECK(a.sfxMuted());
    CHECK(!a.musicMuted());        // the music is left alone

    a.toggleMusicMute();
    CHECK(a.musicMuted());
    CHECK(a.sfxMuted());           // and muting music leaves effects alone

    CHECK(!a.toggleSfxMute());
    CHECK(!a.sfxMuted());
    CHECK(a.musicMuted());
}

TEST(the_sound_key_toggles_effects_in_game) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    CHECK(!h.g.audio().sfxMuted());
    h.press(Action::MuteSfx);
    CHECK(h.g.audio().sfxMuted());
    CHECK(!h.g.audio().musicMuted());   // untouched
    h.press(Action::MuteSfx);
    CHECK(!h.g.audio().sfxMuted());
}

TEST(pause_freezes_the_round) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    h.run(30);
    h.press(Action::Pause);
    CHECK(h.g.paused());

    const Vec2  pos  = h.g.round().player().position();
    const float fuel = h.g.round().fuel().fuel();
    std::vector<std::pair<float,float>> cars;
    for (const auto& e : h.g.round().enemies()) cars.push_back({e.position().x, e.position().y});

    h.run(180);                          // three seconds of nothing happening

    CHECK_NEAR(h.g.round().player().position().x, pos.x, 0.001);
    CHECK_NEAR(h.g.round().player().position().y, pos.y, 0.001);
    CHECK_NEAR(h.g.round().fuel().fuel(), fuel, 0.001);     // fuel does not burn
    for (size_t i = 0; i < cars.size(); ++i) {
        CHECK_NEAR(h.g.round().enemies()[i].position().x, cars[i].first,  0.001);
        CHECK_NEAR(h.g.round().enemies()[i].position().y, cars[i].second, 0.001);
    }
    CHECK(h.g.state() == GameState::Playing);
}

TEST(unpausing_lets_the_round_carry_on) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));

    h.press(Action::Pause);
    CHECK(h.g.paused());
    const float held = h.g.round().fuel().fuel();
    h.run(120);

    h.press(Action::Pause);
    CHECK(!h.g.paused());
    h.run(120);
    CHECK(h.g.round().fuel().fuel() < held);     // time is passing again
}

TEST(pause_stops_the_music_without_changing_the_mute_setting) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));
    CHECK(!h.g.audio().musicMuted());

    h.press(Action::Pause);
    CHECK(h.g.audio().musicPaused());
    CHECK(!h.g.audio().musicMuted());   // pausing is not muting

    h.press(Action::Pause);
    CHECK(!h.g.audio().musicPaused());
    CHECK(!h.g.audio().musicMuted());
}

TEST(menus_and_between_round_cards_cannot_be_paused) {
    HeadlessGame h; CHECK(h.ok);
    CHECK(h.g.state() == GameState::StartScreen);
    h.press(Action::Pause);
    CHECK(!h.g.paused());               // nothing to freeze on the title

    h.press(Action::Start);
    CHECK(h.g.state() == GameState::Ready);
    h.press(Action::Pause);
    CHECK(!h.g.paused());               // nor on the READY card
}

TEST(a_state_change_never_leaves_the_game_paused) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    CHECK(h.waitFor(GameState::Playing, 300));
    h.press(Action::Pause);
    CHECK(h.g.paused());

    // Clearing the round while paused would otherwise strand the next round.
    h.g.debugCollectAllFlags();
    h.press(Action::Pause);             // unpause so the round can finish
    h.run(1);
    CHECK(h.g.state() == GameState::RoundComplete);
    CHECK(!h.g.paused());
    CHECK(h.waitFor(GameState::Ready, 60 * 5));
    CHECK(!h.g.paused());
}

TEST(a_paused_challenging_stage_does_not_run_its_fuel_down) {
    HeadlessGame h; CHECK(h.ok);
    h.press(Action::Start);
    for (int round = 1; round <= 2; ++round) {
        CHECK(h.waitFor(GameState::Playing, 60 * 10));
        h.g.debugCollectAllFlags();
        h.run(1);
        CHECK(h.waitFor(GameState::Ready, 60 * 5));
    }
    CHECK(h.waitFor(GameState::ChallengingStage, 60 * 5));

    h.press(Action::Pause);
    CHECK(h.g.paused());
    const float fuel = h.g.round().fuel().fuel();
    h.run(300);
    CHECK_NEAR(h.g.round().fuel().fuel(), fuel, 0.001);
    CHECK(!h.g.round().chaseActive());   // and the cars stay penned
}
