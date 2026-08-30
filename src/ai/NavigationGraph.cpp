#include "ai/NavigationGraph.h"
#include "world/TileMap.h"

namespace rx {

namespace {
constexpr uint8_t bitFor(Direction d) { return static_cast<uint8_t>(1u << static_cast<int>(d)); }
const Direction kDirs[4] = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
}

void NavigationGraph::build(const TileMap& map) {
    w_ = map.width();
    h_ = map.height();
    mask_.assign(static_cast<size_t>(w_) * h_, 0);
    nodeIndex_.assign(static_cast<size_t>(w_) * h_, -1);
    nodes_.clear();

    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            if (!map.isRoad(x, y)) continue;
            uint8_t m = 0;
            for (Direction d : kDirs)
                if (map.isRoad(x + dirDX(d), y + dirDY(d))) m |= bitFor(d);
            mask_[index(x, y)] = m;
        }

    // Nodes first, so edges can reference them by index.
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x) {
            if (!map.isRoad(x, y) || !isDecisionTile(x, y)) continue;
            nodeIndex_[index(x, y)] = static_cast<int>(nodes_.size());
            Node n; n.tx = x; n.ty = y;
            nodes_.push_back(n);
        }

    // Edges: walk each corridor from a node until the next node is reached.
    for (auto& n : nodes_) {
        for (Direction d : kDirs) {
            if (!isOpen(n.tx, n.ty, d)) continue;
            int cx = n.tx + dirDX(d), cy = n.ty + dirDY(d), steps = 1;
            while (cx >= 0 && cy >= 0 && cx < w_ && cy < h_ &&
                   mask_[index(cx, cy)] != 0 && nodeIndex_[index(cx, cy)] < 0) {
                cx += dirDX(d); cy += dirDY(d); ++steps;
            }
            if (cx < 0 || cy < 0 || cx >= w_ || cy >= h_) continue;
            const int target = nodeIndex_[index(cx, cy)];
            if (target < 0) continue;
            n.neighbor[static_cast<int>(d)] = target;
            n.cost[static_cast<int>(d)]     = steps;
        }
    }
}

uint8_t NavigationGraph::openMask(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= w_ || ty >= h_) return 0;
    return mask_[index(tx, ty)];
}

bool NavigationGraph::isOpen(int tx, int ty, Direction d) const {
    if (d == Direction::None) return false;
    return (openMask(tx, ty) & bitFor(d)) != 0;
}

int NavigationGraph::openCount(int tx, int ty) const {
    const uint8_t m = openMask(tx, ty);
    int n = 0;
    for (int i = 0; i < 4; ++i) if (m & (1u << i)) ++n;
    return n;
}

bool NavigationGraph::isDecisionTile(int tx, int ty) const {
    const uint8_t m = openMask(tx, ty);
    if (m == 0) return false;
    const int n = openCount(tx, ty);
    if (n != 2) return true;                       // junction or dead end
    // Exactly two exits: a corner counts, a straight run does not.
    const bool vertical   = (m & bitFor(Direction::Up))   && (m & bitFor(Direction::Down));
    const bool horizontal = (m & bitFor(Direction::Left)) && (m & bitFor(Direction::Right));
    return !(vertical || horizontal);
}

int NavigationGraph::nodeAt(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= w_ || ty >= h_) return -1;
    return nodeIndex_[index(tx, ty)];
}

std::vector<Direction> NavigationGraph::legalMoves(int tx, int ty,
                                                   Direction excludeReverseOf) const {
    std::vector<Direction> out;
    const Direction banned = opposite(excludeReverseOf);
    for (Direction d : kDirs) {
        if (!isOpen(tx, ty, d)) continue;
        if (d == banned) continue;
        out.push_back(d);
    }
    return out;
}

} // namespace rx
