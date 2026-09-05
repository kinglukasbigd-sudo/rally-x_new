#include "TestFramework.h"
#include "gameplay/TurboSystem.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "core/InputManager.h"
#include "world/LevelLoader.h"
#include "world/Maze.h"
#include <algorithm>
#include <cmath>
#include <set>

using namespace rx;

namespace {

// A long open corridor with a turbo-friendly amount of room.  The player runs
// right along it from the spawn.
const char* kRun =
    "name RUN\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "####################\n"
    "#P................F#\n"
    "####################\n";

LevelData parse(const char* t) {
    LevelData d;
    CHECK(LevelLoader::loadFromString(t, d));
    return d;
}

// A shipped level, which is where the placement rules actually have to hold.
LevelData shipped(int n, int levelNumber) {
    char path[64];
    std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
    Maze m;
    CHECK(m.load(path));
    LevelData d = m.data();
    d.levelNumber = levelNumber;
    return d;
}

void hold(InputManager& in, Action a) {
    for (Action d : { Action::Up, Action::Down, Action::Left, Action::Right })
        in.setFromExternal(d, false);
    in.setFromExternal(a, true);
}

} // namespace

// --- the spawn table -------------------------------------------------------

TEST(the_turbo_count_follows_the_level_table) {
    for (int lvl = 1; lvl <= 4;  ++lvl) CHECK_EQ(TurboRules::countForLevel(lvl), 0);
    for (int lvl = 5; lvl <= 7;  ++lvl) CHECK_EQ(TurboRules::countForLevel(lvl), 2);
    for (int lvl = 8; lvl <= 10; ++lvl) CHECK_EQ(TurboRules::countForLevel(lvl), 3);
    for (int lvl = 11; lvl <= 40; ++lvl) CHECK_EQ(TurboRules::countForLevel(lvl), 5);
}

TEST(a_round_carries_exactly_the_turbos_its_level_calls_for) {
    // Walked over the shipped mazes, because a table that is right in
    // isolation is worth nothing if the maze cannot hold the pickups.
    const int expected[13] = { 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 3, 5, 5 };
    for (int lvl = 1; lvl <= 12; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 0x1234u + static_cast<uint32_t>(lvl));
        CHECK_EQ(static_cast<int>(r.turbos().size()), expected[lvl]);
    }
}

TEST(the_first_four_rounds_have_no_turbos_at_all) {
    for (int lvl = 1; lvl <= 4; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 77u);
        CHECK(r.turbos().empty());
    }
}

TEST(a_round_past_the_authored_set_still_gets_five) {
    // Round 25 replays maze 1 with a much higher level number: the count must
    // follow the level the player is on, not the maze file.
    Round r;
    r.load(shipped(1, 25), 4242u);
    CHECK_EQ(static_cast<int>(r.turbos().size()), 5);
}

// --- where they land -------------------------------------------------------

TEST(turbos_land_on_road_and_never_on_anything_else) {
    for (int lvl = 5; lvl <= 12; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 0xBEEFu + static_cast<uint32_t>(lvl));
        const LevelData& d = r.level();

        std::set<std::pair<int,int>> taken;
        for (const auto& rk : d.rocks)    taken.insert({ rk.tx, rk.ty });
        for (const auto& pn : d.enemyPen) taken.insert({ pn.tx, pn.ty });
        for (const auto& f : r.flags())
            taken.insert({ TileMap::toTile(f.pos.x), TileMap::toTile(f.pos.y) });

        std::set<std::pair<int,int>> seen;
        for (const auto& t : r.turbos()) {
            const int tx = TileMap::toTile(t.pos.x), ty = TileMap::toTile(t.pos.y);
            CHECK(r.map().isRoad(tx, ty));                       // inside, on road
            CHECK(taken.find({tx, ty}) == taken.end());          // nothing else there
            CHECK(seen.insert({tx, ty}).second);                 // no two on a tile
            // Clear of the player's spawn, so nothing is picked up by accident.
            CHECK(std::abs(tx - d.playerSpawn.tx) + std::abs(ty - d.playerSpawn.ty)
                  >= FlagPlacer::MIN_FROM_SPAWN);
        }
    }
}

TEST(every_turbo_is_reachable_from_the_spawn) {
    for (int lvl = 5; lvl <= 12; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 0x5150u + static_cast<uint32_t>(lvl));
        const TileMap& m = r.map();

        // Flood the maze from the spawn and insist every pickup is in it.
        std::vector<char> seen(static_cast<size_t>(m.width()) * m.height(), 0);
        std::vector<int> q{ r.level().playerSpawn.ty * m.width() + r.level().playerSpawn.tx };
        seen[static_cast<size_t>(q[0])] = 1;
        for (size_t h = 0; h < q.size(); ++h) {
            const int x = q[h] % m.width(), y = q[h] / m.width();
            for (Direction d : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
                const int nx = x + dirDX(d), ny = y + dirDY(d);
                if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
                const size_t n = static_cast<size_t>(ny) * m.width() + nx;
                if (seen[n] || !m.isRoad(nx, ny)) continue;
                seen[n] = 1;
                q.push_back(static_cast<int>(n));
            }
        }
        for (const auto& t : r.turbos())
            CHECK(seen[static_cast<size_t>(TileMap::toTile(t.pos.y)) * m.width()
                        + TileMap::toTile(t.pos.x)] != 0);
    }
}

