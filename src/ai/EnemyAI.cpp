#include "ai/EnemyAI.h"
#include "ai/EscapeAnalyzer.h"
#include "ai/NavigationGraph.h"
#include "ai/Pathfinding.h"
#include "entities/Enemy.h"
#include "gameplay/SmokeSystem.h"
#include "world/TileMap.h"
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace rx {

void EnemyAI::reset(uint32_t seed, const Tuning& t) {
    tuning_ = t;
    rng_ = seed ? seed : 1u;
    lastDecisionTile_.clear();
    pendingTurn_.clear();
    bumpBan_.clear();
    bumpBanTimer_.clear();
    yieldTimer_.clear();
    pursuers_.clear();
    escape_ = EscapeAnalyzer::Result{};
    lastPlayerTile_ = -1;
    fairnessTimer_  = 0.f;
}

int EnemyAI::yieldingCars() const {
    int n = 0;
    for (float t : yieldTimer_) if (t > 0.f) ++n;
    return n;
}

bool EnemyAI::isYielding(size_t index) const {
    return index < yieldTimer_.size() && yieldTimer_[index] > 0.f;
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


// ---------------------------------------------------------------------------
// Fairness
//
// The rule the pack plays by: the player may be cornered, but never sealed in.
// Everything below exists to enforce that one sentence.
// ---------------------------------------------------------------------------

void EnemyAI::collectPursuers(const std::vector<Enemy>& enemies) {
    pursuers_.resize(enemies.size());
    for (size_t i = 0; i < enemies.size(); ++i) {
        pursuers_[i].pos    = enemies[i].position();
        pursuers_[i].speed  = enemies[i].speed();
        // A stunned or penned car is not closing anything off, so it does not
        // count towards a trap -- and must not be blamed for one either.
        pursuers_[i].active = enemies[i].dangerous();
    }
}

void EnemyAI::enforceFairness(const std::vector<Enemy>& enemies,
                              const Context& ctx, float dt) {
    for (auto& t : yieldTimer_) if (t > 0.f) t = std::max(0.f, t - dt);

    if (!tuning_.antiTrap || !ctx.playerAlive) return;

    // Recompute when the player crosses a tile, and otherwise on a slow tick:
    // the picture only changes meaningfully when somebody moves a whole tile.
    const int ptile = TileMap::toTile(ctx.playerPos.y) * ctx.map->width()
                    + TileMap::toTile(ctx.playerPos.x);
    fairnessTimer_ -= dt;
    if (ptile == lastPlayerTile_ && fairnessTimer_ > 0.f) return;
    lastPlayerTile_ = ptile;
    fairnessTimer_  = tuning_.fairnessInterval;

    collectPursuers(enemies);
    escape_ = EscapeAnalyzer::analyze(*ctx.map, ctx.playerPos, ctx.playerSpeed, pursuers_);

    // A yield only means anything if the car acts on it now, so whoever is
    // told to stand down re-decides on the very next frame instead of waiting
    // to reach a junction.
    auto standDown = [&](size_t i) {
        yieldTimer_[i] = tuning_.yieldSeconds;
        if (i < lastDecisionTile_.size()) lastDecisionTile_[i] = -1;
    };

    if (!escape_.trapped) {
        // Down to the last way out.  Catching a trap as it is forming is much
        // better than breaking one that already has: a car whose next tile
        // would close the last route is pulled off before it gets there, even
        // though its own turn is not due for several tiles yet.
        if (escape_.routes <= 1) {
            for (size_t i = 0; i < enemies.size(); ++i) {
                if (!pursuers_[i].active || yieldTimer_[i] > 0.f) continue;
                if (EscapeAnalyzer::analyzeWithMove(*ctx.map, ctx.playerPos, ctx.playerSpeed,
                                                    pursuers_, static_cast<int>(i),
                                                    enemies[i].direction()).trapped) {
                    standDown(i);
                    break;
                }
            }
        }
        return;
    }

    // Boxed in.  Find the car whose absence would restore a way out -- that is
    // the one holding the lid down -- and send it away for a moment.  Nearest
    // first, because the nearest car is both the likeliest culprit and the one
    // whose departure the player can actually use.
    std::vector<size_t> order;
    for (size_t i = 0; i < pursuers_.size(); ++i)
        if (pursuers_[i].active) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto d = [&](size_t i) {
            return std::fabs(pursuers_[i].pos.x - ctx.playerPos.x)
                 + std::fabs(pursuers_[i].pos.y - ctx.playerPos.y);
        };
        return d(a) < d(b);
    });

    for (size_t i : order) {
        if (yieldTimer_[i] > 0.f) continue;    // already standing down
        const auto without = EscapeAnalyzer::analyze(*ctx.map, ctx.playerPos,
                                                     ctx.playerSpeed, pursuers_,
                                                     static_cast<int>(i));
        if (!without.trapped) {
            standDown(i);
            return;
        }
    }

    // Nobody's departure alone would help.  Before blaming the pack, ask
    // whether they are the cause at all: take every car off the map and see if
    // the player has anywhere to go.  If they still do not, the box is the
    // maze's -- a pocket, a stub, a corner the player drove into -- and the
    // cars are guilty of nothing.  Standing them down here is what would turn
    // a fairness rule into an order to keep away, so they are left to chase.
    std::vector<EscapeAnalyzer::Pursuer> none;
    if (EscapeAnalyzer::analyze(*ctx.map, ctx.playerPos, ctx.playerSpeed, none).trapped)
        return;

    // The pack really is the problem, and it takes more than one of them to
    // fix it: the two nearest both peel off.  Rare, and the last resort.
    int stood = 0;
    for (size_t i : order) {
        if (yieldTimer_[i] > 0.f) continue;
        standDown(i);
        if (++stood == 2) break;
    }
}

