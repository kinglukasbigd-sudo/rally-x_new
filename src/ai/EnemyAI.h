#pragma once
#include "core/Types.h"
#include <cstdint>
#include <vector>

namespace rx {

class TileMap;
class NavigationGraph;
class Enemy;
class SmokeSystem;

// Drives the enemy cars.  The behaviour is intentionally shallow: at long
// range a car simply takes whichever legal turn shortens the gap, and only
// close in does it route properly through the maze.  It never reverses on a
// whim, it never predicts, and it never coordinates with the other cars.
class EnemyAI {
public:
    // Tunables kept together so difficulty is one readable table, not magic
    // numbers spread through the update.
    struct Tuning {
        int   pathfindRangeTiles = 12;   // inside this, route with A*
        float randomTurnChance   = 0.16f;// keeps the pack from single-filing
        float stunSeconds        = 2.6f;
        // After bouncing off a rock a car refuses that direction for a moment,
        // so it backs out and goes around instead of grinding against it.
        float bumpMemorySeconds  = 1.6f;
    };

    struct Context {
        // Where a car may physically go: the maze with the rocks filled in.
        const TileMap*         map   = nullptr;
        // What a car believes the maze looks like: rocks are invisible to it,
        // which is why it drives into one and has to back out.
        const TileMap*         planMap = nullptr;
        const NavigationGraph* nav   = nullptr;
        const SmokeSystem*     smoke = nullptr;
        Vec2                   playerPos;
        bool                   playerAlive = true;
        bool                   frozen      = false;   // debug: F7
    };

    void reset(uint32_t seed, const Tuning& t);

    // One simulation step for every car.
    void update(std::vector<Enemy>& enemies, const Context& ctx, float dt);

    const Tuning& tuning() const { return tuning_; }
    void setTuning(const Tuning& t) { tuning_ = t; }

private:
    Direction decide(const Enemy& e, const Context& ctx, Direction banned);
    Direction escapeFrom(const Enemy& e, const Context& ctx);
    uint32_t  nextRandom();
    float     randomUnit();

    Tuning   tuning_;
    uint32_t rng_ = 1u;

    // Per-car memory of the tile the last decision was made on, so a choice is
    // taken once per tile rather than every frame.
    std::vector<int> lastDecisionTile_;
    // The turn a car is waiting to take.  It is held until the car is aligned
    // with the corridor and can actually take it, which is what stops the cars
    // sailing straight past junctions.
    std::vector<Direction> pendingTurn_;
    // The direction each car has just bounced off, and how long it sulks.
    std::vector<Direction> bumpBan_;
    std::vector<float>     bumpBanTimer_;
};

} // namespace rx
