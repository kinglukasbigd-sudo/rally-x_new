#include "TestFramework.h"
#include "ai/EscapeAnalyzer.h"
#include "ai/EnemyAI.h"
#include "ai/NavigationGraph.h"
#include "entities/Enemy.h"
#include "gameplay/SmokeSystem.h"
#include "world/LevelLoader.h"
#include "world/Maze.h"
#include <algorithm>
#include <cmath>

using namespace rx;

namespace {

// A wide open room with a pillar in it: plenty of room, plenty of routes.
const char* kRoom =
    "name ROOM\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "##########\n"
    "#........#\n"
    "#........#\n"
    "#...##...#\n"
    "#...##...#\n"
    "#........#\n"
    "#........#\n"
    "#........#\n"
    "#........#\n"
    "##########\n";

// One long, one-tile corridor: the least forgiving shape in the game, and the
// easiest place to shut a player in.  It is deliberately long, because in a
// corridor a player only owns the half of the gap they would reach first --
// a short corridor is trapped by definition and would prove nothing.
const char* kCorridor =
    "name CORRIDOR\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "##########################\n"
    "#........................#\n"
    "##########################\n";

// A dead-end pocket of exactly four tiles hanging off a long corridor.  The
// pocket is too small to count as an escape even when nothing is blocking it.
const char* kPocket =
    "name POCKET\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "##############\n"
    "#............#\n"
    "#####.########\n"
    "#####.########\n"
    "##############\n";

// A ring: one loop of corridor around a solid block, which is the shape
// Rally-X mazes are actually built from.  Two cars can pincer a player here,
// but there is always somewhere for one of them to give way to -- unlike a
// blind corridor, where being cornered is the maze's doing and not the AI's.
const char* kRing =
    "name RING\ntype NORMAL\nfuel 100\nfuelDrain 0\nplayerSpeed 1\nenemySpeed 1\nmaze\n"
    "######################\n"
    "#....................#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#.##################.#\n"
    "#....................#\n"
    "######################\n";

LevelData parse(const char* t) {
    LevelData d;
    CHECK(LevelLoader::loadFromString(t, d));
    return d;
}

Vec2 tile(int tx, int ty) { return Maze::tileCenterWorld(tx, ty); }

using P = EscapeAnalyzer::Pursuer;

} // namespace

// --- the analysis itself ---------------------------------------------------

TEST(an_empty_room_is_all_escape_and_no_trap) {
    LevelData d = parse(kRoom);
    const auto r = EscapeAnalyzer::analyze(d.map, tile(2, 2), 1.f, {});
    CHECK(!r.trapped);
    CHECK(r.routes >= 2);
    CHECK(r.freeTiles > 20);
}

TEST(one_distant_car_does_not_take_the_room_away) {
    LevelData d = parse(kRoom);
    std::vector<P> cars{ P{ tile(8, 8), 1.f, true } };
    const auto r = EscapeAnalyzer::analyze(d.map, tile(2, 2), 1.f, cars);
    CHECK(!r.trapped);
    CHECK(r.routes >= 1);
}

TEST(a_car_each_side_in_a_corridor_is_a_trap) {
    LevelData d = parse(kCorridor);
    // The player is in the middle of a one-tile corridor with a car closing
    // from each end: there is genuinely nowhere to go.
    std::vector<P> cars{ P{ tile(4, 1), 1.f, true }, P{ tile(14, 1), 1.f, true } };
    const auto r = EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars);
    CHECK(r.trapped);
    CHECK_EQ(r.routes, 0);
}

TEST(one_car_in_a_corridor_still_leaves_the_other_way_out) {
    LevelData d = parse(kCorridor);
    std::vector<P> cars{ P{ tile(4, 1), 1.f, true } };
    const auto r = EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars);
    CHECK(!r.trapped);
    CHECK_EQ(r.routes, 1);
    CHECK(r.dirOpen[static_cast<int>(Direction::Right)]);
}

