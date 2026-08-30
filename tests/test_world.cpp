#include "TestFramework.h"
#include "world/LevelLoader.h"
#include "world/CollisionSystem.h"
#include "world/Camera.h"
#include "core/Types.h"
#include <cstdio>
#include <string>

using namespace rx;

namespace {

// A hand-written 8x8 test level: a ring corridor inside a solid border with a
// cross in the middle, so turns, dead ends and blocked moves are all present.
const char* kTestLevel =
    "name TEST\n"
    "type NORMAL\n"
    "fuel 50\n"
    "playerSpeed 2\n"
    "enemySpeed 1\n"
    "maze\n"
    "########\n"
    "#P.....#\n"
    "#.####.#\n"
    "#.#FF#.#\n"
    "#.#RE#.#\n"
    "#.####.#\n"
    "#.....S#\n"
    "########\n";

LevelData loadTest() {
    LevelData d;
    CHECK(LevelLoader::loadFromString(kTestLevel, d));
    return d;
}

} // namespace

TEST(level_parses_geometry_and_entities) {
    LevelData d = loadTest();
    CHECK_EQ(d.map.width(), 8);
    CHECK_EQ(d.map.height(), 8);
    CHECK(d.map.isWall(0, 0));
    CHECK(d.map.isRoad(1, 1));
    CHECK(d.map.isWall(2, 2));
    CHECK_EQ(d.playerSpawn.tx, 1);
    CHECK_EQ(d.playerSpawn.ty, 1);
    CHECK_EQ(static_cast<int>(d.flags.size()), 3);       // 2 normal + 1 special
    CHECK_EQ(static_cast<int>(d.rocks.size()), 1);
    CHECK_EQ(static_cast<int>(d.enemyPen.size()), 1);
    CHECK_EQ(d.enemyCount, 1);          // defaults to the pen size
    CHECK_NEAR(d.playerSpeed, 2.0, 1e-5);
}

TEST(level_flag_kinds) {
    LevelData d = loadTest();
    int normal = 0, special = 0, lucky = 0;
    for (const auto& f : d.flags) {
        if (f.kind == 0) ++normal;
        if (f.kind == 1) ++special;
        if (f.kind == 2) ++lucky;
    }
    CHECK_EQ(normal, 2);
    CHECK_EQ(special, 1);
    CHECK_EQ(lucky, 0);
}

TEST(tiles_outside_the_map_read_as_solid) {
    LevelData d = loadTest();
    CHECK(d.map.isWall(-1, 3));
    CHECK(d.map.isWall(3, -1));
    CHECK(d.map.isWall(99, 3));
}

TEST(car_stops_against_a_wall_and_never_enters_it) {
    LevelData d = loadTest();
    Vec2 pos{ 1 * TILE + 8.f, 1 * TILE + 8.f };      // centre of the spawn tile
    Direction dir = Direction::Up;                   // wall immediately above

    for (int i = 0; i < 120; ++i)
        CollisionSystem::step(d.map, pos, dir, Direction::Up, 2.f);

    CHECK_NEAR(pos.y, 1 * TILE + 8.f, 0.001);        // parked on the tile centre
    CHECK(!d.map.isWallAtPixel(pos.x, pos.y));
}

TEST(car_drives_down_an_open_corridor) {
    LevelData d = loadTest();
    Vec2 pos{ 1 * TILE + 8.f, 1 * TILE + 8.f };
    Direction dir = Direction::Right;

    for (int i = 0; i < 60; ++i)
        CollisionSystem::step(d.map, pos, dir, Direction::Right, 2.f);

    // Row 1 is open to x=6; the car must end parked in that last open tile.
    CHECK_NEAR(pos.x, 6 * TILE + 8.f, 0.001);
    CHECK_NEAR(pos.y, 1 * TILE + 8.f, 0.001);
}

TEST(turning_is_only_legal_into_an_open_tile) {
    LevelData d = loadTest();
    Vec2 pos{ 1 * TILE + 8.f, 1 * TILE + 8.f };
    Direction dir = Direction::Right;

    // Down from (1,1) is open, up is solid.
    CHECK(CollisionSystem::canTurn(d.map, pos, Direction::Down, 5.f));
    CHECK(!CollisionSystem::canTurn(d.map, pos, Direction::Up, 5.f));

    auto r = CollisionSystem::step(d.map, pos, dir, Direction::Down, 2.f);
    CHECK(r.turned);
    CHECK(dir == Direction::Down);
}

