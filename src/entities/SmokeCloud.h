#pragma once
#include "core/Types.h"

namespace rx {

// A puff of the smoke screen.  This is a gameplay object, not decoration: it
// occupies world space, expires on its own clock and is what actually stalls
// an enemy car that drives into it.
struct SmokeCloud {
    Vec2  pos;
    float lifetime    = 0.f;   // seconds remaining
    float maxLifetime = 1.6f;
    float radius      = 9.f;
    bool  active      = false;

    float lifeFraction() const {
        return (maxLifetime > 0.f) ? lifetime / maxLifetime : 0.f;
    }
};

} // namespace rx
