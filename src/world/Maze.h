#pragma once
#include "core/Types.h"
#include "world/TileMap.h"
#include <string>
#include <vector>

namespace rx {

enum class RoundType { Normal, Challenge };

struct FlagSpawn {
    int tx = 0, ty = 0;
    int kind = 0;   // 0 = normal, 1 = special, 2 = lucky (see FlagType)
};

struct TileSpawn { int tx = 0, ty = 0; };

// A parsed level: the static maze plus everything that has to be respawned
// when a round restarts.  Rounds are restored from this, never mutated in it.
struct LevelData {
    std::string name       = "ROUND";
    RoundType   type       = RoundType::Normal;
    int         difficulty = 1;
    float       fuel       = 100.f;   // tank capacity
    float       fuelDrain  = 1.0f;    // units per second while driving
    float       playerSpeed = 1.35f;
    float       enemySpeed  = 1.05f;

    TileMap                map;
    TileSpawn              playerSpawn;

    // The enemy pen: one straight run of road tiles, marked 'E' in the level
    // file, well away from the player's spawn.  Every pursuit car in the round
    // starts here and they all launch together, so a round always opens with
    // the pack in one known place and the player with room to get clear.
    std::vector<TileSpawn> enemyPen;
    int                    enemyCount = 0;   // cars to place; clamped to the pen
    std::vector<TileSpawn> rocks;
    std::vector<FlagSpawn> flags;

    bool valid = false;
};

// Owns the level currently being played.
class Maze {
public:
    bool load(const std::string& path);
    void loadFallback();                    // built-in maze if data is missing

    const LevelData& data() const { return data_; }
    const TileMap&   map()  const { return data_.map; }

    // Centre of a tile in world pixels.
    static Vec2 tileCenterWorld(int tx, int ty) {
        return Vec2(tx * TILE + TILE * 0.5f, ty * TILE + TILE * 0.5f);
    }

private:
    LevelData data_;
};

} // namespace rx
