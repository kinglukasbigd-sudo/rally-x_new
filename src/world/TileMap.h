#pragma once
#include "core/Types.h"
#include <vector>

namespace rx {

enum class Tile : uint8_t {
    Road = 0,
    Wall,
};

// Pure tile storage + queries.  Knows nothing about entities or rendering;
// every collision question about the static world is answered here rather
// than by ad-hoc rectangles scattered through the code.
class TileMap {
public:
    void resize(int w, int h);

    int  width()  const { return w_; }
    int  height() const { return h_; }

    Tile at(int tx, int ty) const;
    void set(int tx, int ty, Tile t);

    bool isWall(int tx, int ty) const { return at(tx, ty) == Tile::Wall; }
    bool isRoad(int tx, int ty) const { return at(tx, ty) == Tile::Road; }

    // World-pixel helpers.
    bool isWallAtPixel(float px, float py) const {
        return isWall(static_cast<int>(px) / TILE, static_cast<int>(py) / TILE);
    }
    static int  toTile(float px)  { return static_cast<int>(px) / TILE; }
    static float tileCenter(int t) { return t * TILE + TILE * 0.5f; }

private:
    int w_ = 0, h_ = 0;
    std::vector<Tile> tiles_;
};

} // namespace rx