TEST(turning_mid_tile_is_refused_until_aligned) {
    LevelData d = loadTest();
    Vec2 pos{ 1 * TILE + 8.f, 1 * TILE + 8.f };
    Direction dir = Direction::Right;

    CollisionSystem::step(d.map, pos, dir, Direction::Right, 8.f);  // half a tile along
    CHECK(!CollisionSystem::canTurn(d.map, pos, Direction::Down, 5.f));
    CHECK(dir == Direction::Right);
}

TEST(reversal_is_always_allowed) {
    LevelData d = loadTest();
    Vec2 pos{ 1 * TILE + 8.f, 1 * TILE + 8.f };
    Direction dir = Direction::Right;
    CollisionSystem::step(d.map, pos, dir, Direction::Right, 7.f);   // deliberately off-centre
    auto r = CollisionSystem::step(d.map, pos, dir, Direction::Left, 2.f);
    CHECK(r.turned);
    CHECK(dir == Direction::Left);
}

TEST(turning_snaps_the_car_onto_the_corridor_centre) {
    LevelData d = loadTest();
    Vec2 pos{ 1 * TILE + 8.f, 1 * TILE + 10.f };     // 2px off the centre line
    Direction dir = Direction::Down;
    CollisionSystem::step(d.map, pos, dir, Direction::Right, 2.f);
    CHECK(dir == Direction::Right);
    CHECK_NEAR(pos.y, 1 * TILE + 8.f, 0.001);
}

TEST(camera_follows_and_clamps_to_the_world) {
    Camera cam;
    cam.setViewport(VIEW_W, VIEW_H);

    cam.centerOn(Vec2{ WORLD_W / 2.f, WORLD_H / 2.f });
    CHECK_NEAR(cam.position().x, WORLD_W / 2.f - VIEW_W / 2.f, 0.001);
    CHECK_NEAR(cam.position().y, WORLD_H / 2.f - VIEW_H / 2.f, 0.001);

    cam.centerOn(Vec2{ 4.f, 4.f });                       // top-left corner
    CHECK_NEAR(cam.position().x, 0.0, 0.001);
    CHECK_NEAR(cam.position().y, 0.0, 0.001);

    cam.centerOn(Vec2{ WORLD_W - 4.f, WORLD_H - 4.f });   // bottom-right corner
    CHECK_NEAR(cam.position().x, WORLD_W - VIEW_W, 0.001);
    CHECK_NEAR(cam.position().y, WORLD_H - VIEW_H, 0.001);
}

TEST(shipped_levels_are_well_formed_and_connected) {
    // Every level the game will actually load, not a hand-kept list.
    int checked = 0;
    for (int n = 1; n <= 99; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        if (!LevelLoader::loadFromFile(path, d)) break;
        ++checked;
        CHECK_EQ(d.map.width(),  MAP_W);
        CHECK_EQ(d.map.height(), MAP_H);

        int normal = 0;
        for (const auto& f : d.flags) if (f.kind == 0) ++normal;
        CHECK_EQ(normal, FLAGS_PER_ROUND);

        // Nothing may be sealed inside a wall.
        CHECK(d.map.isRoad(d.playerSpawn.tx, d.playerSpawn.ty));
        for (const auto& f : d.flags) CHECK(d.map.isRoad(f.tx, f.ty));
        for (const auto& e : d.enemyPen) CHECK(d.map.isRoad(e.tx, e.ty));
        for (const auto& rk : d.rocks) CHECK(d.map.isRoad(rk.tx, rk.ty));

        // Flood fill from the spawn: every flag must actually be reachable.
        std::vector<uint8_t> seen(MAP_W * MAP_H, 0);
        std::vector<std::pair<int,int>> stack{{d.playerSpawn.tx, d.playerSpawn.ty}};
        seen[d.playerSpawn.ty * MAP_W + d.playerSpawn.tx] = 1;
        while (!stack.empty()) {
            auto [x, y] = stack.back(); stack.pop_back();
            const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
            for (int i = 0; i < 4; ++i) {
                const int nx = x + dx[i], ny = y + dy[i];
                if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
                if (seen[ny * MAP_W + nx] || !d.map.isRoad(nx, ny)) continue;
                seen[ny * MAP_W + nx] = 1;
                stack.push_back({nx, ny});
            }
        }
        for (const auto& f : d.flags) CHECK(seen[f.ty * MAP_W + f.tx] != 0);
    }
    CHECK(checked >= 12);          // the shipped round table
}

