#pragma once
#include "core/Types.h"

namespace rx {

// A hazard.  Rocks are deliberately NOT maze walls: walls stop the car, rocks
// destroy it, so they are stored and tested separately.
//
// From round 10 onwards a rock also patrols: it slides left and right along a
// short stretch of its own corridor and turns round at the ends.  The stretch
// is worked out once, when the round loads, and never changes -- a rock can
// only ever be somewhere the player could already have been killed, so a
// moving rock adds pressure without adding surprises.
struct Rock {
    Vec2  pos;

    // The patrol, in world pixels.  A stationary rock has minX == maxX and a
    // speed of zero, which is every rock before round 10.
    float minX  = 0.f;
    float maxX  = 0.f;
    float speed = 0.f;    // world pixels per fixed step
    int   dirX  = 1;      // +1 right, -1 left

    bool moving() const { return speed > 0.f && maxX > minX; }

    // One fixed step of the patrol.  Movement is per-step, exactly like the
    // cars', so the rocks stay in lockstep with everything else in the round.
    void step() {
        if (!moving()) return;
        pos.x += speed * static_cast<float>(dirX);
        if (pos.x <= minX) { pos.x = minX; dirX =  1; }
        if (pos.x >= maxX) { pos.x = maxX; dirX = -1; }
    }

    Rect bounds() const { return Rect{ pos.x - 5.f, pos.y - 5.f, 10.f, 10.f }; }
};

} // namespace rx
