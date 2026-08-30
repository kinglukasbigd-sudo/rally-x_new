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
}

void Round::load(const LevelData& level) {
    level_ = level;
    camera_.setViewport(VIEW_W, VIEW_H);
    buildEnemyMap();
    // The navigation graph describes the maze, not the rocks: a car routes as
    // if the corridors were clear and finds out about a rock by hitting it.
    nav_.build(level_.map);
    restart();
}

void Round::buildEnemyMap() {
    // Start from the maze, then close off every rock tile.
    enemyMap_ = level_.map;
    for (const auto& rs : level_.rocks)
        enemyMap_.set(rs.tx, rs.ty, Tile::Wall);
}

void Round::spawnEnemies() {
    enemies_.clear();
    // Challenging stages run with no pursuit at all; see ChallengeStage.
    if (isChallenge()) return;

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
        e.spawn(Maze::tileCenterWorld(tile.tx, tile.ty), facing,
                level_.enemySpeed, ENEMY_LAUNCH_DELAY);
        enemies_.push_back(e);
    }

    EnemyAI::Tuning t;
    // Higher rounds route further out and wander less: the pack tightens up.
    t.pathfindRangeTiles = 10 + level_.difficulty;
    t.randomTurnChance   = std::max(0.05f, 0.20f - 0.02f * level_.difficulty);
    ai_.reset(0x9E3779B9u ^ static_cast<uint32_t>(level_.difficulty * 2654435761u), t);
}

void Round::restart() {
    flags_.clear();
    required_ = 0;
    collected_ = 0;

    for (const auto& fs : level_.flags) {
        Flag f;
        f.pos  = Maze::tileCenterWorld(fs.tx, fs.ty);
        f.type = static_cast<FlagType>(fs.kind);
        flags_.push_back(f);
        if (f.type == FlagType::Normal) ++required_;
    }
    resetActors();
}

void Round::restartAfterDeath() {
    // Everything that moves goes back to its starting place; the flags do not.
    resetActors();
}

void Round::resetActors() {
    rocks_.clear();
    for (const auto& rs : level_.rocks)
        rocks_.push_back(Rock{ Maze::tileCenterWorld(rs.tx, rs.ty) });

    smoke_.reset();
    fuel_.reset(level_.fuel, level_.fuelDrain);   // a new car has a full tank
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

    player_.update(map(), input.desiredDirection(), dt);

    EnemyAI::Context ctx;
    ctx.map         = &enemyMap_;   // where a car may go
    ctx.planMap     = &map();       // what a car thinks the maze is
    ctx.nav         = &nav_;
    ctx.smoke       = &smoke_;
    ctx.playerPos   = player_.position();
    ctx.playerAlive = player_.alive();
    ctx.frozen      = enemiesFrozen_;
    ai_.update(enemies_, ctx, dt);

    collectFlags(score, ev);
    checkRocks(ev);
    if (!ev.playerDied) checkEnemies(ev);

    if (!ev.playerDied && fuel_.empty()) {
        player_.kill();
        ev.playerDied = true;
        ev.cause = DeathCause::OutOfFuel;
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
