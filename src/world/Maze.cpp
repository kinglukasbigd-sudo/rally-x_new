#include "world/Maze.h"
#include "world/LevelLoader.h"

namespace rx {

bool Maze::load(const std::string& path) {
    LevelData d;
    if (!LevelLoader::loadFromFile(path, d)) return false;
    data_ = std::move(d);
    return true;
}

void Maze::loadFallback() {
    // Deliberately tiny: this only exists so a missing levels/ directory
    // produces a playable (if dull) maze instead of a crash.
    std::string text =
        "name FALLBACK\ntype NORMAL\ndifficulty 1\nfuel 100\nmaze\n";
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            char c = '.';
            if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) c = '#';
            else if (x % 4 == 2 && y % 4 == 2) c = '#';
            text += c;
        }
        text += '\n';
    }
    LevelLoader::loadFromString(text, data_);
    data_.playerSpawn = {1, 1};
    for (int i = 0; i < FLAGS_PER_ROUND; ++i)
        data_.flags.push_back({3 + (i % 7) * 4, 3 + (i / 7) * 6, 0});
}

} // namespace rx
