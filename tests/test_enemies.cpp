#include "TestFramework.h"
#include "ai/NavigationGraph.h"
#include "ai/Pathfinding.h"
#include "ai/EnemyAI.h"
#include "entities/Enemy.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "gameplay/SmokeSystem.h"
#include "gameplay/FuelSystem.h"
#include "core/InputManager.h"
#include "world/LevelLoader.h"
#include <cstdio>

using namespace rx;

namespace {

// A plus-shaped maze: one true junction in the middle, four dead-end arms.
const char* kCross =
    "name CROSS\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "#######\n"
    "###.###\n"
    "###.###\n"
    "#.....#\n"
    "###.###\n"
    "###.###\n"
    "#######\n";

// A trap: from (5,1) the target at (3,3) looks closer down the right-hand
// column, but that column is a dead end.  Straight-line steering walks into
// it; a real route through the maze has to go the long way round the left.
const char* kDetour =
    "name DETOUR\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "#######\n"
    "#.....#\n"
    "#.###.#\n"
    "#.#.#.#\n"
    "#.#.#.#\n"
    "#...#.#\n"
    "#######\n";

LevelData parse(const char* t) {
    LevelData d;
    CHECK(LevelLoader::loadFromString(t, d));
    return d;
}

} // namespace

TEST(navigation_graph_finds_junctions_corners_and_dead_ends) {
    LevelData d = parse(kCross);
    NavigationGraph g;
    g.build(d.map);

    CHECK_EQ(g.openCount(3, 3), 4);          // the centre of the cross
    CHECK(g.isDecisionTile(3, 3));
    CHECK(g.isDecisionTile(3, 1));           // dead end at the top
    CHECK(g.isDecisionTile(1, 3));           // dead end on the left
    CHECK(!g.isDecisionTile(3, 2));          // plain vertical corridor
    CHECK(!g.isDecisionTile(2, 3));          // plain horizontal corridor
}

TEST(navigation_graph_links_nodes_along_corridors) {
    LevelData d = parse(kCross);
    NavigationGraph g;
    g.build(d.map);

    const int centre = g.nodeAt(3, 3);
    CHECK(centre >= 0);
    const auto& n = g.nodes()[centre];
    for (Direction dir : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
        const int nb = n.neighbor[static_cast<int>(dir)];
        CHECK(nb >= 0);                       // every arm ends in a dead-end node
        CHECK_EQ(n.cost[static_cast<int>(dir)], 2);
    }
}

TEST(navigation_graph_refuses_to_step_into_walls) {
    LevelData d = parse(kCross);
    NavigationGraph g;
    g.build(d.map);
    CHECK(!g.isOpen(3, 1, Direction::Up));
    CHECK(!g.isOpen(3, 1, Direction::Left));
    CHECK(g.isOpen(3, 1, Direction::Down));
}

TEST(legal_moves_exclude_a_straight_reversal) {
    LevelData d = parse(kCross);
    NavigationGraph g;
    g.build(d.map);
    // Standing at the centre having arrived heading Right: Left is a reversal.
    const auto moves = g.legalMoves(3, 3, Direction::Right);
    CHECK_EQ(static_cast<int>(moves.size()), 3);
    for (Direction m : moves) CHECK(m != Direction::Left);
}

TEST(pathfinding_routes_around_a_dead_end_that_fools_greedy_steering) {
    LevelData d = parse(kDetour);
    // Greedy steering dives into the right-hand column because it starts out
    // closer; A* sees that it ends nowhere and goes left instead.
    CHECK(Pathfinding::greedyStep(d.map, 5, 1, 3, 3, Direction::None) == Direction::Down);
    CHECK(Pathfinding::firstStep (d.map, 5, 1, 3, 3, Direction::None) == Direction::Left);
}

TEST(greedy_stepping_prefers_closing_the_gap) {
    LevelData d = parse(kCross);
    const Direction step = Pathfinding::greedyStep(d.map, 3, 3, 3, 5, Direction::None);
    CHECK(step == Direction::Down);
}

TEST(pathfinding_reverses_out_of_a_dead_end) {
    LevelData d = parse(kCross);
    // Sitting in the top dead end, heading Up, with the target below.
    const Direction step = Pathfinding::greedyStep(d.map, 3, 1, 3, 5, Direction::Up);
    CHECK(step == Direction::Down);
}

