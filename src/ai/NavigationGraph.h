#pragma once
#include "core/Types.h"
#include <vector>

namespace rx {

class TileMap;

// A navigation view of the maze.  Every road tile records which of the four
// directions are open; tiles where a real choice exists (anything that is not
// a plain corridor) become graph nodes joined by corridor edges.  This is what
// the enemy cars steer with -- they follow the maze, never a straight line to
// the player.
class NavigationGraph {
public:
    struct Node {
        int tx = 0, ty = 0;
        int neighbor[4] = { -1, -1, -1, -1 };   // node index per Direction
        int cost[4]     = {  0,  0,  0,  0 };   // corridor length in tiles
    };

    void build(const TileMap& map);

    int  width()  const { return w_; }
    int  height() const { return h_; }

    // Bit per Direction (Up=1, Down=2, Left=4, Right=8).
    uint8_t openMask(int tx, int ty) const;
    bool    isOpen(int tx, int ty, Direction d) const;
    int     openCount(int tx, int ty) const;

    // A junction, a corner or a dead end -- anywhere the AI must decide.
    bool isDecisionTile(int tx, int ty) const;

    int nodeAt(int tx, int ty) const;                  // -1 when not a node
    const std::vector<Node>& nodes() const { return nodes_; }

    // Legal directions from a tile, optionally excluding a straight reversal.
    std::vector<Direction> legalMoves(int tx, int ty, Direction excludeReverseOf) const;

private:
    int index(int tx, int ty) const { return ty * w_ + tx; }

    int w_ = 0, h_ = 0;
    std::vector<uint8_t> mask_;
    std::vector<int>     nodeIndex_;
    std::vector<Node>    nodes_;
};

} // namespace rx
