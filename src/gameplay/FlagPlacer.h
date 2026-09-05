#pragma once
#include "core/Types.h"
#include "world/Maze.h"
#include <cstdint>
#include <vector>

namespace rx {

class TileMap;

// Scatters a round's flags across the maze.
//
// The authored positions in a .lvl file are a starting point, not a fixed
// layout: every new round re-places the flags so the same maze never plays the
// same way twice.  Placement is still constrained -- flags land on road, keep
// their distance from each other and from the player's spawn, and never sit on
// a rock or inside the enemy pen -- so a shuffled round is as fair as an
// authored one.
namespace FlagPlacer {

// Spacing rules.  Relaxed automatically if a maze is too tight to satisfy them.
constexpr int MIN_SEPARATION   = 7;   // tiles between any two flags
constexpr int MIN_FROM_SPAWN   = 6;   // so nothing is collected by accident

struct Request {
    const TileMap*         map = nullptr;
    TileSpawn              playerSpawn;
    std::vector<TileSpawn> reserved;      // rocks, the enemy pen, anything else
    int normal  = 0;
    int special = 0;
    int lucky   = 0;
};

// Returns the requested flags, or as many as the maze can actually hold.
// The same seed always produces the same layout.
std::vector<FlagSpawn> place(const Request& req, uint32_t seed);

// The placement rule on its own: `count` reachable, well-spread road tiles
// that clear the spawn and avoid everything in `reserved`.  place() is built
// on it, and so is the turbo scatter -- a power-up has to be as fair to reach
// as a flag, so it must not get a second, looser set of rules.
std::vector<TileSpawn> pickTiles(const Request& req, int count, uint32_t seed);

} // namespace FlagPlacer
} // namespace rx