TEST(pathfinding_walks_a_full_route_to_the_target) {
    LevelData d = parse(kDetour);
    int x = 5, y = 1;
    Direction last = Direction::None;
    int steps = 0;
    while (!(x == 3 && y == 3) && steps < 200) {
        const Direction s = Pathfinding::firstStep(d.map, x, y, 3, 3, last);
        CHECK(s != Direction::None);
        if (s == Direction::None) break;
        x += dirDX(s); y += dirDY(s);
        CHECK(d.map.isRoad(x, y));
        last = s;
        ++steps;
    }
    CHECK_EQ(x, 3);
    CHECK_EQ(y, 3);
    CHECK(steps < 200);
}

TEST(every_car_starts_inside_the_pen) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    CHECK_EQ(static_cast<int>(r.enemies().size()), d.enemyCount);
    CHECK(d.enemyCount <= static_cast<int>(d.enemyPen.size()));

    for (const auto& e : r.enemies()) {
        const int tx = TileMap::toTile(e.position().x);
        const int ty = TileMap::toTile(e.position().y);
        bool inPen = false;
        for (const auto& pen : d.enemyPen)
            if (pen.tx == tx && pen.ty == ty) inPen = true;
        CHECK(inPen);
    }
}

TEST(no_two_cars_share_a_pen_tile) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level12.lvl", d));
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    for (size_t i = 0; i < r.enemies().size(); ++i)
        for (size_t j = i + 1; j < r.enemies().size(); ++j) {
            const Vec2& a = r.enemies()[i].position();
            const Vec2& b = r.enemies()[j].position();
            CHECK(std::fabs(a.x - b.x) + std::fabs(a.y - b.y) > 0.5f);
        }
}

TEST(the_whole_pack_launches_on_the_same_step) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level12.lvl", d));
    d.fuelDrain = 0.f;
    d.playerSpeed = 0.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    CHECK(static_cast<int>(r.enemies().size()) > 1);
    for (const auto& e : r.enemies()) CHECK(e.state() == EnemyState::Spawning);

    InputManager in;
    int launchStep = -1;
    for (int i = 0; i < 60 * 6; ++i) {
        r.update(in, s, static_cast<float>(FIXED_DT));

        int waiting = 0, loose = 0;
        for (const auto& e : r.enemies())
            (e.state() == EnemyState::Spawning ? waiting : loose)++;

        // There must never be a step where some cars are out and some are not.
        CHECK(waiting == 0 || loose == 0);
        if (loose > 0 && launchStep < 0) launchStep = i;
    }
    CHECK(launchStep > 0);                       // they did wait first
    for (const auto& e : r.enemies()) CHECK(e.dangerous());
}

TEST(the_pack_starts_far_enough_away_to_escape) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    // Straight-line distance is not the promise; the promise is that the
    // player has most of the maze between them and the pack at the off.
    for (const auto& e : r.enemies()) {
        const float dx = e.position().x - r.player().position().x;
        const float dy = e.position().y - r.player().position().y;
        CHECK(std::fabs(dx) + std::fabs(dy) > 12 * TILE);
    }
}

TEST(enemies_actually_move_through_the_maze) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    for (int i = 0; i < 60 * 3; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    std::vector<Vec2> before;
    for (const auto& e : r.enemies()) before.push_back(e.position());

    for (int i = 0; i < 60 * 3; ++i) r.update(in, s, static_cast<float>(FIXED_DT));

    int moved = 0;
    for (size_t i = 0; i < before.size(); ++i) {
        const Vec2& p = r.enemies()[i].position();
        if (std::fabs(p.x - before[i].x) + std::fabs(p.y - before[i].y) > 4.f) ++moved;
        // and never inside a wall
        CHECK(!r.map().isWallAtPixel(p.x, p.y));
    }
    CHECK_EQ(moved, static_cast<int>(before.size()));
}

TEST(enemies_close_in_on_a_stationary_player) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    d.fuelDrain = 0.f;
    d.playerSpeed = 0.f;      // park the car so the pursuit is what is measured
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    auto nearest = [&]() {
        float best = 1e9f;
        for (const auto& e : r.enemies()) {
            if (!e.dangerous()) continue;
            const float dx = e.position().x - r.player().position().x;
            const float dy = e.position().y - r.player().position().y;
            best = std::min(best, std::fabs(dx) + std::fabs(dy));
        }
        return best;
    };

    // Let the pack finish spawning before measuring the opening gap.
    for (int i = 0; i < 90; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    const float start = nearest();
    CHECK(start > 0.f);

    bool died = false;
    DeathCause cause = DeathCause::None;
    float closest = start;
    for (int i = 0; i < 60 * 25 && !died; ++i) {
        const auto ev = r.update(in, s, static_cast<float>(FIXED_DT));
        if (ev.playerDied) { died = true; cause = ev.cause; }
        closest = std::min(closest, nearest());
    }

    // Either they ran the player down, or they at least halved the gap.
    CHECK(died || closest < start * 0.5f);
    if (died) CHECK(cause == DeathCause::Enemy);
}

