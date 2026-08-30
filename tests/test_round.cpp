#include "TestFramework.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "core/InputManager.h"
#include "world/LevelLoader.h"

using namespace rx;

namespace {

// A straight 12-tile corridor with three flags in it.
const char* kCorridor =
    "name CORRIDOR\ntype NORMAL\nfuel 100\nplayerSpeed 2\nenemySpeed 1\nmaze\n"
    "##############\n"
    "#P..F...F...F#\n"
    "##############\n";

LevelData corridor() {
    LevelData d;
    CHECK(LevelLoader::loadFromString(kCorridor, d));
    return d;
}

void hold(InputManager& in, Action a) {
    in.setFromExternal(Action::Up, false);   in.setFromExternal(Action::Down, false);
    in.setFromExternal(Action::Left, false); in.setFromExternal(Action::Right, false);
    in.setFromExternal(a, true);
}

} // namespace

TEST(round_spawns_the_player_on_its_spawn_tile) {
    Round r; ScoreSystem s; s.newGame();
    r.load(corridor());
    CHECK_NEAR(r.player().position().x, 1 * TILE + 8.f, 0.001);
    CHECK_NEAR(r.player().position().y, 1 * TILE + 8.f, 0.001);
    CHECK_EQ(r.flagsCollected(), 0);
    CHECK_EQ(r.flagsRequired(), 3);
}

TEST(driving_over_flags_collects_them_and_scores) {
    Round r; ScoreSystem s; s.newGame();
    r.load(corridor());

    InputManager in;
    hold(in, Action::Right);

    Round::Events last;
    for (int i = 0; i < 400 && !last.roundComplete; ++i)
        last = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK_EQ(r.flagsCollected(), 3);
    CHECK_EQ(r.flagsRemaining(), 0);
    CHECK(last.roundComplete);
    CHECK_EQ(s.score(), 100 + 200 + 300);
}

TEST(a_collected_flag_is_never_collected_twice) {
    Round r; ScoreSystem s; s.newGame();
    r.load(corridor());

    InputManager in;
    hold(in, Action::Right);
    for (int i = 0; i < 200; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    const int scoreAfterRun = s.score();

    hold(in, Action::Left);
    for (int i = 0; i < 200; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK_EQ(s.score(), scoreAfterRun);
    CHECK_EQ(r.flagsCollected(), 3);
}

TEST(restart_puts_every_flag_back) {
    Round r; ScoreSystem s; s.newGame();
    r.load(corridor());
    InputManager in; hold(in, Action::Right);
    for (int i = 0; i < 200; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK_EQ(r.flagsCollected(), 3);

    r.restart();
    CHECK_EQ(r.flagsCollected(), 0);
    for (const auto& f : r.flags()) CHECK(!f.collected);
    CHECK_NEAR(r.player().position().x, 1 * TILE + 8.f, 0.001);
}

TEST(the_car_cannot_leave_the_corridor) {
    Round r; ScoreSystem s; s.newGame();
    r.load(corridor());

    InputManager in;
    for (int i = 0; i < 300; ++i) {
        hold(in, (i / 10 % 2) ? Action::Up : Action::Down);   // mash into the walls
        r.update(in, s, static_cast<float>(FIXED_DT));
        CHECK(!r.map().isWallAtPixel(r.player().position().x, r.player().position().y));
        CHECK_NEAR(r.player().position().y, 1 * TILE + 8.f, 0.001);
    }
}

TEST(the_camera_stays_locked_to_the_player) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in; hold(in, Action::Right);
    for (int i = 0; i < 300; ++i) r.update(in, s, static_cast<float>(FIXED_DT));

    const Vec2 p = r.player().position();
    const Vec2 c = r.camera().position();
    CHECK(c.x >= 0.f && c.y >= 0.f);
    CHECK(c.x <= WORLD_W - VIEW_W && c.y <= WORLD_H - VIEW_H);
    // The player must always be somewhere inside the visible viewport.
    CHECK(p.x - c.x >= 0.f && p.x - c.x <= VIEW_W);
    CHECK(p.y - c.y >= 0.f && p.y - c.y <= VIEW_H);
}
