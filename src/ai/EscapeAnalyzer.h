#pragma once
#include "core/Types.h"
#include <vector>

namespace rx {

class TileMap;

// The fairness watchdog for the pursuit.
//
// The pursuit cars are allowed to be brutal, but they are not allowed to be
// unfair: the player must always have at least one escape they can actually
// take.  This class answers the only question that matters -- "if the player
// ran for it right now, is there anywhere they would get to before a red car
// does?" -- and EnemyAI uses the answer to veto a move that would close the
// last way out.
//
// It is deliberately a pure function of the world state: no memory, no
// ownership, nothing to reset.  That makes it cheap to run speculatively,
// which is exactly what the veto needs.
class EscapeAnalyzer {
public:
    // One pursuit car, reduced to the two things that decide a race.
    struct Pursuer {
        Vec2  pos;
        float speed  = 1.f;    // world pixels per fixed step
        bool  active = true;   // stunned/penned cars are not a threat
    };

    struct Result {
        // Tiles the player would reach before any car could.
        int  freeTiles = 0;
        // How many of the four directions open into a genuine escape.
        int  routes    = 0;
        bool trapped   = true;
        // Per-direction breakdown, indexed by Direction.
        int  dirTiles[4] = { 0, 0, 0, 0 };
        bool dirOpen[4]  = { false, false, false, false };
    };

    // A way out has to lead somewhere.  Fewer tiles than this and it is a
    // pocket the player merely dies in slightly later, not an escape.
    static constexpr int MIN_ROUTE_TILES = 6;

    // The head start, in fixed steps, that the player must have over the
    // nearest car for a tile to count as theirs.  Roughly a fifth of a second:
    // enough that a photo-finish does not count as safety.
    static constexpr float SAFETY_STEPS = 12.f;

    // `map` must be the map the player can actually survive on -- maze walls
    // with the rocks filled in, because a rock kills rather than blocks.
    // `ignore` skips one car, which is how the caller finds out which car is
    // responsible for a trap.
    static Result analyze(const TileMap& map,
                          const Vec2& playerPos, float playerSpeed,
                          const std::vector<Pursuer>& pursuers,
                          int ignore = -1);

    // The same question asked about a hypothetical: pursuer `index` is treated
    // as though it had already moved one tile in `step`.  This is what lets a
    // car test a turn before committing to it.
    static Result analyzeWithMove(const TileMap& map,
                                  const Vec2& playerPos, float playerSpeed,
                                  const std::vector<Pursuer>& pursuers,
                                  int index, Direction step);
};

} // namespace rx
