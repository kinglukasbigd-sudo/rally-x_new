#pragma once
#include "core/Types.h"

namespace rx {

// A deliberately small, hard-edged arcade palette.  No gradients anywhere in
// the game: every colour used on screen comes from this list.
namespace pal {
constexpr Color Black      {  0,   0,   0, 255};
constexpr Color Wall       { 32,  60, 232, 255};
constexpr Color WallLight  { 88, 116, 255, 255};
constexpr Color WallDark   { 16,  30, 130, 255};
constexpr Color Road       {  0,   0,   0, 255};
constexpr Color Grid       { 20,  20,  36, 255};

constexpr Color PlayerBody { 60, 150, 255, 255};
constexpr Color PlayerTrim {255, 255, 255, 255};
constexpr Color EnemyBody  {228,  48,  32, 255};
constexpr Color EnemyTrim  {255, 200,  80, 255};
constexpr Color Tyre       { 24,  24,  24, 255};
constexpr Color Glass      {160, 220, 255, 255};

constexpr Color FlagPole   {255, 255, 255, 255};
constexpr Color FlagNormal {248, 188,   0, 255};
constexpr Color FlagSpecial{ 64, 232,  96, 255};
constexpr Color FlagLucky  {248, 108, 200, 255};

constexpr Color Rock       {172, 132,  72, 255};
constexpr Color RockDark   {104,  76,  36, 255};

constexpr Color Turbo      { 64, 232, 232, 255};
constexpr Color TurboDark  { 16, 120, 140, 255};

constexpr Color Smoke      {224, 224, 224, 255};
constexpr Color SmokeDark  {148, 148, 148, 255};

constexpr Color Text       {255, 255, 255, 255};
constexpr Color TextDim    {160, 160, 160, 255};
constexpr Color Accent     {248, 188,   0, 255};
constexpr Color Danger     {228,  48,  32, 255};

constexpr Color FuelHigh   { 40, 224,  80, 255};
constexpr Color FuelLow    {228,  48,  32, 255};
constexpr Color PanelLine  { 96,  96, 112, 255};
constexpr Color RadarBack  { 12,  12,  24, 255};
} // namespace pal

} // namespace rx