TEST(an_enemy_that_touches_the_player_kills_it) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    d.fuelDrain = 0.f;
    d.playerSpeed = 0.f;      // parked, so the only thing that can kill is a car
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    Round::Events ev;
    for (int i = 0; i < 60 * 60 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.playerDied);
    CHECK(ev.cause == DeathCause::Enemy);
    CHECK(!r.player().alive());
}

TEST(smoke_stuns_an_enemy_and_the_stun_wears_off) {
    Enemy e;
    e.spawn(Vec2{100.f, 100.f}, Direction::Left, 1.f, 0.f);
    CHECK(e.dangerous());

    e.stun(1.0f);
    CHECK(e.stunned());
    CHECK(!e.dangerous());

    for (int i = 0; i < 70; ++i) e.tickStun(static_cast<float>(FIXED_DT));
    CHECK(!e.stunned());
    CHECK(e.dangerous());
}

// One enemy, one corridor, one player parked facing away from it, so the car
// has no choice but to drive through the smoke trail.
const char* kSmokeLane =
    "name SMOKELANE\ntype NORMAL\nfuel 500\nfuelDrain 0\nplayerSpeed 0\nenemySpeed 1\nmaze\n"
    "#########\n"
    "#E...P..#\n"
    "#########\n";

TEST(smoke_stops_the_car_chasing_the_player) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kSmokeLane));
    CHECK_EQ(static_cast<int>(r.enemies().size()), 1);

    InputManager in;

    // Smoke comes in three-puff bursts now, so keeping a cloud up means
    // tapping the button repeatedly rather than leaning on it.
    bool sawStunned = false;
    for (int i = 0; i < 60 * 4; ++i) {
        in.setFromExternal(Action::Smoke, (i % 30) < 2);
        r.update(in, s, static_cast<float>(FIXED_DT));
        if (r.enemies()[0].stunned()) sawStunned = true;
    }
    CHECK(sawStunned);
    CHECK(r.player().alive());              // the smoke kept it off

    // A car must not travel a single pixel across any pair of frames where it
    // was stalled on both.
    Vec2 prev = r.enemies()[0].position();
    bool wasStunned = r.enemies()[0].stunned();
    for (int i = 0; i < 120; ++i) {
        in.setFromExternal(Action::Smoke, (i % 30) < 2);
        r.update(in, s, static_cast<float>(FIXED_DT));
        const bool nowStunned = r.enemies()[0].stunned();
        const Vec2 now = r.enemies()[0].position();
        if (wasStunned && nowStunned) {
            CHECK_NEAR(now.x, prev.x, 0.001);
            CHECK_NEAR(now.y, prev.y, 0.001);
        }
        prev = now;
        wasStunned = nowStunned;
    }
}

TEST(when_the_smoke_clears_the_car_comes_back) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kSmokeLane));

    InputManager in;
    for (int i = 0; i < 60 * 3; ++i) {
        in.setFromExternal(Action::Smoke, (i % 30) < 2);   // keep bursting
        r.update(in, s, static_cast<float>(FIXED_DT));
    }
    CHECK(r.player().alive());

    in.setFromExternal(Action::Smoke, false);      // stop laying smoke
    Round::Events ev;
    for (int i = 0; i < 60 * 10 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.playerDied);
    CHECK(ev.cause == DeathCause::Enemy);
}

// A rock parked in a one-tile corridor: the only way through is over it.
const char* kRockGate =
    "name ROCKGATE\ntype NORMAL\nfuel 500\nfuelDrain 0\nplayerSpeed 0\nenemySpeed 1\n"
    "enemies 1\nmaze\n"
    "##########\n"
    "#E...R..P#\n"
    "##########\n";

TEST(rocks_are_solid_to_the_pursuit_cars) {
    LevelData d = parse(kRockGate);
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    CHECK_EQ(static_cast<int>(r.rocks().size()), 1);
    const float rockX = r.rocks()[0].pos.x;

    InputManager in;
    for (int i = 0; i < 60 * 12; ++i) {
        r.update(in, s, static_cast<float>(FIXED_DT));
        // The car may nose right up to the rock, but never past it.
        CHECK(r.enemies()[0].position().x < rockX);
    }
    // The player is walled off behind the rock, so it survives.
    CHECK(r.player().alive());
}

