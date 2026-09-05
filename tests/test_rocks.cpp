#include "TestFramework.h"
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

// A wide room with a single rock in the middle of it: plenty of room either
// side, so the rock has a patrol to be given.
const char* kOpen =
    "name OPEN\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "##############\n"
    "#P...........#\n"
    "#.....R......#\n"
    "#...........F#\n"
    "##############\n";

// A dead-straight corridor with the player at one end and a rock at the far
// end.  The rock's only room is towards the player, so the two close on each
// other -- which is what makes it a test of a *moving* rock rather than of a
// rock the player happens to drive into.
const char* kRow =
    "name ROW\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "############\n"
    "#P........R#\n"
    "############\n";

// A rock wedged into a one-tile alcove: there is nowhere for it to slide, so
// it has to stay put even on a round where rocks move.
const char* kWedged =
    "name WEDGED\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "##########\n"
    "#P.......#\n"
    "###.######\n"
    "###R######\n"
    "##########\n";

// A single corridor that a rock could seal if it were allowed to wander into
// the neck of it.  The connectivity rule has to refuse that.
const char* kNeck =
    "name NECK\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "############\n"
    "#P...#.....#\n"
    "#.R..#....F#\n"
    "#....#.....#\n"
    "#..........#\n"
    "############\n";

LevelData parse(const char* t, int levelNumber) {
    LevelData d;
    CHECK(LevelLoader::loadFromString(t, d));
    d.levelNumber = levelNumber;
    return d;
}

LevelData shipped(int n, int levelNumber) {
    char path[64];
    std::snprintf(path, sizeof path, "levels/level%02d.lvl", n);
    Maze m;
    CHECK(m.load(path));
    LevelData d = m.data();
    d.levelNumber = levelNumber;
    return d;
}

// Are all the road tiles still one connected piece with `blocked` walled off?
bool connectedWithout(const TileMap& m, const std::set<std::pair<int,int>>& blocked) {
    int open = 0, start = -1;
    for (int y = 0; y < m.height(); ++y)
        for (int x = 0; x < m.width(); ++x)
            if (m.isRoad(x, y) && blocked.find({x, y}) == blocked.end()) {
                ++open;
                if (start < 0) start = y * m.width() + x;
            }
    if (start < 0) return false;

    std::vector<char> seen(static_cast<size_t>(m.width()) * m.height(), 0);
    std::vector<int> q{ start };
    seen[static_cast<size_t>(start)] = 1;
    for (size_t h = 0; h < q.size(); ++h) {
        const int x = q[h] % m.width(), y = q[h] / m.width();
        for (Direction d : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
            const int nx = x + dirDX(d), ny = y + dirDY(d);
            if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
            const size_t n = static_cast<size_t>(ny) * m.width() + nx;
            if (seen[n] || !m.isRoad(nx, ny) || blocked.count({nx, ny})) continue;
            seen[n] = 1;
            q.push_back(static_cast<int>(n));
        }
    }
    return static_cast<int>(q.size()) == open;
}

void idle(Round& r, ScoreSystem& s, int steps) {
    InputManager in;
    for (int i = 0; i < steps; ++i) r.update(in, s, static_cast<float>(FIXED_DT));
}

} // namespace

// --- the level gate --------------------------------------------------------

TEST(rocks_sit_still_for_the_first_nine_rounds) {
    for (int lvl = 1; lvl <= 9; ++lvl) {
        Round r;
        r.load(parse(kOpen, lvl));
        CHECK(!r.rocksMove());
        for (const auto& rk : r.rocks()) CHECK(!rk.moving());

        ScoreSystem s; s.newGame();
        const float x0 = r.rocks()[0].pos.x;
        idle(r, s, 600);
        CHECK_NEAR(r.rocks()[0].pos.x, x0, 0.0001);   // not a pixel
    }
}

TEST(rocks_start_moving_at_round_ten) {
    for (int lvl = 10; lvl <= 14; ++lvl) {
        Round r;
        r.load(parse(kOpen, lvl));
        CHECK(r.rocksMove());
        CHECK(r.rocks()[0].moving());

        ScoreSystem s; s.newGame();
        const float x0 = r.rocks()[0].pos.x;
        idle(r, s, 120);
        CHECK(std::fabs(r.rocks()[0].pos.x - x0) > 1.f);
    }
}

TEST(a_moving_rock_never_changes_row) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kOpen, 12));
    const float y0 = r.rocks()[0].pos.y;
    for (int i = 0; i < 2000; ++i) {
        idle(r, s, 1);
        CHECK_NEAR(r.rocks()[0].pos.y, y0, 0.0001);   // horizontal, and only that
    }
}

// --- staying where they belong --------------------------------------------

