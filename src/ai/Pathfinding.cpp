#include "ai/Pathfinding.h"
#include "world/TileMap.h"
#include <vector>
#include <queue>
#include <cstdlib>
#include <limits>

namespace rx {
namespace Pathfinding {

namespace {
const Direction kDirs[4] = { Direction::Up, Direction::Down, Direction::Left, Direction::Right };
int manhattan(int ax, int ay, int bx, int by) { return std::abs(ax - bx) + std::abs(ay - by); }
}

Direction firstStep(const TileMap& map, int fromX, int fromY, int toX, int toY,
                    Direction banned) {
    if (fromX == toX && fromY == toY) return Direction::None;
    if (!map.isRoad(toX, toY)) return greedyStep(map, fromX, fromY, toX, toY, banned);

    const int w = map.width(), h = map.height();
    const int n = w * h;
    const int INF = std::numeric_limits<int>::max();

    std::vector<int> g(n, INF);
    std::vector<Direction> firstMove(n, Direction::None);

    struct Item { int f, idx; };
    struct Cmp { bool operator()(const Item& a, const Item& b) const { return a.f > b.f; } };
    std::priority_queue<Item, std::vector<Item>, Cmp> open;

    const Direction reverse = opposite(banned);
    const int start = fromY * w + fromX;
    g[start] = 0;

    // Seed with the legal first moves so every node remembers which way the
    // route left the start tile.
    for (Direction d : kDirs) {
        if (d == reverse) continue;
        const int nx = fromX + dirDX(d), ny = fromY + dirDY(d);
        if (!map.isRoad(nx, ny)) continue;
        const int idx = ny * w + nx;
        if (g[idx] <= 1) continue;
        g[idx] = 1;
        firstMove[idx] = d;
        open.push({ 1 + manhattan(nx, ny, toX, toY), idx });
    }

    while (!open.empty()) {
        const Item cur = open.top(); open.pop();
        const int cx = cur.idx % w, cy = cur.idx / w;
        if (cx == toX && cy == toY) return firstMove[cur.idx];
        if (cur.f - manhattan(cx, cy, toX, toY) > g[cur.idx]) continue;   // stale

        for (Direction d : kDirs) {
            const int nx = cx + dirDX(d), ny = cy + dirDY(d);
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            if (!map.isRoad(nx, ny)) continue;
            const int idx = ny * w + nx;
            const int ng = g[cur.idx] + 1;
            if (ng >= g[idx]) continue;
            g[idx] = ng;
            firstMove[idx] = firstMove[cur.idx];
            open.push({ ng + manhattan(nx, ny, toX, toY), idx });
        }
    }
    return greedyStep(map, fromX, fromY, toX, toY, banned);
}

Direction greedyStep(const TileMap& map, int fromX, int fromY, int toX, int toY,
                     Direction banned) {
    const Direction reverse = opposite(banned);
    Direction best = Direction::None;
    int bestDist = std::numeric_limits<int>::max();

    for (Direction d : kDirs) {
        if (d == reverse) continue;
        const int nx = fromX + dirDX(d), ny = fromY + dirDY(d);
        if (!map.isRoad(nx, ny)) continue;
        const int dist = manhattan(nx, ny, toX, toY);
        if (dist < bestDist) { bestDist = dist; best = d; }
    }
    // A dead end: reversing is the only thing left, exactly as the original
    // cars do when they run themselves into a pocket.
    if (best == Direction::None && map.isRoad(fromX + dirDX(reverse), fromY + dirDY(reverse)))
        best = reverse;
    return best;
}

} // namespace Pathfinding
} // namespace rx
