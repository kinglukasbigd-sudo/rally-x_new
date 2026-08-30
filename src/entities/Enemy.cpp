#include "entities/Enemy.h"
#include "world/CollisionSystem.h"
#include <algorithm>

namespace rx {

namespace { constexpr float COLLIDE_SIZE = 11.f; }

void Enemy::spawn(const Vec2& worldPos, Direction facing, float speed, float delay) {
    pos_        = worldPos;
    target_     = worldPos;
    dir_        = facing;
    speed_      = speed;
    stunTimer_  = 0.f;
    spawnTimer_ = delay;
    state_      = (delay > 0.f) ? EnemyState::Spawning : EnemyState::Moving;
    blocked_    = false;
    animTimer_  = 0.f;
    animFrame_  = 0;
}

void Enemy::update(const TileMap& map, Direction desired, float dt) {
    if (state_ == EnemyState::Inactive) return;

    if (state_ == EnemyState::Spawning) {
        spawnTimer_ -= dt;
        if (spawnTimer_ <= 0.f) { spawnTimer_ = 0.f; state_ = EnemyState::Moving; }
        return;
    }
    if (state_ == EnemyState::Stunned) return;   // stalled in the smoke

    const auto res = CollisionSystem::step(map, pos_, dir_, desired, speed_);
    blocked_ = res.blocked;
    if (res.moved) {
        animTimer_ += dt;
        if (animTimer_ >= 0.09f) { animTimer_ = 0.f; animFrame_ ^= 1; }
    }
}

void Enemy::stun(float seconds) {
    if (state_ == EnemyState::Inactive || state_ == EnemyState::Spawning) return;
    state_ = EnemyState::Stunned;
    stunTimer_ = std::max(stunTimer_, seconds);
}

void Enemy::tickStun(float dt) {
    if (state_ != EnemyState::Stunned) return;
    stunTimer_ -= dt;
    if (stunTimer_ <= 0.f) { stunTimer_ = 0.f; state_ = EnemyState::Moving; }
}

Rect Enemy::bounds() const {
    return CollisionSystem::boundsAt(pos_, COLLIDE_SIZE);
}

} // namespace rx
