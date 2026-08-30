#pragma once
#include "core/Types.h"
#include "world/TileMap.h"

namespace rx {

// All static-world collision and grid movement lives here.  Entities never
// test tiles themselves, and rendering never participates in collision.
namespace CollisionSystem {

// Result of one movement step.
struct MoveResult {
    bool moved   = false;   // the entity actually advanced this step
    bool blocked = false;   // it is pressed against a wall in its facing dir
    bool turned  = false;   // the facing direction changed this step
};

// True when a car centred on this tile would fit (i.e. the tile is road).
bool canOccupy(const TileMap& map, int tx, int ty);

// True when a car currently at `pos` may start moving in `dir`.
bool canTurn(const TileMap& map, const Vec2& pos, Direction dir, float tolerance);

// Advance a grid-aligned car.  `desired` is the direction the controller
// wants; the car keeps its current heading until a turn becomes legal, which
// is what gives Rally-X its "commit to the corridor" feel.
MoveResult step(const TileMap& map, Vec2& pos, Direction& dir,
                Direction desired, float speed, float tolerance = 5.0f);

// Centred collision box, deliberately smaller than the 16x16 sprite so that
// near-misses in a one-tile corridor stay near-misses.
inline Rect boundsAt(const Vec2& c, float size) {
    return Rect{ c.x - size * 0.5f, c.y - size * 0.5f, size, size };
}

// Cheap centre-distance test used for car/car, car/rock and car/flag hits.
bool circlesOverlap(const Vec2& a, const Vec2& b, float radius);

} // namespace CollisionSystem
} // namespace rx
