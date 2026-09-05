#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "gameplay/LuckyFlagBonusCalculator.h"
#include "core/InputManager.h"
#include "world/CollisionSystem.h"
#include <algorithm>
#include <cmath>

namespace rx {

namespace {
constexpr float FLAG_PICKUP_RADIUS = 9.f;
constexpr float ROCK_HIT_RADIUS    = 9.f;
constexpr float SMOKE_TRAIL_OFFSET = 10.f;   // puffs appear behind the car
constexpr float ENEMY_HIT_RADIUS   = 11.f;
// Every car in the round waits out the same countdown and then launches at
// the same instant -- the pack leaves the pen together, never in a trickle.
constexpr float ENEMY_LAUNCH_DELAY = 1.4f;
// The challenging-stage chase gets a much shorter beat -- just enough that the
// alarm registers and the player is not killed on the very frame the cars
// appear, which is what happens if they were standing near the pen.
constexpr float CHASE_LAUNCH_DELAY = 0.5f;

// --- moving rocks (round 10+) ---------------------------------------------
// Slow enough to read.  At 0.30 px/step a rock crosses a tile in about nine
// tenths of a second, against a player covering it in a fifth: the rock is
// always something to be driven around, never something that lands on you.
constexpr float ROCK_PATROL_SPEED = 0.30f;
// How far either side of home a rock may wander.  Two tiles is enough to
// close and reopen a corridor mouth without a rock ever becoming a wall you
// have to memorise the whole length of.
constexpr int   ROCK_PATROL_REACH = 2;
}

void Round::load(const LevelData& level, uint32_t flagSeed) {
    level_ = level;
    flagSeed_ = flagSeed;
    camera_.setViewport(VIEW_W, VIEW_H);
    // The navigation graph describes the maze, not the rocks: a car routes as
    // if the corridors were clear and finds out about a rock by hitting it.
    nav_.build(level_.map);
    restart();
}

void Round::buildEnemyMap() {
    // Start from the maze, then close off wherever the rocks currently are.
    // Once the rocks patrol this has to be rebuilt as they slide, which is why
    // it reads the live rocks rather than the level's authored list.
    enemyMap_ = level_.map;
    for (const auto& r : rocks_)
        enemyMap_.set(TileMap::toTile(r.pos.x), TileMap::toTile(r.pos.y), Tile::Wall);
}

void Round::placeCarsInPen(float speed, float delay) {
    enemies_.clear();

    const int penSize = static_cast<int>(level_.enemyPen.size());
    if (penSize == 0) return;

    // One car per pen tile, packed from the head of the lane.  A round can ask
    // for fewer cars than the pen holds, but never more than fit.
    const int count = std::clamp(level_.enemyCount, 0, penSize);

    // The pen is a straight lane; point the cars along it, towards the end
    // nearer the player, so they pull out of it facing the right way.
    const Vec2 head = Maze::tileCenterWorld(level_.enemyPen.front().tx,
                                            level_.enemyPen.front().ty);
    const Vec2 tail = Maze::tileCenterWorld(level_.enemyPen.back().tx,
                                            level_.enemyPen.back().ty);
    const bool horizontal = std::fabs(tail.x - head.x) >= std::fabs(tail.y - head.y);
    const Vec2 player = Maze::tileCenterWorld(level_.playerSpawn.tx, level_.playerSpawn.ty);

    Direction facing;
    if (horizontal) facing = (player.x >= head.x) ? Direction::Right : Direction::Left;
    else            facing = (player.y >= head.y) ? Direction::Down  : Direction::Up;

    for (int i = 0; i < count; ++i) {
        const auto& tile = level_.enemyPen[static_cast<size_t>(i)];
        Enemy e;
        e.spawn(Maze::tileCenterWorld(tile.tx, tile.ty), facing, speed, delay);
        enemies_.push_back(e);
    }
}

void Round::spawnEnemies() {
    enemies_.clear();
    chaseActive_ = false;

    EnemyAI::Tuning t;
    // Higher rounds route further out and wander less: the pack tightens up.
    t.pathfindRangeTiles = 10 + level_.difficulty;
    t.randomTurnChance   = std::max(0.05f, 0.20f - 0.02f * level_.difficulty);
    ai_.reset(0x9E3779B9u ^ static_cast<uint32_t>(level_.difficulty * 2654435761u), t);

    // A challenging stage starts with the track clear.  Its cars are held in
    // the pen and only released when the tank runs dry -- see
    // startChallengeChase().
    if (isChallenge()) return;

    placeCarsInPen(level_.enemySpeed, ENEMY_LAUNCH_DELAY);
}

void Round::startChallengeChase() {
    chaseActive_ = true;

    // Twice the player's speed, out of the pen after the briefest of beats.
    // This is not a chase to be won: it is the stage's closing bell, and the
    // only way out is to have already taken the flags.
    placeCarsInPen(level_.playerSpeed * CHASE_SPEED_MULTIPLE, CHASE_LAUNCH_DELAY);

    EnemyAI::Tuning t;
    t.pathfindRangeTiles = MAP_W + MAP_H;   // route properly from anywhere
    t.randomTurnChance   = 0.02f;           // and barely wander
    // The fairness rule is deliberately off here.  Everywhere else the pack
    // must leave a way out; this chase is the stage's closing bell and is
    // meant to be lost.  Policing it would quietly undo the whole feature.
    t.antiTrap           = false;
    ai_.reset(0xC0FFEEu ^ static_cast<uint32_t>(level_.difficulty * 40503u), t);
}

void Round::restart() {
    placeFlags();
    placeTurbos();
    resetActors();
}

void Round::restart(uint32_t flagSeed) {
    flagSeed_ = flagSeed;
    restart();
}

void Round::placeFlags() {
    flags_.clear();
    required_ = 0;
    collected_ = 0;

    // Count what the level asks for, then either honour its layout or shuffle
    // one of the same shape.
    int normal = 0, special = 0, lucky = 0;
    for (const auto& fs : level_.flags) {
        if (fs.kind == 1)      ++special;
        else if (fs.kind == 2) ++lucky;
        else                   ++normal;
    }

    std::vector<FlagSpawn> spawns;
    if (flagSeed_ == 0) {
        spawns = level_.flags;                 // authored positions
    } else {
        FlagPlacer::Request req;
        req.map         = &level_.map;
        req.playerSpawn = level_.playerSpawn;
        req.normal      = normal;
        req.special     = special;
        req.lucky       = lucky;

        // Nothing may share a tile with a rock or sit inside the enemy pen.
        req.reserved = level_.rocks;
        req.reserved.insert(req.reserved.end(),
                            level_.enemyPen.begin(), level_.enemyPen.end());

        spawns = FlagPlacer::place(req, flagSeed_);
        if (spawns.empty()) spawns = level_.flags;   // never leave a round empty
    }

    for (const auto& fs : spawns) {
        Flag f;
        f.pos  = Maze::tileCenterWorld(fs.tx, fs.ty);
        f.type = static_cast<FlagType>(fs.kind);
        flags_.push_back(f);
        if (f.type == FlagType::Normal) ++required_;
    }
}

void Round::placeTurbos() {
    turbos_.clear();

    const int wanted = TurboRules::countForLevel(level_.levelNumber);
    if (wanted <= 0) return;                  // rounds 1-4 have none at all

    // Turbos go through the flag placer, so they inherit its fairness rules
    // wholesale: reachable road only, clear of the spawn, spread out, and
    // never on top of a rock, the pen or a flag.
    FlagPlacer::Request req;
    req.map         = &level_.map;
    req.playerSpawn = level_.playerSpawn;
    req.reserved    = level_.rocks;
    req.reserved.insert(req.reserved.end(),
                        level_.enemyPen.begin(), level_.enemyPen.end());
    for (const auto& f : flags_)
        req.reserved.push_back({ TileMap::toTile(f.pos.x), TileMap::toTile(f.pos.y) });

    // A seed of its own, derived from the flag seed, so the boosts move with
    // the flags each round instead of landing in the same places every time.
    const uint32_t seed = (flagSeed_ ? flagSeed_ : 0x51ED51EDu) * 2246822519u + 0x9E3779B9u;

    for (const auto& t : FlagPlacer::pickTiles(req, wanted, seed))
        turbos_.push_back(Turbo{ Maze::tileCenterWorld(t.tx, t.ty), false });
}

void Round::assignRockPatrols() {
    if (!rocksMove()) return;

    const TileMap& m = level_.map;

    // The fairness guarantee.  A rock is lethal, so for the player it is a
    // wall wherever it happens to be; if a patrol could put one in a corridor
    // that is the only way through, the maze would come apart underneath the
    // player.  So a tile only joins a patrol if sealing it -- together with
    // every tile already claimed -- leaves the whole maze still connected.
    // Because the check is cumulative, no combination of rock positions can
    // ever cut the map in two.
    std::vector<TileSpawn> sealed;
    for (const auto& rs : level_.rocks) sealed.push_back(rs);

    // Tiles a rock may never slide onto: the player's spawn, the pen, and any
    // tile already claimed -- which covers both the other rocks' homes and the
    // stretches they have already taken, so two beats can never overlap.
    auto occupied = [&](int tx, int ty) {
        if (tx == level_.playerSpawn.tx && ty == level_.playerSpawn.ty) return true;
        for (const auto& p : level_.enemyPen) if (p.tx == tx && p.ty == ty) return true;
        for (const auto& t : sealed)          if (t.tx == tx && t.ty == ty) return true;
        return false;
    };

    auto stillConnected = [&](int tx, int ty) {
        std::vector<char> blocked(static_cast<size_t>(m.width()) * m.height(), 0);
        for (const auto& t : sealed) blocked[static_cast<size_t>(t.ty) * m.width() + t.tx] = 1;
        blocked[static_cast<size_t>(ty) * m.width() + tx] = 1;

        int open = 0, start = -1;
        for (int y = 0; y < m.height(); ++y)
            for (int x = 0; x < m.width(); ++x) {
                const size_t i = static_cast<size_t>(y) * m.width() + x;
                if (m.isRoad(x, y) && !blocked[i]) { ++open; if (start < 0) start = static_cast<int>(i); }
            }
        if (start < 0) return false;

        std::vector<char> seen(blocked.size(), 0);
        std::vector<int>  queue{ start };
        seen[static_cast<size_t>(start)] = 1;
        for (size_t head = 0; head < queue.size(); ++head) {
            const int x = queue[head] % m.width(), y = queue[head] / m.width();
            for (Direction d : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
                const int nx = x + dirDX(d), ny = y + dirDY(d);
                if (nx < 0 || ny < 0 || nx >= m.width() || ny >= m.height()) continue;
                const size_t n = static_cast<size_t>(ny) * m.width() + nx;
                if (seen[n] || blocked[n] || !m.isRoad(nx, ny)) continue;
                seen[n] = 1;
                queue.push_back(static_cast<int>(n));
            }
        }
        return static_cast<int>(queue.size()) == open;
    };

    for (auto& rock : rocks_) {
        const int tx = TileMap::toTile(rock.pos.x);
        const int ty = TileMap::toTile(rock.pos.y);

        // Grow the beat outwards a tile at a time, stopping at the first tile
        // that is walled, taken, or would cut the maze.
        int left = 0, right = 0;
        for (int k = 1; k <= ROCK_PATROL_REACH; ++k) {
            const int nx = tx - k;
            if (!m.isRoad(nx, ty) || occupied(nx, ty) || !stillConnected(nx, ty)) break;
            sealed.push_back({ nx, ty });
            left = k;
        }
        for (int k = 1; k <= ROCK_PATROL_REACH; ++k) {
            const int nx = tx + k;
            if (!m.isRoad(nx, ty) || occupied(nx, ty) || !stillConnected(nx, ty)) break;
            sealed.push_back({ nx, ty });
            right = k;
        }

        if (left == 0 && right == 0) continue;      // nowhere to go: stays put

        rock.minX  = Maze::tileCenterWorld(tx - left,  ty).x;
        rock.maxX  = Maze::tileCenterWorld(tx + right, ty).x;
        rock.speed = ROCK_PATROL_SPEED;
        // Start whichever way there is more room, so a lopsided beat opens
        // with the longer half.
        rock.dirX  = (right >= left) ? 1 : -1;
    }
}

void Round::restartAfterDeath() {
    // Everything that moves goes back to its starting place; the flags do not.
    resetActors();
}

void Round::resetActors() {
    rocks_.clear();
    for (const auto& rs : level_.rocks) {
        Rock r;
        r.pos = Maze::tileCenterWorld(rs.tx, rs.ty);
        r.minX = r.maxX = r.pos.x;              // stationary until told otherwise
        rocks_.push_back(r);
    }
    assignRockPatrols();
    buildEnemyMap();                            // the rocks are back home

    turbo_.reset();                             // a new car starts unboosted
    smoke_.reset();
    fuel_.reset(level_.fuel, level_.fuelDrain);   // a new car has a full tank
    chaseActive_ = false;
    spawnEnemies();

    player_.spawn(Maze::tileCenterWorld(level_.playerSpawn.tx, level_.playerSpawn.ty),
                  Direction::Right, level_.playerSpeed);
    camera_.snapTo(player_.position());
}

Vec2 Round::smokeEmitPoint() const {
    const Vec2& p = player_.position();
    const Direction back = opposite(player_.direction());
    return Vec2{ p.x + dirDX(back) * SMOKE_TRAIL_OFFSET,
                 p.y + dirDY(back) * SMOKE_TRAIL_OFFSET };
}

Round::Events Round::update(const InputManager& input, ScoreSystem& score, float dt) {
    Events ev;
    if (!player_.alive()) return ev;

    // Smoke first: it must come out behind the car's *current* position.
    ev.smokePuffed = smoke_.emit(smokeEmitPoint(), fuel_, input.down(Action::Smoke), dt);
    smoke_.update(dt);

    fuel_.update(dt);

    // The boost is a speed, not a movement mode: the car still turns on the
    // grid and still stops at walls, it just covers ground faster.
    turbo_.update(dt);
    applyPlayerSpeed();
    player_.update(map(), input.desiredDirection(), dt);

    updateRocks();

    EnemyAI::Context ctx;
    ctx.map         = &enemyMap_;   // where a car may go
    ctx.planMap     = &map();       // what a car thinks the maze is
    ctx.nav         = &nav_;
    ctx.smoke       = &smoke_;
    ctx.playerPos   = player_.position();
    ctx.playerSpeed = player_.speed();
    ctx.playerAlive = player_.alive();
    ctx.frozen      = enemiesFrozen_;
    ai_.update(enemies_, ctx, dt);

    collectFlags(score, ev);
    collectTurbos(ev);
    checkRocks(ev);
    if (!ev.playerDied) checkEnemies(ev);

    if (!ev.playerDied && fuel_.empty()) {
        if (isChallenge()) {
            // The tank running dry does not end a challenging stage -- it
            // lets the cars out.  The player keeps driving (and can still
            // finish the flags), but now with the pack on them.
            if (!chaseActive_) {
                startChallengeChase();
                ev.chaseStarted = true;
            }
        } else {
            player_.kill();
            ev.playerDied = true;
            ev.cause = DeathCause::OutOfFuel;
        }
    }

    camera_.centerOn(player_.position());
    return ev;
}

void Round::collectFlags(ScoreSystem& score, Events& ev) {
    for (auto& f : flags_) {
        if (f.collected) continue;
        if (!CollisionSystem::circlesOverlap(player_.position(), f.pos, FLAG_PICKUP_RADIUS))
            continue;

        f.collected = true;
        switch (f.type) {
            case FlagType::Normal:
                score.awardNormalFlag();
                ++collected_;
                ++ev.flagsTaken;
                ev.flagSequence = score.flagsScored();
                break;
            case FlagType::Special:
                // Scores nothing itself; doubles every flag taken after it.
                score.activateSpecialFlag();
                ev.specialTaken = true;
                break;
            case FlagType::Lucky: {
                const int bonus = LuckyFlagBonusCalculator::bonusFor(fuel_.fuel(),
                                                                     fuel_.capacity());
                score.addBonus(bonus);
                ev.bonusAwarded = bonus;
                ev.luckyTaken = true;
                break;
            }
        }
    }
    if (collected_ >= required_ && required_ > 0) ev.roundComplete = true;
}

void Round::applyPlayerSpeed() {
    player_.setSpeed(turbo_.speedFor(level_.playerSpeed));
}

void Round::collectTurbos(Events& ev) {
    for (auto& t : turbos_) {
        if (t.collected) continue;
        if (!CollisionSystem::circlesOverlap(player_.position(), t.pos,
                                             TurboRules::PICKUP_RADIUS))
            continue;
        t.collected = true;
        turbo_.activate();
        applyPlayerSpeed();
        ev.turboTaken = true;
    }
}

void Round::updateRocks() {
    if (!rocksMove()) return;

    bool anyMoved = false;
    for (auto& r : rocks_) {
        if (!r.moving()) continue;
        const int before = TileMap::toTile(r.pos.x);
        r.step();
        if (TileMap::toTile(r.pos.x) != before) anyMoved = true;
    }
    // The pursuit cars treat rocks as solid, so their map has to follow the
    // rocks around -- and so does the escape analysis built on top of it.
    if (anyMoved) buildEnemyMap();
}

void Round::checkRocks(Events& ev) {
    for (const auto& r : rocks_) {
        if (!CollisionSystem::circlesOverlap(player_.position(), r.pos, ROCK_HIT_RADIUS))
            continue;
        player_.kill();
        ev.playerDied = true;
        ev.cause = DeathCause::Rock;
        return;
    }
}

float Round::launchCountdown() const {
    float longest = 0.f;
    for (const auto& e : enemies_)
        if (e.state() == EnemyState::Spawning) longest = std::max(longest, e.spawnTimer());
    return longest;
}

void Round::checkEnemies(Events& ev) {
    for (const auto& e : enemies_) {
        // A stunned car is harmless -- that is the whole point of the smoke.
        if (!e.dangerous()) continue;
        if (!CollisionSystem::circlesOverlap(player_.position(), e.position(), ENEMY_HIT_RADIUS))
            continue;
        player_.kill();
        ev.playerDied = true;
        ev.cause = DeathCause::Enemy;
        return;
    }
}

void Round::debugCollectAllFlags(ScoreSystem& score) {
    for (auto& f : flags_) {
        if (f.collected || f.type != FlagType::Normal) continue;
        f.collected = true;
        score.awardNormalFlag();
        ++collected_;
    }
}

} // namespace rx