TEST(every_shipped_level_has_one_straight_pen_far_from_the_player) {
    int checked = 0;
    for (int n = 1; n <= 99; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        if (!LevelLoader::loadFromFile(path, d)) break;
        ++checked;

        const int size = static_cast<int>(d.enemyPen.size());
        CHECK(size >= 6 && size <= 7);              // a 6-7 tile lane
        CHECK(d.enemyCount <= size);                // never more cars than tiles

        // Every pen tile is road, and the lane is one contiguous straight run.
        for (const auto& t : d.enemyPen) CHECK(d.map.isRoad(t.tx, t.ty));

        const bool horizontal = (d.enemyPen.front().ty == d.enemyPen.back().ty);
        for (int i = 1; i < size; ++i) {
            const auto& a = d.enemyPen[i - 1];
            const auto& b = d.enemyPen[i];
            if (horizontal) { CHECK_EQ(b.ty, a.ty); CHECK_EQ(b.tx, a.tx + 1); }
            else            { CHECK_EQ(b.tx, a.tx); CHECK_EQ(b.ty, a.ty + 1); }
        }

        // And the whole lane sits a long way from the player, measured through
        // the maze rather than as the crow flies.
        std::vector<int> dist(MAP_W * MAP_H, -1);
        std::vector<std::pair<int,int>> queue{{d.playerSpawn.tx, d.playerSpawn.ty}};
        dist[d.playerSpawn.ty * MAP_W + d.playerSpawn.tx] = 0;
        for (size_t head = 0; head < queue.size(); ++head) {
            auto [x, y] = queue[head];
            const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
            for (int i = 0; i < 4; ++i) {
                const int nx = x + dx[i], ny = y + dy[i];
                if (nx < 0 || ny < 0 || nx >= MAP_W || ny >= MAP_H) continue;
                if (dist[ny * MAP_W + nx] >= 0 || !d.map.isRoad(nx, ny)) continue;
                dist[ny * MAP_W + nx] = dist[y * MAP_W + x] + 1;
                queue.push_back({nx, ny});
            }
        }
        for (const auto& t : d.enemyPen) {
            const int steps = dist[t.ty * MAP_W + t.tx];
            CHECK(steps >= 16);                      // a real head start
        }
    }
    CHECK(checked >= 12);
}

TEST(no_two_shipped_mazes_are_the_same_layout) {
    std::vector<std::string> shapes;
    for (int n = 1; n <= 99; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        if (!LevelLoader::loadFromFile(path, d)) break;

        std::string shape;
        for (int y = 0; y < MAP_H; ++y)
            for (int x = 0; x < MAP_W; ++x)
                shape += d.map.isWall(x, y) ? '#' : '.';
        for (const auto& prior : shapes) CHECK(shape != prior);
        shapes.push_back(shape);
    }
    CHECK(static_cast<int>(shapes.size()) >= 12);
}

TEST(the_difficulty_ramp_climbs_across_the_round_table) {
    LevelData first, last;
    CHECK(LevelLoader::loadFromFile("levels/level01.lvl", first));
    CHECK(LevelLoader::loadFromFile("levels/level12.lvl", last));

    CHECK(last.enemyCount > first.enemyCount);                   // more cars
    CHECK(last.rocks.size()       > first.rocks.size());         // more rocks
    CHECK(last.enemySpeed         > first.enemySpeed);           // faster cars
    CHECK(last.fuelDrain          > first.fuelDrain);            // thirstier
    CHECK(last.difficulty         > first.difficulty);

    // The pursuit must stay slower than the player at every authored round.
    for (int n = 1; n <= 12; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        CHECK(LevelLoader::loadFromFile(path, d));
        CHECK(d.enemySpeed < d.playerSpeed);
    }
}

TEST(every_round_starts_with_a_full_hundred_unit_tank) {
    // Difficulty comes from the burn rate, never from a smaller tank, so the
    // gauge always starts full and reads the same way in every round.
    int checked = 0;
    for (int n = 1; n <= 99; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        if (!LevelLoader::loadFromFile(path, d)) break;
        ++checked;
        CHECK_NEAR(d.fuel, 100.0, 1e-4);
        CHECK(d.fuelDrain > 0.f);
    }
    CHECK(checked >= 12);
}

TEST(the_burn_rate_climbs_and_shortens_each_normal_round) {
    // Round length is tank / burn rate.  Normal rounds must get shorter as the
    // game goes on; the challenging stages are their own, much tighter clock.
    float previous = 1e9f;
    int normals = 0;
    for (int n = 1; n <= 12; ++n) {
        char path[64];
        std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
        LevelData d;
        CHECK(LevelLoader::loadFromFile(path, d));

        const float seconds = d.fuel / d.fuelDrain;
        if (d.type == RoundType::Challenge) {
            CHECK(seconds < 55.f);            // a bonus stage is a sprint
            continue;
        }
        CHECK(seconds <= previous + 0.01f);   // never gets more generous
        previous = seconds;
        ++normals;
    }
    CHECK(normals >= 8);
    CHECK(previous < 65.f);                   // and it really has tightened up
}