TEST(a_stunned_car_is_not_holding_anybody_in) {
    LevelData d = parse(kCorridor);
    // The same two cars as the trap case, but one is sitting in smoke.
    std::vector<P> cars{ P{ tile(4, 1), 1.f, false }, P{ tile(14, 1), 1.f, true } };
    const auto r = EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars);
    CHECK(!r.trapped);
}

TEST(a_four_tile_pocket_does_not_count_as_an_escape) {
    LevelData d = parse(kPocket);
    // Standing at the mouth of the pocket with a car bearing down the corridor
    // from the left: the pocket is open, but it is a place to die in, not a
    // way out, so it must not be counted.
    std::vector<P> cars{ P{ tile(1, 1), 1.f, true } };
    const auto r = EscapeAnalyzer::analyze(d.map, tile(5, 1), 1.f, cars);
    CHECK(r.dirOpen[static_cast<int>(Direction::Down)]);          // it is open
    CHECK(r.dirTiles[static_cast<int>(Direction::Down)] < EscapeAnalyzer::MIN_ROUTE_TILES);
    // ...and the way out is the corridor to the right, not the pocket.
    CHECK(r.dirOpen[static_cast<int>(Direction::Right)]);
}

TEST(a_route_the_player_would_lose_the_race_to_is_not_a_route) {
    LevelData d = parse(kCorridor);
    // Both cars are to the right, but one is much faster and much nearer than
    // the player is to the far end: the right-hand "opening" is a losing race.
    std::vector<P> cars{ P{ tile(13, 1), 2.f, true } };
    const auto r = EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars);
    // Four tiles of clear corridor to the right, and the player still loses
    // every one of them: an opening is not a route if the race is already lost.
    CHECK(!r.dirOpen[static_cast<int>(Direction::Right)]);
    CHECK(r.dirOpen[static_cast<int>(Direction::Left)]);          // the real one
}

TEST(rocks_are_walls_as_far_as_an_escape_is_concerned) {
    LevelData d = parse(kCorridor);
    // The analyser is handed the map with the rocks filled in, exactly as the
    // Round hands it over.  A rock at (8,1) walls the corridor's right half.
    TileMap withRock = d.map;
    withRock.set(15, 1, Tile::Wall);
    std::vector<P> cars{ P{ tile(4, 1), 1.f, true } };

    const auto open   = EscapeAnalyzer::analyze(d.map,    tile(9, 1), 1.f, cars);
    const auto walled = EscapeAnalyzer::analyze(withRock, tile(9, 1), 1.f, cars);
    CHECK(!open.trapped);
    CHECK(walled.trapped);      // the rock caps the only long way out
}

TEST(ignoring_a_car_shows_which_one_shut_the_door) {
    LevelData d = parse(kCorridor);
    std::vector<P> cars{ P{ tile(4, 1), 1.f, true }, P{ tile(14, 1), 1.f, true } };
    CHECK(EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars).trapped);
    // Take either one away and the corridor opens again -- which is exactly
    // how EnemyAI decides who has to stand down.
    CHECK(!EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars, 0).trapped);
    CHECK(!EscapeAnalyzer::analyze(d.map, tile(9, 1), 1.f, cars, 1).trapped);
}

TEST(a_speculative_move_is_judged_before_it_is_taken) {
    LevelData d = parse(kCorridor);
    // One car is already left of the player; a second sits to the right, one
    // step away from closing the corridor.  Stepping left keeps the door open;
    // stepping toward the player shuts it.
    std::vector<P> cars{ P{ tile(4, 1), 1.f, true }, P{ tile(22, 1), 1.f, true } };
    const auto shut = EscapeAnalyzer::analyzeWithMove(d.map, tile(9, 1), 1.f, cars,
                                                      1, Direction::Left);
    const auto open = EscapeAnalyzer::analyzeWithMove(d.map, tile(9, 1), 1.f, cars,
                                                      1, Direction::Right);
    CHECK(shut.trapped);        // one step closer and the corridor is shut
    CHECK(!open.trapped);       // backing off keeps it open
    CHECK(shut.routes < open.routes);
}

