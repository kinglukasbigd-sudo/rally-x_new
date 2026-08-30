#pragma once
#include "core/Types.h"

namespace rx {

class TileMap;

enum class EnemyState : uint8_t {
    Spawning,   // waiting on the grid before it joins the chase
    Moving,     // patrolling towards the player at long range
    Chasing,    // actively routing through the maze after the player
    Stunned,    // sitting in a smoke cloud
    Inactive    // not in play at all (challenging stages)
};

inline const char* enemyStateName(EnemyState s) {
    switch (s) {
        case EnemyState::Spawning: return "SPAWNING";
        case EnemyState::Moving:   return "MOVING";
        case EnemyState::Chasing:  return "CHASING";
        case EnemyState::Stunned:  return "STUNNED";
        case EnemyState::Inactive: return "INACTIVE";
    }
    return "?";
}

// A pursuing car.  It carries only its own state; every decision about where
// to go is made by EnemyAI, so the movement code stays identical to the
// player's and the behaviour can be retuned in one place.
class Enemy {
public:
    void spawn(const Vec2& worldPos, Direction facing, float speed, float delay);
    void setInactive() { state_ = EnemyState::Inactive; }

    // Motion only: `desired` comes from EnemyAI.
    void update(const TileMap& map, Direction desired, float dt);

    void stun(float seconds);
    void tickStun(float dt);

    const Vec2& position()  const { return pos_; }
    Direction   direction() const { return dir_; }
    EnemyState  state()     const { return state_; }
    void        setState(EnemyState s) { state_ = s; }

    bool  stunned()  const { return state_ == EnemyState::Stunned; }
    bool  onTrack()  const { return state_ != EnemyState::Inactive; }
    bool  dangerous()const { return state_ == EnemyState::Moving || state_ == EnemyState::Chasing; }
    float stunTimer()const { return stunTimer_; }
    float spawnTimer()const{ return spawnTimer_; }

    float speed() const { return speed_; }
    void  setSpeed(float s) { speed_ = s; }

    const Vec2& target() const { return target_; }
    void  setTarget(const Vec2& t) { target_ = t; }

    // True when the car is nosed into a wall and needs a fresh decision.
    bool blocked() const { return blocked_; }

    Rect bounds() const;
    int  animationFrame() const { return animFrame_; }

private:
    Vec2       pos_;
    Vec2       target_;
    Direction  dir_        = Direction::Left;
    EnemyState state_      = EnemyState::Spawning;
    float      speed_      = 1.05f;
    float      stunTimer_  = 0.f;
    float      spawnTimer_ = 0.f;
    bool       blocked_    = false;

    float animTimer_ = 0.f;
    int   animFrame_ = 0;
};

} // namespace rx