Direction EnemyAI::keepEscapeOpen(size_t index, const Enemy& e, const Context& ctx,
                                  Direction want, Direction banned) {
    if (!tuning_.antiTrap || index >= pursuers_.size()) return want;

    const int ex = TileMap::toTile(e.position().x);
    const int ey = TileMap::toTile(e.position().y);
    const int px = TileMap::toTile(ctx.playerPos.x);
    const int py = TileMap::toTile(ctx.playerPos.y);
    const bool yielding = yieldTimer_[index] > 0.f;

    // Distant cars cannot be closing the box, so they are not vetted at all.
    // This is what keeps the pack dangerous: the check is surgical, not a
    // blanket order to stay away.
    if (!yielding && std::abs(ex - px) + std::abs(ey - py) > tuning_.fairnessRangeTiles)
        return want;
    // Nor is there anything to protect while the player still has options.
    if (!yielding && escape_.routes > 1) return want;

    pursuers_[index].pos = e.position();       // the snapshot may be a tick old

    auto score = [&](Direction d) {
        const auto r = EscapeAnalyzer::analyzeWithMove(*ctx.map, ctx.playerPos,
                                                       ctx.playerSpeed, pursuers_,
                                                       static_cast<int>(index), d);
        // Routes first, then room: a wide single corridor beats two crevices.
        int s = r.routes * 1000000 + r.freeTiles * 1000;
        // A car that has been told to stand down must actually go somewhere.
        // Where the player is boxed into a pocket the maze itself made, no move
        // improves the count, every option ties, and without this the car would
        // simply keep its heading -- yielding on paper and pressing in fact.
        // Backing off is then the tie-break, so the pack visibly opens up.
        if (yielding)
            s += std::abs(ex + dirDX(d) - px) + std::abs(ey + dirDY(d) - py);
        return s;
    };

    const int wantScore = (want == Direction::None) ? -1 : score(want);
    // A move that leaves the player a way out is always allowed through, so
    // the cars keep chasing exactly as before in the overwhelming majority of
    // frames.  Only a lid-closing move is rejected.
    if (!yielding && wantScore >= 1000000) return want;

    Direction best = want;
    int bestScore = wantScore;
    for (Direction d : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
        if (d == want || d == banned) continue;
        if (!ctx.map->isRoad(ex + dirDX(d), ey + dirDY(d))) continue;
        const int s = score(d);
        if (s > bestScore) { bestScore = s; best = d; }
    }
    return best;
}

void EnemyAI::update(std::vector<Enemy>& enemies, const Context& ctx, float dt) {
    if (!ctx.map || !ctx.nav) return;
    if (lastDecisionTile_.size() != enemies.size()) {
        lastDecisionTile_.assign(enemies.size(), -1);
        pendingTurn_.assign(enemies.size(), Direction::None);
        bumpBan_.assign(enemies.size(), Direction::None);
        bumpBanTimer_.assign(enemies.size(), 0.f);
        yieldTimer_.assign(enemies.size(), 0.f);
        lastPlayerTile_ = -1;
    }

    // Decide who, if anyone, has to give the player room this frame.
    enforceFairness(enemies, ctx, dt);

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
            pendingTurn_[i] = keepEscapeOpen(i, e, ctx, decide(e, ctx, bumpBan_[i]),
                                             bumpBan_[i]);

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
