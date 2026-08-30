#pragma once
#include "core/Types.h"
#include "rendering/Palette.h"

namespace rx {

// Each round redresses the playfield: the maze blocks and the pursuit cars
// change colour so progress is visible at a glance.  The HUD, the player car
// and the flags keep their colours -- those are how the player reads the game,
// so they stay fixed.
//
// Every entry is a flat, hard-edged arcade colour.  No gradients, no tints.
struct RoundTheme {
    int   index = 0;
    Color wall, wallLight, wallDark;
    Color enemyBody, enemyTrim;
    const char* name = "";
};

constexpr int ROUND_THEME_COUNT = 8;

RoundTheme themeFor(int roundNumber);
const RoundTheme& themeByIndex(int index);

} // namespace rx
