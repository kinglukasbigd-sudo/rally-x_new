#pragma once
#include "core/Types.h"

namespace rx {

enum class FlagType : uint8_t { Normal = 0, Special = 1, Lucky = 2 };

struct Flag {
    Vec2     pos;                        // world pixels, centre of the tile
    FlagType type      = FlagType::Normal;
    bool     collected = false;

    Rect bounds() const { return Rect{ pos.x - 5.f, pos.y - 5.f, 10.f, 10.f }; }
};

} // namespace rx
