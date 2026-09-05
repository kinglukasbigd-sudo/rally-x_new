#pragma once
#include "core/Types.h"

namespace rx {

// A turbo pickup.  Same shape of thing as a Flag -- it sits on a tile until
// the player drives over it -- but it scores nothing; it buys speed.
struct Turbo {
    Vec2 pos;                    // world pixels, centre of the tile
    bool collected = false;

    Rect bounds() const { return Rect{ pos.x - 5.f, pos.y - 5.f, 10.f, 10.f }; }
};

} // namespace rx
