#include "world/TileMap.h"

namespace rx {

void TileMap::resize(int w, int h) {
    w_ = w; h_ = h;
    tiles_.assign(static_cast<size_t>(w) * h, Tile::Road);
}

Tile TileMap::at(int tx, int ty) const {
    // Anything outside the map reads as solid, which keeps every caller from
    // having to bounds-check before asking.
    if (tx < 0 || ty < 0 || tx >= w_ || ty >= h_) return Tile::Wall;
    return tiles_[static_cast<size_t>(ty) * w_ + tx];
}

void TileMap::set(int tx, int ty, Tile t) {
    if (tx < 0 || ty < 0 || tx >= w_ || ty >= h_) return;
    tiles_[static_cast<size_t>(ty) * w_ + tx] = t;
}

} // namespace rx
