#include "entities/Player.h"
#include "world/CollisionSystem.h"

namespace rx {

namespace { constexpr float COLLIDE_SIZE = 11.f; }

void Player::spawn(const Vec2& worldPos, Direction facing, float speed) {
    pos_   = worldPos;
    spawn_ = worldPos;
    dir_   = facing;
    speed_ = speed;
    alive_ = true;
    moving_ = false;
    autoTurned_ = false;
    animTimer_ = 0.f;
    animFrame_ = 0;
    // Seeded from the spawn tile so a restarted round is reproducible.
    rng_ = 0x2545F491u ^ (static_cast<uint32_t>(worldPos.x) * 73856093u)
                       ^ (static_cast<uint32_t>(worldPos.y) * 19349663u);
    if (rng_ == 0) rng_ = 1u;
}

uint32_t Player::nextRandom() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return rng_;
}

Direction Player::chooseEscapeDirection(const TileMap& map) {
    const int tx = TileMap::toTile(pos_.x);
    const int ty = TileMap::toTile(pos_.y);

    Direction options[4];
    int count = 0;
    const Direction back = opposite(dir_);

    for (Direction d : { Direction::Up, Direction::Down, Direction::Left, Direction::Right }) {
        if (d == dir_ || d == back) continue;          // blocked, or a U-turn
        if (map.isRoad(tx + dirDX(d), ty + dirDY(d))) options[count++] = d;
    }
    if (count > 0) return options[nextRandom() % count];

    // Nothing but the way it came: a dead end, so turn around.
    if (map.isRoad(tx + dirDX(back), ty + dirDY(back))) return back;
    return Direction::None;
}

void Player::update(const TileMap& map, Direction desired, float dt) {
    if (!alive_) return;

    // In Rally-X the car is always rolling; releasing the stick simply keeps
    // it going the way it was already pointed.
    auto res = CollisionSystem::step(map, pos_, dir_, desired, speed_);

    // House rule: a wall redirects the car instead of parking it.  Steering
    // still wins -- this only fires when the player has not asked for a legal
    // turn, because in that case the step above would not report `blocked`.
    autoTurned_ = false;
    if (res.blocked) {
        const Direction escape = chooseEscapeDirection(map);
        if (escape != Direction::None) {
            res = CollisionSystem::step(map, pos_, dir_, escape, speed_);
            autoTurned_ = true;
        }
    }

    moving_ = res.moved;

    if (moving_) {
        animTimer_ += dt;
        if (animTimer_ >= 0.08f) { animTimer_ = 0.f; animFrame_ ^= 1; }
    }
}

Rect Player::bounds() const {
    return CollisionSystem::boundsAt(pos_, COLLIDE_SIZE);
}

} // namespace rx
