#pragma once
#include "core/Types.h"
#include "world/TileMap.h"

namespace rx {

// The player's car.  Movement is pure arcade: constant speed, four
// directions, instant response.  No acceleration, drift or steering model.
//
// House rule (a deliberate departure from the 1981 original, requested for
// this build): the car never comes to a stop against a wall.  When it runs
// out of road and the player is not steering it somewhere legal, it picks a
// random open direction and keeps driving.
class Player {
public:
    void spawn(const Vec2& worldPos, Direction facing, float speed);
    void update(const TileMap& map, Direction desired, float dt);

    const Vec2& position()  const { return pos_; }
    Direction   direction() const { return dir_; }
    bool        alive()     const { return alive_; }
    void        kill()            { alive_ = false; }
    bool        moving()    const { return moving_; }

    float speed() const { return speed_; }
    void  setSpeed(float s) { speed_ = s; }

    // The last turn the car took for itself rather than being steered into.
    bool  autoTurnedThisStep() const { return autoTurned_; }

    Rect  bounds() const;
    int   animationFrame() const { return animFrame_; }

private:
    // Picks an open direction at the current tile, preferring anything other
    // than a straight reversal so the car keeps exploring the maze.
    Direction chooseEscapeDirection(const TileMap& map);
    uint32_t  nextRandom();

private:
    Vec2      pos_;
    Vec2      spawn_;
    Direction dir_    = Direction::Right;
    float     speed_  = 1.35f;   // world pixels per 1/60 s step
    bool      alive_  = true;
    bool      moving_ = false;

    float animTimer_ = 0.f;
    int   animFrame_ = 0;

    bool     autoTurned_ = false;
    uint32_t rng_ = 0x2545F491u;
};

} // namespace rx
