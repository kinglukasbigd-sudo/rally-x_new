#pragma once
#include "core/Types.h"
#include "world/Maze.h"
#include "world/Camera.h"
#include "entities/Player.h"
#include "entities/Flag.h"
#include "entities/Rock.h"
#include "entities/Enemy.h"
#include "gameplay/FuelSystem.h"
#include "gameplay/SmokeSystem.h"
#include "ai/NavigationGraph.h"
#include "ai/EnemyAI.h"
#include "gameplay/FlagPlacer.h"
#include <vector>

namespace rx {

class InputManager;
class ScoreSystem;

enum class DeathCause { None, Rock, OutOfFuel, Enemy };

// One playable round: the maze, everything living in it, and the rules that
// end it.  A Round is rebuilt from its LevelData on every restart, so a lost
// life always restores exactly the original layout.
class Round {
public:
    struct Events {
        bool       playerDied    = false;
        bool       roundComplete = false;
        DeathCause cause         = DeathCause::None;
        int        bonusAwarded  = 0;   // points from a Lucky Flag this step
        int        flagsTaken    = 0;   // normal flags collected this step
        int        flagSequence  = 0;   // 1-based index of the last one taken
        bool       specialTaken  = false;
        bool       luckyTaken    = false;
        bool       smokePuffed   = false;
    };

    // `flagSeed` of 0 keeps the flag positions authored in the level file;
    // any other value reshuffles them.  Each new round gets a fresh seed, so
    // the same maze never plays the same way twice.
    void load(const LevelData& level, uint32_t flagSeed = 0);

    // A fresh round: the maze, everything back to the start, flags re-placed.
    void restart();
    void restart(uint32_t flagSeed);

    // A fresh car in the middle of a round.  The flags already collected stay
    // collected -- losing a life costs the life, not the round's progress.
    void restartAfterDeath();

    Events update(const InputManager& input, ScoreSystem& score, float dt);

    const LevelData& level()  const { return level_; }
    const TileMap&   map()    const { return level_.map; }

    // The same maze with the rocks filled in as solid.  Pursuit cars drive and
    // navigate on this, so they bump up against a rock and turn away instead
    // of sailing straight through it.  The player still uses map() -- a rock
    // wrecks the player's car rather than blocking it.
    const TileMap&   enemyMap() const { return enemyMap_; }
    const Camera&    camera() const { return camera_; }
    const Player&    player() const { return player_; }

    const std::vector<Flag>& flags() const { return flags_; }
    const std::vector<Rock>&  rocks()   const { return rocks_; }
    const std::vector<Enemy>& enemies() const { return enemies_; }
    const NavigationGraph&    nav()     const { return nav_; }

    // Seconds until the pack leaves the pen; 0 once they are loose.
    float launchCountdown() const;
    const SmokeSystem&       smoke() const { return smoke_; }
    const FuelSystem&        fuel()  const { return fuel_;  }

    int flagsRequired()  const { return required_; }
    int flagsCollected() const { return collected_; }
    int flagsRemaining() const { return required_ - collected_; }

    bool isChallenge() const { return level_.type == RoundType::Challenge; }

    void setInfiniteFuel(bool on) { fuel_.setInfinite(on); }
    void setEnemiesFrozen(bool on) { enemiesFrozen_ = on; }
    void debugCollectAllFlags(ScoreSystem& score);

private:
    void collectFlags(ScoreSystem& score, Events& ev);
    void checkRocks(Events& ev);
    void checkEnemies(Events& ev);
    void spawnEnemies();
    void buildEnemyMap();
    void resetActors();
    void placeFlags();
    Vec2 smokeEmitPoint() const;

    LevelData         level_;
    Player            player_;
    std::vector<Flag> flags_;
    std::vector<Rock>  rocks_;
    std::vector<Enemy> enemies_;
    TileMap            enemyMap_;
    NavigationGraph    nav_;
    EnemyAI            ai_;
    SmokeSystem       smoke_;
    FuelSystem        fuel_;
    Camera            camera_;

    bool     enemiesFrozen_ = false;
    uint32_t flagSeed_      = 0;

    int required_  = FLAGS_PER_ROUND;
    int collected_ = 0;
};

} // namespace rx