// --- the AI acting on it ---------------------------------------------------

namespace {

// Drives a pack of cars for a while and reports the worst the player ever had.
struct Watch {
    int minRoutes    = 99;
    int trappedSteps = 0;
    int yieldSteps   = 0;
    int longestTrap  = 0;   // the worst unbroken stretch, in fixed steps
};

Watch runPack(const LevelData& d, const std::vector<Vec2>& starts, const Vec2& playerPos,
              bool antiTrap, int steps = 600) {
    TileMap plan = d.map;
    NavigationGraph nav;
    nav.build(plan);

    std::vector<Enemy> cars;
    for (const auto& s : starts) {
        Enemy e;
        e.spawn(s, Direction::Left, 1.f, 0.f);
        cars.push_back(e);
    }

    EnemyAI ai;
    EnemyAI::Tuning t;
    t.randomTurnChance = 0.f;          // no jitter: we want the pure pursuit
    t.antiTrap = antiTrap;
    ai.reset(12345u, t);

    EnemyAI::Context ctx;
    ctx.map = &plan;
    ctx.planMap = &plan;
    ctx.nav = &nav;
    ctx.playerPos = playerPos;         // a stationary player is the worst case
    ctx.playerSpeed = 1.f;

    Watch w;
    int run = 0;
    for (int i = 0; i < steps; ++i) {
        ai.update(cars, ctx, static_cast<float>(FIXED_DT));

        std::vector<P> snap;
        for (const auto& e : cars) snap.push_back(P{ e.position(), e.speed(), e.dangerous() });
        const auto r = EscapeAnalyzer::analyze(plan, playerPos, 1.f, snap);
        w.minRoutes = std::min(w.minRoutes, r.routes);
        if (r.trapped) { ++w.trappedSteps; w.longestTrap = std::max(w.longestTrap, ++run); }
        else run = 0;
        if (ai.yieldingCars() > 0) ++w.yieldSteps;
    }
    return w;
}

} // namespace

TEST(a_single_car_never_traps_a_parked_player_in_a_room) {
    LevelData d = parse(kRoom);
    const Watch w = runPack(d, { tile(8, 8) }, tile(2, 2), true);
    CHECK_EQ(w.trappedSteps, 0);
    CHECK(w.minRoutes >= 1);
}

TEST(a_whole_pack_cannot_seal_a_parked_player_into_a_room) {
    LevelData d = parse(kRoom);
    // Four cars converging on a player who never moves: the cruellest case the
    // fairness rule has to survive.
    const Watch w = runPack(d, { tile(8, 8), tile(1, 8), tile(8, 1), tile(6, 6) },
                            tile(2, 2), true);
    CHECK_EQ(w.trappedSteps, 0);
}

TEST(the_pack_still_closes_in_when_the_fairness_rule_is_off) {
    LevelData d = parse(kRoom);
    // The control: with antiTrap disabled the same four cars do box the player
    // in.  If this ever stops happening the test above has stopped proving
    // anything, so the two are kept together deliberately.
    const Watch off = runPack(d, { tile(8, 8), tile(1, 8), tile(8, 1), tile(6, 6) },
                              tile(2, 2), false);
    const Watch on  = runPack(d, { tile(8, 8), tile(1, 8), tile(8, 1), tile(6, 6) },
                              tile(2, 2), true);
    CHECK(off.trappedSteps > 0);
    CHECK(on.trappedSteps < off.trappedSteps);
}

TEST(a_pincer_on_a_ring_is_broken_rather_than_closed) {
    LevelData d = parse(kRing);
    // Two cars coming round opposite sides of the ring at a parked player: the
    // classic pincer, and the one situation where the pack could legitimately
    // seal someone in.  The rule has to break it, every step, for the whole run.
    const Watch on  = runPack(d, { tile(10, 1), tile(10, 14) }, tile(1, 7), true,  900);
    const Watch off = runPack(d, { tile(10, 1), tile(10, 14) }, tile(1, 7), false, 900);
    CHECK_EQ(on.trappedSteps, 0);
    CHECK(off.trappedSteps > 0);       // the control: unpoliced, they do seal it
    CHECK(on.yieldSteps > 0);          // somebody actually backed off
}

