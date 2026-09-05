#pragma once
#include "core/Types.h"
#include "ai/EscapeAnalyzer.h"
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

        // --- fairness ------------------------------------------------------
        // The cars may corner the player; they may not seal the player in.
        // With this on, a move that would close the last escape route is
        // refused, and if the player is already boxed in the car most
        // responsible peels off for a moment.
        bool  antiTrap           = true;
        float yieldSeconds       = 1.4f;   // how long the guilty car backs off
        // Only cars this close are worth vetting -- a car eight tiles away is
        // not the one closing the box.
        int   fairnessRangeTiles = 10;
        // How often the escape picture is recomputed when the player has not
        // changed tile.  It is also recomputed immediately when they do.
        float fairnessInterval   = 0.10f;
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
        float                  playerSpeed = 1.35f;
        bool                   playerAlive = true;
        bool                   frozen      = false;   // debug: F7
    };

    void reset(uint32_t seed, const Tuning& t);

    // One simulation step for every car.
    void update(std::vector<Enemy>& enemies, const Context& ctx, float dt);

    const Tuning& tuning() const { return tuning_; }
    void setTuning(const Tuning& t) { tuning_ = t; }

    // The most recent fairness picture.  Exposed so the debug overlay and the
    // tests can see what the AI saw rather than having to infer it.
    const EscapeAnalyzer::Result& escape() const { return escape_; }
    // Cars currently backing off to keep a way out open.
    int  yieldingCars() const;
    bool isYielding(size_t index) const;

private:
    Direction decide(const Enemy& e, const Context& ctx, Direction banned);
    Direction escapeFrom(const Enemy& e, const Context& ctx);
    uint32_t  nextRandom();
    float     randomUnit();

    // --- fairness ----------------------------------------------------------
    // Rebuilds the escape picture and hands out a yield if the player is boxed.
    void      enforceFairness(const std::vector<Enemy>& enemies,
                              const Context& ctx, float dt);
    // Snapshot of the pack in the form EscapeAnalyzer wants.
    void      collectPursuers(const std::vector<Enemy>& enemies);
    // Vets `want`, and returns it or a kinder alternative.
    Direction keepEscapeOpen(size_t index, const Enemy& e, const Context& ctx,
                             Direction want, Direction banned);

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

    // How much longer each car stays off the player's back.
    std::vector<float>     yieldTimer_;
    // The pack as the analyser sees it, kept between frames so the veto can
    // re-run the analysis without rebuilding the list every time.
    std::vector<EscapeAnalyzer::Pursuer> pursuers_;
    EscapeAnalyzer::Result escape_;
    int   lastPlayerTile_ = -1;
    float fairnessTimer_  = 0.f;
};

} // namespace rx
