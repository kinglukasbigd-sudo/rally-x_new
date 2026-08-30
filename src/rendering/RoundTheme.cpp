#include "rendering/RoundTheme.h"

namespace rx {

namespace {

const RoundTheme kThemes[ROUND_THEME_COUNT] = {
    // index, wall,              wallLight,          wallDark,
    //        enemyBody,         enemyTrim,          name
    { 0, { 32,  60, 232, 255}, { 88, 116, 255, 255}, { 16,  30, 130, 255},
         {228,  48,  32, 255}, {255, 200,  80, 255}, "BLUE" },

    { 1, { 24, 156,  72, 255}, { 80, 216, 128, 255}, { 12,  84,  40, 255},
         {236, 108,  16, 255}, {255, 224, 128, 255}, "GREEN" },

    { 2, {120,  44, 200, 255}, {180, 112, 255, 255}, { 62,  20, 108, 255},
         { 32, 216, 216, 255}, {224, 255, 255, 255}, "VIOLET" },

    { 3, { 16, 140, 156, 255}, { 72, 208, 224, 255}, {  8,  74,  84, 255},
         {236,  60, 168, 255}, {255, 196, 232, 255}, "TEAL" },

    { 4, {168,  36,  44, 255}, {236, 100, 108, 255}, { 92,  16,  20, 255},
         {236, 216,  32, 255}, {255, 255, 176, 255}, "CRIMSON" },

    { 5, { 60,  60, 172, 255}, {124, 124, 240, 255}, { 28,  28,  92, 255},
         {140, 236,  40, 255}, {224, 255, 176, 255}, "INDIGO" },

    { 6, {196, 112,  16, 255}, {252, 180,  72, 255}, {108,  60,   8, 255},
         { 96, 140, 255, 255}, {200, 224, 255, 255}, "AMBER" },

    { 7, { 96, 100, 112, 255}, {168, 172, 188, 255}, { 48,  50,  58, 255},
         {248,  72,  72, 255}, {255, 255, 255, 255}, "STEEL" },
};

} // namespace

const RoundTheme& themeByIndex(int index) {
    if (index < 0) index = 0;
    return kThemes[index % ROUND_THEME_COUNT];
}

RoundTheme themeFor(int roundNumber) {
    if (roundNumber < 1) roundNumber = 1;
    return kThemes[(roundNumber - 1) % ROUND_THEME_COUNT];
}

} // namespace rx
