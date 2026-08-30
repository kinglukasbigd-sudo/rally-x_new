#pragma once
#include "core/Types.h"

namespace rx {

// A static hazard.  Rocks are deliberately NOT maze walls: walls stop the car,
// rocks destroy it, so they are stored and tested separately.
struct Rock {
    Vec2 pos;
    Rect bounds() const { return Rect{ pos.x - 5.f, pos.y - 5.f, 10.f, 10.f }; }
};

} // namespace rx