TEST(a_fresh_round_moves_the_turbos_with_the_flags) {
    Round a, b;
    a.load(shipped(12, 12), 111u);
    b.load(shipped(12, 12), 222u);
    bool differs = false;
    for (size_t i = 0; i < a.turbos().size() && i < b.turbos().size(); ++i)
        if (std::fabs(a.turbos()[i].pos.x - b.turbos()[i].pos.x) > 0.5f ||
            std::fabs(a.turbos()[i].pos.y - b.turbos()[i].pos.y) > 0.5f) differs = true;
    CHECK(differs);

    // ...but the same seed still lays out the same round.
    Round c;
    c.load(shipped(12, 12), 111u);
    CHECK_EQ(c.turbos().size(), a.turbos().size());
    for (size_t i = 0; i < a.turbos().size(); ++i)
        CHECK_NEAR(c.turbos()[i].pos.x, a.turbos()[i].pos.x, 0.001);
}

// --- what it does ----------------------------------------------------------

TEST(the_boost_raises_the_speed_and_then_gives_it_back) {
    TurboSystem t;
    CHECK(!t.active());
    CHECK_NEAR(t.speedFor(1.35f), 1.35f, 0.0001);

    t.activate();
    CHECK(t.active());
    CHECK_NEAR(t.speedFor(1.35f), 1.35f * TurboRules::SPEED_MULTIPLIER, 0.0001);

    // Run it exactly to the end of its life.
    for (int i = 0; i < static_cast<int>(TurboRules::DURATION_SECONDS * 60.f) - 1; ++i)
        t.update(static_cast<float>(FIXED_DT));
    CHECK(t.active());                        // still on, one step from the end
    t.update(static_cast<float>(FIXED_DT));
    t.update(static_cast<float>(FIXED_DT));
    CHECK(!t.active());
    CHECK_NEAR(t.speedFor(1.35f), 1.35f, 0.0001);
}

TEST(a_second_boost_restarts_the_clock_rather_than_stacking) {
    TurboSystem t;
    t.activate();
    for (int i = 0; i < 120; ++i) t.update(static_cast<float>(FIXED_DT));
    const float half = t.remaining();
    t.activate();
    CHECK(t.remaining() > half);
    CHECK_NEAR(t.remaining(), TurboRules::DURATION_SECONDS, 0.0001);
    // And the speed is the single multiple, never twice over.
    CHECK_NEAR(t.speedFor(1.f), TurboRules::SPEED_MULTIPLIER, 0.0001);
}

TEST(driving_over_a_turbo_picks_it_up_and_speeds_the_car_up) {
    LevelData d = parse(kRun);
    d.levelNumber = 5;                        // a level that has turbos
    Round r; ScoreSystem s; s.newGame();
    r.load(d, 12345u);
    CHECK_EQ(static_cast<int>(r.turbos().size()), 2);

    InputManager in;
    hold(in, Action::Right);

    bool sawPickup = false, sawBoostedSpeed = false;
    const float base = d.playerSpeed;
    for (int i = 0; i < 1200; ++i) {
        const auto ev = r.update(in, s, static_cast<float>(FIXED_DT));
        if (ev.turboTaken) sawPickup = true;
        if (r.turbo().active()) {
            sawBoostedSpeed = true;
            CHECK_NEAR(r.player().speed(), base * TurboRules::SPEED_MULTIPLIER, 0.0001);
        }
        // Right is held throughout: at the end of the corridor the car's own
        // wall rule turns it round, so it sweeps the whole run either way.
    }
    CHECK(sawPickup);
    CHECK(sawBoostedSpeed);
    // Whatever it picked up stays picked up.
    int collected = 0;
    for (const auto& t : r.turbos()) if (t.collected) ++collected;
    CHECK(collected >= 1);
}

TEST(a_boosted_car_still_obeys_the_walls) {
    LevelData d = parse(kRun);
    d.levelNumber = 11;
    Round r; ScoreSystem s; s.newGame();
    r.load(d, 999u);

    InputManager in;
    hold(in, Action::Right);
    for (int i = 0; i < 3000; ++i) {
        r.update(in, s, static_cast<float>(FIXED_DT));
        const Vec2 p = r.player().position();
        // The boost is a speed, not a licence: the car must never end a step
        // inside a wall tile, however fast it is going.
        CHECK(r.map().isRoad(TileMap::toTile(p.x), TileMap::toTile(p.y)));
    }
}

TEST(a_new_car_after_a_death_starts_unboosted) {
    LevelData d = parse(kRun);
    d.levelNumber = 8;
    Round r; ScoreSystem s; s.newGame();
    r.load(d, 31337u);

    InputManager in;
    hold(in, Action::Right);
    for (int i = 0; i < 2000 && !r.turbo().active(); ++i)
        r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(r.turbo().active());

    r.restartAfterDeath();
    CHECK(!r.turbo().active());
    CHECK_NEAR(r.player().speed(), d.playerSpeed, 0.0001);
}

TEST(a_fresh_round_puts_every_turbo_back) {
    Round r;
    r.load(shipped(12, 12), 606u);
    const size_t total = r.turbos().size();
    CHECK(total > 0);

    // Nothing collected after a reload, whichever way the round was restarted.
    r.restart();
    for (const auto& t : r.turbos()) CHECK(!t.collected);
    CHECK_EQ(r.turbos().size(), total);
}