TEST(a_moving_rock_stays_inside_its_own_beat_and_turns_round) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kOpen, 12));

    const Rock home = r.rocks()[0];
    CHECK(home.maxX > home.minX);

    bool sawLeftEnd = false, sawRightEnd = false;
    for (int i = 0; i < 4000; ++i) {
        idle(r, s, 1);
        const Rock& rk = r.rocks()[0];
        CHECK(rk.pos.x >= home.minX - 0.001f);
        CHECK(rk.pos.x <= home.maxX + 0.001f);
        if (rk.pos.x <= home.minX + 0.001f) sawLeftEnd = true;
        if (rk.pos.x >= home.maxX - 0.001f) sawRightEnd = true;
    }
    CHECK(sawLeftEnd);      // it really does reverse at both ends
    CHECK(sawRightEnd);
}

TEST(a_moving_rock_never_slides_into_a_wall_or_off_the_map) {
    for (int lvl = 10; lvl <= 12; ++lvl) {
        Round r; ScoreSystem s; s.newGame();
        r.load(shipped(lvl, lvl), 0xC0DEu + static_cast<uint32_t>(lvl));
        const TileMap& m = r.map();
        for (int i = 0; i < 3000; ++i) {
            idle(r, s, 1);
            for (const auto& rk : r.rocks()) {
                const int tx = TileMap::toTile(rk.pos.x), ty = TileMap::toTile(rk.pos.y);
                CHECK(tx >= 0 && ty >= 0 && tx < m.width() && ty < m.height());
                CHECK(m.isRoad(tx, ty));
            }
        }
    }
}

TEST(a_rock_with_nowhere_to_go_simply_stays_put) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kWedged, 12));
    CHECK(r.rocksMove());                    // the round does move its rocks
    CHECK(!r.rocks()[0].moving());           // this one has no room to
    const float x0 = r.rocks()[0].pos.x;
    idle(r, s, 600);
    CHECK_NEAR(r.rocks()[0].pos.x, x0, 0.0001);
}

TEST(no_two_rocks_ever_share_a_patrol_tile) {
    for (int lvl = 10; lvl <= 12; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 0x99u + static_cast<uint32_t>(lvl));
        std::set<std::pair<int,int>> claimed;
        for (const auto& rk : r.rocks()) {
            const int ty = TileMap::toTile(rk.pos.y);
            for (int tx = TileMap::toTile(rk.minX); tx <= TileMap::toTile(rk.maxX); ++tx)
                CHECK(claimed.insert({ tx, ty }).second);
        }
    }
}

// --- fairness --------------------------------------------------------------

TEST(a_patrol_can_never_cut_the_maze_in_two) {
    // The guarantee that matters: seal every tile every rock can ever occupy,
    // all at once -- the worst case the player could ever face -- and the maze
    // must still be one connected piece.
    for (int lvl = 10; lvl <= 12; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 0x4242u + static_cast<uint32_t>(lvl));
        std::set<std::pair<int,int>> everywhere;
        for (const auto& rk : r.rocks()) {
            const int ty = TileMap::toTile(rk.pos.y);
            for (int tx = TileMap::toTile(rk.minX); tx <= TileMap::toTile(rk.maxX); ++tx)
                everywhere.insert({ tx, ty });
        }
        CHECK(connectedWithout(r.map(), everywhere));
    }
}

TEST(a_rock_is_refused_the_tile_that_would_plug_a_corridor) {
    Round r;
    r.load(parse(kNeck, 12));
    // The rock sits at (2,2) with the wall column at x=5.  Wherever it is
    // allowed to go, the maze stays whole.
    std::set<std::pair<int,int>> everywhere;
    for (const auto& rk : r.rocks()) {
        const int ty = TileMap::toTile(rk.pos.y);
        for (int tx = TileMap::toTile(rk.minX); tx <= TileMap::toTile(rk.maxX); ++tx)
            everywhere.insert({ tx, ty });
    }
    CHECK(connectedWithout(r.map(), everywhere));
}

TEST(a_rock_never_wanders_onto_the_player_spawn_or_into_the_pen) {
    for (int lvl = 10; lvl <= 12; ++lvl) {
        Round r;
        r.load(shipped(lvl, lvl), 0x7777u + static_cast<uint32_t>(lvl));
        const LevelData& d = r.level();
        for (const auto& rk : r.rocks()) {
            const int ty = TileMap::toTile(rk.pos.y);
            for (int tx = TileMap::toTile(rk.minX); tx <= TileMap::toTile(rk.maxX); ++tx) {
                CHECK(!(tx == d.playerSpawn.tx && ty == d.playerSpawn.ty));
                for (const auto& pn : d.enemyPen) CHECK(!(tx == pn.tx && ty == pn.ty));
            }
        }
    }
}

TEST(a_rock_is_slow_enough_to_drive_around) {
    // Fairness in one number: the player has to be able to out-pace a rock by
    // a wide margin, or a patrol becomes an ambush.
    Round r;
    r.load(parse(kOpen, 12));
    CHECK(r.rocks()[0].speed * 3.f < r.level().playerSpeed);
}