TEST(a_blind_corridor_is_the_mazes_doing_not_the_pursuits) {
    LevelData d = parse(kCorridor);
    // Honesty about the limit of the rule.  In a corridor with no loop and no
    // side turning, cars at both ends corner the player by geometry alone --
    // there is no move any car could make that would open a way out.  The rule
    // does not pretend otherwise; what it does is keep trying, so the cars are
    // still standing down rather than piling on.
    const Watch on = runPack(d, { tile(1, 1), tile(24, 1) }, tile(12, 1), true, 400);
    CHECK(on.yieldSteps > 0);
}

TEST(the_cars_keep_coming_and_do_not_simply_run_away) {
    LevelData d = parse(kRoom);
    // The fairness rule must not turn into "keep your distance".  Left alone
    // with one car and an open room, it should still close the gap.
    TileMap plan = d.map;
    NavigationGraph nav;
    nav.build(plan);

    std::vector<Enemy> cars(1);
    cars[0].spawn(tile(8, 8), Direction::Left, 1.f, 0.f);

    EnemyAI ai;
    EnemyAI::Tuning t;
    t.randomTurnChance = 0.f;
    ai.reset(999u, t);

    EnemyAI::Context ctx;
    ctx.map = &plan; ctx.planMap = &plan; ctx.nav = &nav;
    ctx.playerPos = tile(2, 2); ctx.playerSpeed = 1.f;

    const float before = std::fabs(cars[0].position().x - ctx.playerPos.x)
                       + std::fabs(cars[0].position().y - ctx.playerPos.y);
    float closest = before;
    for (int i = 0; i < 600; ++i) {
        ai.update(cars, ctx, static_cast<float>(FIXED_DT));
        closest = std::min(closest, std::fabs(cars[0].position().x - ctx.playerPos.x)
                                  + std::fabs(cars[0].position().y - ctx.playerPos.y));
    }
    CHECK(closest < before * 0.35f);   // it got a long way in
}

// --- the cases that only show up in a real maze -----------------------------

TEST(a_player_backed_into_a_corner_by_rocks_is_still_left_a_way_out) {
    LevelData d = parse(kRoom);
    // The map the cars are handed always has the rocks filled in, so this is
    // exactly what Round::enemyMap() looks like with two rocks laid down.  The
    // player is in the top-left corner with the rocks narrowing it further.
    TileMap withRocks = d.map;
    withRocks.set(4, 1, Tile::Wall);
    withRocks.set(1, 4, Tile::Wall);
    LevelData rocky = d;
    rocky.map = withRocks;

    const Watch w = runPack(rocky, { tile(8, 8), tile(8, 1), tile(1, 8) },
                            tile(1, 1), true, 900);
    // A corner this tight, with rocks narrowing it further, is a place the
    // player can momentarily run out of room -- and saying otherwise would be
    // a lie.  What must hold is that it never lasts: the pack is broken up
    // inside a third of a second, every time.
    CHECK(w.trappedSteps * 20 < 900);          // under 5% of the run
    CHECK(w.longestTrap < 20);
}

TEST(rocks_narrow_the_escape_without_the_rule_losing_track_of_them) {
    LevelData d = parse(kRoom);
    TileMap withRocks = d.map;
    withRocks.set(4, 1, Tile::Wall);
    withRocks.set(1, 4, Tile::Wall);

    // The same cars, the same player: the rocks must actually cost the player
    // room, or the analysis is ignoring them.
    std::vector<P> cars{ P{ tile(8, 8), 1.f, true } };
    const auto clear = EscapeAnalyzer::analyze(d.map,     tile(1, 1), 1.f, cars);
    const auto rocky = EscapeAnalyzer::analyze(withRocks, tile(1, 1), 1.f, cars);
    CHECK(rocky.freeTiles < clear.freeTiles);
}
