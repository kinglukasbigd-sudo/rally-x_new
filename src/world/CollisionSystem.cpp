#include "world/CollisionSystem.h"
#include <cmath>
#include <algorithm>

namespace rx {
namespace CollisionSystem {

bool canOccupy(const TileMap& map, int tx, int ty) {
    return map.isRoad(tx, ty);
}

static float centerOfTile(int t) { return t * TILE + TILE * 0.5f; }

bool canTurn(const TileMap& map, const Vec2& pos, Direction dir, float tolerance) {
    if (dir == Direction::None) return false;
    const int tx = TileMap::toTile(pos.x);
    const int ty = TileMap::toTile(pos.y);

    // Turning across the corridor is only legal near the tile centre.
    const bool horizontal = (dir == Direction::Left || dir == Direction::Right);
    const float perp    = horizontal ? pos.y : pos.x;
    const float perpMid = horizontal ? centerOfTile(ty) : centerOfTile(tx);
    if (std::fabs(perp - perpMid) > tolerance) return false;

    return canOccupy(map, tx + dirDX(dir), ty + dirDY(dir));
}

MoveResult step(const TileMap& map, Vec2& pos, Direction& dir,
                Direction desired, float speed, float tolerance) {
    MoveResult r;

    // A reversal is always legal: the car is already centred on that axis.
    if (desired != Direction::None && desired != dir) {
        if (desired == opposite(dir)) {
            dir = desired;
            r.turned = true;
        } else if (canTurn(map, pos, desired, tolerance)) {
            // Snap onto the corridor centre line so the car stays grid-true.
            const int tx = TileMap::toTile(pos.x);
            const int ty = TileMap::toTile(pos.y);
            if (desired == Direction::Left || desired == Direction::Right)
                pos.y = centerOfTile(ty);
            else
                pos.x = centerOfTile(tx);
            dir = desired;
            r.turned = true;
        }
    }

    if (dir == Direction::None) return r;

    const float dx = static_cast<float>(dirDX(dir));
    const float dy = static_cast<float>(dirDY(dir));

    const float nx = pos.x + dx * speed;
    const float ny = pos.y + dy * speed;

    // Clamp at the centre of the last open tile when a wall is ahead.  This
    // parks a 16x16 car exactly against the wall face.
    const int tx = TileMap::toTile(pos.x);
    const int ty = TileMap::toTile(pos.y);
    const bool wallAhead = !canOccupy(map, tx + dirDX(dir), ty + dirDY(dir));

    float fx = nx, fy = ny;
    if (wallAhead) {
        const float limitX = centerOfTile(tx);
        const float limitY = centerOfTile(ty);
        if (dx > 0) fx = std::min(fx, limitX);
        if (dx < 0) fx = std::max(fx, limitX);
        if (dy > 0) fy = std::min(fy, limitY);
        if (dy < 0) fy = std::max(fy, limitY);
    }

    r.moved   = (fx != pos.x) || (fy != pos.y);
    r.blocked = wallAhead && !r.moved;
    pos.x = fx;
    pos.y = fy;
    return r;
}

bool circlesOverlap(const Vec2& a, const Vec2& b, float radius) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy <= radius * radius;
}

} // namespace CollisionSystem
} // namespace rx
