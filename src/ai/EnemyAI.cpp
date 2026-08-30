#include "ai/EnemyAI.h"
#include "ai/NavigationGraph.h"
#include "ai/Pathfinding.h"
#include "entities/Enemy.h"
#include "gameplay/SmokeSystem.h"
#include "world/TileMap.h"
#include <cstdlib>
#include <algorithm>

namespace rx {

void EnemyAI::reset(uint32_t seed, const Tuning& t) {
    tuning_ = t;
    rng_ = seed ? seed : 1u;
    lastDecisionTile_.clear();
    pendingTurn_.clear();
    bumpBan_.clear();
    bumpBanTimer_.clear();
}

uint32_t EnemyAI::nextRandom() {
    // xorshift32: small, deterministic, and good enough for arcade jitter.
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

float EnemyAI::randomUnit() {
    return static_cast<float>(nextRandom() & 0xFFFFFF) / static_cast<float>(0x1000000);
}

// Where the car wants to go.  Everything here works off the rock-blind map:
// a pursuit car has no idea the rocks are there.
Direction EnemyAI::decide(const Enemy& e, const Context& ctx, Direction banned) {
    const TileMap& plan = *ctx.planMap;
    const int ex = TileMap::toTile(e.position().x);
    const int ey = TileMap::toTile(e.position().y);
    const int px = TileMap::toTile(ctx.playerPos.x);
    const int py = TileMap::toTile(ctx.playerPos.y);

    auto moves = ctx.nav->legalMoves(ex, ey, e.direction());
    if (banned != Direction::None)
        moves.erase(std::remove(moves.begin(), moves.end(), banned), moves.end());

    // A small random turn now and then; without it every car converges onto
    // the same corridor and the pack stops feeling like traffic.
    if (moves.size() > 1 && randomUnit() < tuning_.randomTurnChance)
        return moves[nextRandom() % moves.size()];

    const int distance = std::abs(ex - px) + std::abs(ey - py);
    Direction want = (distance <= tuning_.pathfindRangeTiles)
                   ? Pathfinding::firstStep(plan, ex, ey, px, py, e.direction())
                   : Pathfinding::greedyStep(plan, ex, ey, px, py, e.direction());

    // Never re-pick the direction it just bounced off, if there is any choice.
    if (want == banned && !moves.empty()) return moves[nextRandom() % moves.size()];
    return want;
}

// Where the car can actually go, having just run into something.  This is the
// only decision that knows about rocks, which is what stops a car grinding
// against one forever.
Direction EnemyAI::escapeFrom(const Enemy& e, const Context& ctx) {
    const TileMap& map = *ctx.map;
    const int ex = TileMap::toTile(e.position().x);
    const int ey = TileMap::toTile(e.position().y);

    Direction options[4];
    int count = 0;
    const Direction back = opposite(e.direction());
    for (Direction d : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
        if (d == e.direction() || d == back) continue;
        if (map.isRoad(ex + dirDX(d), ey + dirDY(d))) options[count++] = d;
    }
    if (count > 0) return options[nextRandom() % count];
    if (map.isRoad(ex + dirDX(back), ey + dirDY(back))) return back;
    return Direction::None;
}

void EnemyAI::update(std::vector<Enemy>& enemies, const Context& ctx, float dt) {
    if (!ctx.map || !ctx.nav) return;
    if (lastDecisionTile_.size() != enemies.size()) {
        lastDecisionTile_.assign(enemies.size(), -1);
        pendingTurn_.assign(enemies.size(), Direction::None);
        bumpBan_.assign(enemies.size(), Direction::None);
        bumpBanTimer_.assign(enemies.size(), 0.f);
    }

    for (size_t i = 0; i < enemies.size(); ++i) {
        Enemy& e = enemies[i];
        if (!e.onTrack()) continue;

        // Smoke stalls a car the moment it touches one, wherever it is.
        if (ctx.smoke && e.dangerous() && ctx.smoke->contains(e.position(), 4.f))
            e.stun(tuning_.stunSeconds);

        e.tickStun(dt);
        if (e.stunned()) continue;
        if (ctx.frozen)  continue;                      // debug freeze (F7)

        if (e.state() == EnemyState::Spawning) { e.update(*ctx.map, Direction::None, dt); continue; }

        const int ex = TileMap::toTile(e.position().x);
        const int ey = TileMap::toTile(e.position().y);
        const int tileId = ey * ctx.map->width() + ex;

        if (bumpBanTimer_[i] > 0.f) {
            bumpBanTimer_[i] -= dt;
            if (bumpBanTimer_[i] <= 0.f) bumpBan_[i] = Direction::None;
        }

        if (e.blocked()) {
            // Nosed into a rock or a wall: back out, and remember not to try
            // that way again for a moment.
            bumpBan_[i]      = e.direction();
            bumpBanTimer_[i] = tuning_.bumpMemorySeconds;
            pendingTurn_[i]  = escapeFrom(e, ctx);
            lastDecisionTile_[i] = tileId;
        } else if (tileId != lastDecisionTile_[i]) {
            // Otherwise decide once per tile entered.
            lastDecisionTile_[i] = tileId;
            pendingTurn_[i] = decide(e, ctx, bumpBan_[i]);

            const int px = TileMap::toTile(ctx.playerPos.x);
            const int py = TileMap::toTile(ctx.playerPos.y);
            const int distance = std::abs(ex - px) + std::abs(ey - py);
            e.setState(distance <= tuning_.pathfindRangeTiles ? EnemyState::Chasing
                                                              : EnemyState::Moving);
            e.setTarget(ctx.playerPos);
        }

        // Hold the requested turn until the car is aligned enough to take it.
        e.update(*ctx.map, pendingTurn_[i], dt);
        if (e.direction() == pendingTurn_[i]) pendingTurn_[i] = Direction::None;
    }
}

} // namespace rx
