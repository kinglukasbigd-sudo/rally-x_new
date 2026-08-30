#pragma once
#include "world/Maze.h"
#include <string>

namespace rx {

// Parses the plain-text .lvl format (see levels/*.lvl and tools/genlevel.py).
// Keeping the parser separate from Maze means the on-disk format can change
// without touching gameplay code.
namespace LevelLoader {
    bool loadFromFile(const std::string& path, LevelData& out);
    bool loadFromString(const std::string& text, LevelData& out);
}

} // namespace rx