TEST(a_car_noses_up_to_a_rock_and_turns_away) {
    LevelData d = parse(kRockGate);
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    float closest = 1e9f;
    bool turnedBack = false;
    for (int i = 0; i < 60 * 12; ++i) {
        r.update(in, s, static_cast<float>(FIXED_DT));
        closest = std::min(closest, std::fabs(r.enemies()[0].position().x - r.rocks()[0].pos.x));
        if (r.enemies()[0].direction() == Direction::Left) turnedBack = true;
    }
    CHECK(closest <= static_cast<float>(TILE) + 0.01f);   // it really did reach the rock
    CHECK(turnedBack);                                    // and then gave up on it
}

TEST(the_rock_map_only_blocks_cars_not_the_player) {
    LevelData d = parse(kRockGate);
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    const int rx = d.rocks[0].tx, ry = d.rocks[0].ty;
    CHECK(r.map().isRoad(rx, ry));        // the player drives onto it and dies
    CHECK(r.enemyMap().isWall(rx, ry));   // a car cannot enter it at all

    // Everything else about the two maps is identical.
    for (int y = 0; y < r.map().height(); ++y)
        for (int x = 0; x < r.map().width(); ++x) {
            const bool isRock = (x == rx && y == ry);
            if (!isRock) CHECK(r.map().isWall(x, y) == r.enemyMap().isWall(x, y));
        }
}

TEST(rocks_never_seal_the_pen_off_from_the_player) {
    for (int n = 1; n <= 99; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        if (!LevelLoader::loadFromFile(path, d)) break;
        if (d.enemyCount == 0) continue;          // challenging stage

        Round r; r.load(d);
        const TileMap& m = r.enemyMap();

        // Flood fill from the head of the pen across rock-free road only.
        std::vector<uint8_t> seen(MAP_W * MAP_H, 0);
        std::vector<std::pair<int,int>> queue{{d.enemyPen[0].tx, d.enemyPen[0].ty}};
        seen[d.enemyPen[0].ty * MAP_W + d.enemyPen[0].tx] = 1;
        for (size_t head = 0; head < queue.size(); ++head) {
            auto [x, y] = queue[head];
            const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
            for (int i = 0; i < 4; ++i) {
                const int nx = x + dx[i], ny = y + dy[i];
                if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
                if (seen[ny * MAP_W + nx] || !m.isRoad(nx, ny)) continue;
                seen[ny * MAP_W + nx] = 1;
                queue.push_back({nx, ny});
            }
        }
        // Every car must be able to reach the player, and every pen tile must
        // be able to get out.
        CHECK(seen[d.playerSpawn.ty * MAP_W + d.playerSpawn.tx] != 0);
        for (const auto& t : d.enemyPen) CHECK(seen[t.ty * MAP_W + t.tx] != 0);
    }
}

TEST(a_challenging_stage_puts_no_enemies_on_the_track) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level03.lvl", d));
    CHECK(d.type == RoundType::Challenge);

    Round r; ScoreSystem s; s.newGame();
    r.load(d);
    CHECK(r.isChallenge());
    CHECK_EQ(static_cast<int>(r.enemies().size()), 0);

    InputManager in;
    Round::Events ev;
    for (int i = 0; i < 60 * 20 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(ev.cause != DeathCause::Enemy);
}

// ---------------------------------------------------------------------------
// Challenging stage: the fuel gauge is a countdown to being hunted.
// ---------------------------------------------------------------------------

namespace {

LevelData challengeLevel() {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level03.lvl", d));
    CHECK(d.type == RoundType::Challenge);
    return d;
}

} // namespace

TEST(a_challenging_stage_drives_a_faster_car) {
    for (int n : { 3, 7, 11 }) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData c;
        CHECK(LevelLoader::loadFromFile(path, c));
        CHECK(c.type == RoundType::Challenge);

        LevelData normal;
        CHECK(LevelLoader::loadFromFile("levels/level01.lvl", normal));
        CHECK(c.playerSpeed > normal.playerSpeed);
    }
}

