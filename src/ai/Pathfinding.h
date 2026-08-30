#pragma once
#include "core/Types.h"

namespace rx {

class TileMap;

// Deliberately minimal: the enemy cars only ever need to know which way to
// leave the tile they are standing on, so the search returns the first step of
// the route and nothing else.  There is no path smoothing, no re-planning
// budget and no steering behaviour -- this is arcade pursuit, not modern AI.
namespace Pathfinding {

// A* over road tiles.  `banned` (usually a straight reversal) is refused as
// the first move.  Returns Direction::None when no route exists.
Direction firstStep(const TileMap& map,
                    int fromX, int fromY, int toX, int toY,
                    Direction banned);

// Greedy fallback: the legal move that most reduces Manhattan distance.
// This is what the cars use at long range, and it is what makes them
// occasionally blunder into a wall exactly like the originals do.
Direction greedyStep(const TileMap& map,
                     int fromX, int fromY, int toX, int toY,
                     Direction banned);

} // namespace Pathfinding
} // namespace rx
