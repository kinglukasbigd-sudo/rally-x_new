#include "gameplay/FlagPlacer.h"
#include "world/TileMap.h"
#include <algorithm>
#include <cstdlib>

namespace rx {
namespace FlagPlacer {

namespace {

uint32_t nextRandom(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

int manhattan(const TileSpawn& a, const TileSpawn& b) {
    return std::abs(a.tx - b.tx) + std::abs(a.ty - b.ty);
}

// Every road tile the player can actually drive to, with its distance from the
// spawn.  Using a flood fill rather than straight-line distance means a flag is
// never placed somewhere the maze cannot reach.
std::vector<int> reachableDistances(const TileMap& map, const TileSpawn& from) {
    const int w = map.width(), h = map.height();
    std::vector<int> dist(static_cast<size_t>(w) * h, -1);
    if (!map.isRoad(from.tx, from.ty)) return dist;

    std::vector<std::pair<int,int>> queue{{from.tx, from.ty}};
    dist[static_cast<size_t>(from.ty) * w + from.tx] = 0;

    for (size_t head = 0; head < queue.size(); ++head) {
        const auto [x, y] = queue[head];
        const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
        for (int i = 0; i < 4; ++i) {
            const int nx = x + dx[i], ny = y + dy[i];
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const size_t n = static_cast<size_t>(ny) * w + nx;
            if (dist[n] >= 0 || !map.isRoad(nx, ny)) continue;
            dist[n] = dist[static_cast<size_t>(y) * w + x] + 1;
            queue.push_back({nx, ny});
        }
    }
    return dist;
}

} // namespace

std::vector<TileSpawn> pickTiles(const Request& req, int wanted, uint32_t seed) {
    std::vector<TileSpawn> chosen;
    if (!req.map || wanted <= 0) return chosen;

    const TileMap& map = *req.map;
    const int w = map.width(), h = map.height();

    const std::vector<int> dist = reachableDistances(map, req.playerSpawn);

    // Candidates: reachable road, far enough from the spawn, nothing already
    // occupying the tile.
    std::vector<TileSpawn> pool;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int d = dist[static_cast<size_t>(y) * w + x];
            if (d < MIN_FROM_SPAWN) continue;

            const TileSpawn t{ x, y };
            bool blocked = false;
            for (const auto& r : req.reserved)
                if (r.tx == x && r.ty == y) { blocked = true; break; }
            if (!blocked) pool.push_back(t);
        }
    }
    if (pool.empty()) return chosen;

    uint32_t rng = seed ? seed : 1u;
    for (size_t i = pool.size(); i > 1; --i)
        std::swap(pool[i - 1], pool[nextRandom(rng) % i]);

    // Take spread-out tiles first, easing the spacing only if the maze cannot
    // supply enough at the ideal separation.
    for (int separation = MIN_SEPARATION; separation >= 0 && static_cast<int>(chosen.size()) < wanted;
         --separation) {
        for (const auto& c : pool) {
            if (static_cast<int>(chosen.size()) >= wanted) break;
            bool ok = true;
            for (const auto& t : chosen)
                if (manhattan(c, t) < separation || (c.tx == t.tx && c.ty == t.ty)) { ok = false; break; }
            if (ok) chosen.push_back(c);
        }
    }
    return chosen;
}

std::vector<FlagSpawn> place(const Request& req, uint32_t seed) {
    std::vector<FlagSpawn> out;
    const int wanted = req.normal + req.special + req.lucky;
    const std::vector<TileSpawn> chosen = pickTiles(req, wanted, seed);

    // Normal flags first, then the two bonus flags, so a short maze still gets
    // a full objective before it gets extras.
    int i = 0;
    for (int n = 0; n < req.normal  && i < static_cast<int>(chosen.size()); ++n, ++i)
        out.push_back({ chosen[i].tx, chosen[i].ty, 0 });
    for (int n = 0; n < req.special && i < static_cast<int>(chosen.size()); ++n, ++i)
        out.push_back({ chosen[i].tx, chosen[i].ty, 1 });
    for (int n = 0; n < req.lucky   && i < static_cast<int>(chosen.size()); ++n, ++i)
        out.push_back({ chosen[i].tx, chosen[i].ty, 2 });

    return out;
}

} // namespace FlagPlacer
} // namespace rx