TEST(the_chase_cars_wait_in_the_pen_until_the_tank_is_dry) {
    LevelData d = challengeLevel();
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    // The track starts clear even though the level lists cars.
    CHECK(d.enemyCount > 0);
    CHECK_EQ(static_cast<int>(r.enemies().size()), 0);
    CHECK(!r.chaseActive());

    InputManager in;
    for (int i = 0; i < 60 * 5; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK_EQ(static_cast<int>(r.enemies().size()), 0);   // still clear with fuel left
    CHECK(!r.chaseActive());
}

TEST(running_dry_in_a_challenging_stage_releases_the_cars) {
    LevelData d = challengeLevel();
    d.fuel = 3.f;                       // straight to the interesting part
    d.fuelDrain = 6.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    Round::Events ev;
    for (int i = 0; i < 120 && !ev.chaseStarted; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.chaseStarted);
    CHECK(r.chaseActive());
    CHECK(!ev.playerDied);                          // dry fuel does not kill here
    CHECK(r.player().alive());
    CHECK_EQ(static_cast<int>(r.enemies().size()), d.enemyCount);
}

TEST(the_chase_cars_run_at_twice_the_players_speed) {
    LevelData d = challengeLevel();
    d.fuel = 3.f; d.fuelDrain = 6.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    for (int i = 0; i < 120 && !r.chaseActive(); ++i)
        r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(r.chaseActive());

    for (const auto& e : r.enemies())
        CHECK_NEAR(e.speed(), d.playerSpeed * Round::CHASE_SPEED_MULTIPLE, 1e-4);
    CHECK(r.enemies()[0].speed() > d.playerSpeed);
}

TEST(the_chase_launches_almost_immediately) {
    LevelData d = challengeLevel();
    d.fuel = 3.f; d.fuelDrain = 6.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    for (int i = 0; i < 120 && !r.chaseActive(); ++i)
        r.update(in, s, static_cast<float>(FIXED_DT));

    // Just long enough for the alarm to register -- an ordinary round holds
    // the pack far longer.
    CHECK(r.launchCountdown() > 0.f);
    CHECK(r.launchCountdown() < 1.0f);

    for (int i = 0; i < 60; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
    for (const auto& e : r.enemies()) CHECK(e.state() != EnemyState::Spawning);
}

TEST(the_chase_never_kills_on_the_frame_it_is_released) {
    // If the player happens to be standing on the pen when the tank empties,
    // the cars must not materialise straight into them.
    LevelData d = challengeLevel();
    d.fuel = 3.f; d.fuelDrain = 6.f;
    d.rocks.clear();
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    int chaseAt = -1, diedAt = -1;
    for (int i = 0; i < 60 * 20 && diedAt < 0; ++i) {
        const auto ev = r.update(in, s, static_cast<float>(FIXED_DT));
        if (ev.chaseStarted) chaseAt = i;
        if (ev.playerDied)   diedAt  = i;
    }
    CHECK(chaseAt >= 0);
    if (diedAt >= 0) CHECK(diedAt - chaseAt >= 20);   // at least a third of a second
}

TEST(the_released_cars_run_the_player_down_quickly) {
    LevelData d = challengeLevel();
    d.fuel = 3.f; d.fuelDrain = 6.f;     // real speeds: chase is 2x the player
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    Round::Events ev;
    int steps = 0;
    for (; steps < 60 * 30 && !ev.playerDied; ++steps)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.playerDied);
    CHECK(ev.cause == DeathCause::Enemy);
    CHECK(steps < 60 * 20);              // caught, and not after a long wait
}

TEST(the_chase_only_starts_once) {
    LevelData d = challengeLevel();
    d.fuel = 3.f; d.fuelDrain = 6.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    int started = 0;
    for (int i = 0; i < 300; ++i) {
        const auto ev = r.update(in, s, static_cast<float>(FIXED_DT));
        if (ev.chaseStarted) ++started;
        if (!r.player().alive()) break;
    }
    CHECK_EQ(started, 1);                // not re-spawned every empty-tank frame
}

TEST(an_ordinary_round_still_dies_when_the_tank_runs_dry) {
    LevelData d;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", d));
    d.fuel = 3.f; d.fuelDrain = 6.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    Round::Events ev;
    for (int i = 0; i < 200 && !ev.playerDied; ++i)
        ev = r.update(in, s, static_cast<float>(FIXED_DT));

    CHECK(ev.playerDied);
    CHECK(ev.cause == DeathCause::OutOfFuel);   // unchanged outside a challenge
    CHECK(!ev.chaseStarted);
}

TEST(restarting_a_challenging_stage_puts_the_cars_back_in_the_pen) {
    LevelData d = challengeLevel();
    d.fuel = 3.f; d.fuelDrain = 6.f;
    Round r; ScoreSystem s; s.newGame();
    r.load(d);

    InputManager in;
    for (int i = 0; i < 120 && !r.chaseActive(); ++i)
        r.update(in, s, static_cast<float>(FIXED_DT));
    CHECK(r.chaseActive());

    r.restart();
    CHECK(!r.chaseActive());
    CHECK_EQ(static_cast<int>(r.enemies().size()), 0);
    CHECK_NEAR(r.fuel().fuel(), r.fuel().capacity(), 1e-4);
}