// --- everything else keeps working ----------------------------------------

TEST(the_pursuit_map_follows_the_rocks_as_they_move) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kOpen, 12));

    auto rockTile = [&] {
        return std::make_pair(TileMap::toTile(r.rocks()[0].pos.x),
                              TileMap::toTile(r.rocks()[0].pos.y));
    };
    const auto first = rockTile();
    CHECK(r.enemyMap().isWall(first.first, first.second));

    // Drive it far enough to change tile, then check the cars' map moved too.
    for (int i = 0; i < 4000 && rockTile() == first; ++i) idle(r, s, 1);
    const auto now = rockTile();
    CHECK(now != first);
    CHECK(r.enemyMap().isWall(now.first, now.second));
    CHECK(!r.enemyMap().isWall(first.first, first.second));   // and let go of the old one
    // The player's own map is untouched: a rock kills, it does not block.
    CHECK(r.map().isRoad(now.first, now.second));
}

TEST(a_moving_rock_still_kills_on_contact) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kRow, 12));

    const float home = r.rocks()[0].pos.x;
    CHECK(r.rocks()[0].moving());
    CHECK_EQ(r.rocks()[0].dirX, -1);            // its only room is towards the player

    InputManager in;                            // no steering: the car just rolls
    bool died = false;
    float travelled = 0.f;
    for (int i = 0; i < 1200 && !died; ++i) {
        const auto ev = r.update(in, s, static_cast<float>(FIXED_DT));
        travelled = std::fabs(r.rocks()[0].pos.x - home);
        if (ev.playerDied) {
            died = true;
            CHECK(ev.cause == DeathCause::Rock);
        }
    }
    CHECK(died);
    CHECK(travelled > 1.f);                     // the rock was genuinely on the move
}

TEST(a_lost_life_puts_every_rock_back_where_it_started) {
    Round r; ScoreSystem s; s.newGame();
    r.load(parse(kOpen, 12));
    const float x0 = r.rocks()[0].pos.x;
    const int   d0 = r.rocks()[0].dirX;

    idle(r, s, 500);
    CHECK(std::fabs(r.rocks()[0].pos.x - x0) > 1.f);

    r.restartAfterDeath();
    CHECK_NEAR(r.rocks()[0].pos.x, x0, 0.0001);
    CHECK_EQ(r.rocks()[0].dirX, d0);
    CHECK(r.rocks()[0].moving());              // and it is still a patrolling rock
}

// --- the whole thing running together --------------------------------------

TEST(across_the_shipped_rounds_the_player_is_practically_never_boxed_in) {
    // The end-to-end guarantee, measured rather than asserted in the abstract:
    // play every normal round with a car wandering the maze, with the real
    // pursuit, the real rocks (patrolling from round 10) and the real turbos,
    // and count how much of the time the player has no way out at all.
    //
    // It is not zero, and claiming otherwise would be dishonest: a trap can
    // form for a few frames before the rule breaks it up.  What the rule
    // guarantees is that it never *lasts*.  A challenging stage's closing
    // chase is excluded -- that one is meant to be lost.
    for (int lvl = 1; lvl <= 12; ++lvl) {
        Round r; ScoreSystem sc; sc.newGame();
        r.load(shipped(lvl, lvl), 0xABCDEF01u ^ static_cast<uint32_t>(lvl));
        if (r.isChallenge()) continue;

        InputManager in;
        uint32_t rng = 1234u + static_cast<uint32_t>(lvl);
        auto rnd = [&] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
        const Action dirs[4] = { Action::Up, Action::Down, Action::Left, Action::Right };

        int steps = 0, trapped = 0, routes = 0, longestRun = 0, run = 0;
        for (int i = 0; i < 4200; ++i) {
            if (i % 25 == 0) {
                for (Action a : dirs) in.setFromExternal(a, false);
                in.setFromExternal(dirs[rnd() % 4], true);
            }
            const auto ev = r.update(in, sc, static_cast<float>(FIXED_DT));
            if (ev.playerDied) { r.restartAfterDeath(); run = 0; continue; }
            if (r.launchCountdown() > 0.f) continue;

            ++steps;
            routes += r.escapeState().routes;
            if (r.escapeState().trapped) { ++trapped; longestRun = std::max(longestRun, ++run); }
            else run = 0;
        }
        CHECK(steps > 2000);
        // Boxed in for under 3% of the round...
        CHECK(trapped * 100 < steps * 3);
        // ...never for as long as a second at a stretch...
        std::printf("      L%d trapped=%d/%d longest=%d\n", lvl, trapped, steps, longestRun);
        CHECK(longestRun < 60);
        // ...and two ways out on average, so the pack is not simply keeping
        // its distance.
        CHECK(routes >= steps * 2);
    }
}
