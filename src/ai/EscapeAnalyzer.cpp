#include "ai/EscapeAnalyzer.h"
#include "world/TileMap.h"
#include <algorithm>
#include <limits>

namespace rx {
namespace {

constexpr float INF = std::numeric_limits<float>::max();

// Breadth-first distance in tiles from one source over road tiles.
// Returns -1 for anything walled off or unreachable.
void floodTiles(const TileMap& map, int sx, int sy, std::vector<int>& dist) {
    const int w = map.width(), h = map.height();
    dist.assign(static_cast<size_t>(w) * h, -1);
    if (sx < 0 || sy < 0 || sx >= w || sy >= h || !map.isRoad(sx, sy)) return;

    std::vector<int> queue;
    queue.reserve(dist.size());
    queue.push_back(sy * w + sx);
    dist[static_cast<size_t>(sy) * w + sx] = 0;

    for (size_t head = 0; head < queue.size(); ++head) {
        const int cur = queue[head];
        const int x = cur % w, y = cur / w;
        const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int i = 0; i < 4; ++i) {
            const int nx = x + dx[i], ny = y + dy[i];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t n = static_cast<size_t>(ny) * w + nx;
            if (dist[n] >= 0 || !map.isRoad(nx, ny)) continue;
            dist[n] = dist[static_cast<size_t>(cur)] + 1;
            queue.push_back(static_cast<int>(n));
        }
    }
}

// Fixed steps a car of this speed needs to cover `tiles` tiles.
float travelSteps(int tiles, float speed) {
    if (tiles < 0) return INF;
    if (speed <= 0.f) return INF;
    return static_cast<float>(tiles) * static_cast<float>(TILE) / speed;
}

EscapeAnalyzer::Result run(const TileMap& map,
                           int px, int py, float playerSpeed,
                           const std::vector<EscapeAnalyzer::Pursuer>& pursuers,
                           int ignore, int overrideIndex, int ox, int oy) {
    EscapeAnalyzer::Result out;
    const int w = map.width(), h = map.height();
    if (w <= 0 || h <= 0) return out;
    if (px < 0 || py < 0 || px >= w || py >= h || !map.isRoad(px, py)) return out;

    // When the nearest car reaches each tile.  Every car is flooded
    // separately because they do not all drive at the same speed.
    std::vector<float> enemyAt(static_cast<size_t>(w) * h, INF);
    std::vector<int>   scratch;
    for (size_t i = 0; i < pursuers.size(); ++i) {
        if (static_cast<int>(i) == ignore) continue;
        const auto& p = pursuers[i];
        if (!p.active) continue;

        int ex = TileMap::toTile(p.pos.x);
        int ey = TileMap::toTile(p.pos.y);
        if (static_cast<int>(i) == overrideIndex) { ex = ox; ey = oy; }
        if (ex < 0 || ey < 0 || ex >= w || ey >= h) continue;

        floodTiles(map, ex, ey, scratch);
        for (size_t t = 0; t < enemyAt.size(); ++t)
            enemyAt[t] = std::min(enemyAt[t], travelSteps(scratch[t], p.speed));
    }

    // Now the player's own flood, but restricted to tiles they would win the
    // race to.  Because it only ever expands through tiles that are already
    // safe, everything it reaches is reachable *and* survivable -- which is
    // the difference between an escape route and a gap on the map.
    std::vector<int8_t> firstDir(static_cast<size_t>(w) * h, -1);
    std::vector<char>   seen(static_cast<size_t>(w) * h, 0);

    std::vector<int> queue;
    queue.reserve(seen.size());
    const size_t start = static_cast<size_t>(py) * w + px;
    seen[start] = 1;
    queue.push_back(static_cast<int>(start));

    std::vector<int> steps(static_cast<size_t>(w) * h, 0);

    for (size_t head = 0; head < queue.size(); ++head) {
        const int cur = queue[head];
        const int x = cur % w, y = cur / w;
        const Direction dirs[4] = { Direction::Up, Direction::Down,
                                    Direction::Left, Direction::Right };
        for (Direction d : dirs) {
            const int nx = x + dirDX(d), ny = y + dirDY(d);
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t n = static_cast<size_t>(ny) * w + nx;
            if (seen[n] || !map.isRoad(nx, ny)) continue;

            const int tiles = steps[static_cast<size_t>(cur)] + 1;
            if (travelSteps(tiles, playerSpeed) + EscapeAnalyzer::SAFETY_STEPS >= enemyAt[n])
                continue;                       // a car gets there first

            seen[n]     = 1;
            steps[n]    = tiles;
            // Tiles inherit whichever direction the player left home by, so
            // the region splits cleanly into one area per escape route.
            firstDir[n] = (cur == static_cast<int>(start))
                        ? static_cast<int8_t>(d)
                        : firstDir[static_cast<size_t>(cur)];
            queue.push_back(static_cast<int>(n));

            if (cur == static_cast<int>(start)) out.dirOpen[static_cast<int>(d)] = true;
        }
    }

    out.freeTiles = static_cast<int>(queue.size()) - 1;   // not counting home
    for (size_t t = 0; t < firstDir.size(); ++t)
        if (firstDir[t] >= 0) ++out.dirTiles[firstDir[t]];

    for (int d = 0; d < 4; ++d)
        if (out.dirOpen[d] && out.dirTiles[d] >= EscapeAnalyzer::MIN_ROUTE_TILES)
            ++out.routes;

    out.trapped = (out.routes == 0);
    return out;
}

} // namespace

EscapeAnalyzer::Result EscapeAnalyzer::analyze(const TileMap& map,
                                               const Vec2& playerPos, float playerSpeed,
                                               const std::vector<Pursuer>& pursuers,
                                               int ignore) {
    return run(map, TileMap::toTile(playerPos.x), TileMap::toTile(playerPos.y),
               playerSpeed, pursuers, ignore, -1, 0, 0);
}

EscapeAnalyzer::Result EscapeAnalyzer::analyzeWithMove(const TileMap& map,
                                                       const Vec2& playerPos, float playerSpeed,
                                                       const std::vector<Pursuer>& pursuers,
                                                       int index, Direction step) {
    if (index < 0 || index >= static_cast<int>(pursuers.size()) || step == Direction::None)
        return analyze(map, playerPos, playerSpeed, pursuers);

    const int ex = TileMap::toTile(pursuers[static_cast<size_t>(index)].pos.x) + dirDX(step);
    const int ey = TileMap::toTile(pursuers[static_cast<size_t>(index)].pos.y) + dirDY(step);
    return run(map, TileMap::toTile(playerPos.x), TileMap::toTile(playerPos.y),
               playerSpeed, pursuers, -1, index, ex, ey);
}

} // namespace rx
