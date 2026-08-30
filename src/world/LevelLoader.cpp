#include "world/LevelLoader.h"
#include "core/FileSystem.h"
#include <sstream>
#include <algorithm>

namespace rx {
namespace {

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

} // namespace

bool LevelLoader::loadFromString(const std::string& text, LevelData& out) {
    std::istringstream in(text);
    std::string line;
    std::vector<std::string> rows;
    bool inMaze = false;

    out = LevelData{};

    while (std::getline(in, line)) {
        if (inMaze) {
            std::string r = trim(line);
            if (r.empty()) continue;
            if (r == "end") break;
            rows.push_back(r);
            continue;
        }
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        std::istringstream ls(t);
        std::string key; ls >> key;
        if (key == "maze")            { inMaze = true; continue; }
        else if (key == "name")       { std::getline(ls, out.name); out.name = trim(out.name); }
        else if (key == "type")       { std::string v; ls >> v;
                                        out.type = (v == "CHALLENGE") ? RoundType::Challenge
                                                                      : RoundType::Normal; }
        else if (key == "difficulty") { ls >> out.difficulty; }
        else if (key == "fuel")       { ls >> out.fuel; }
        else if (key == "fuelDrain")  { ls >> out.fuelDrain; }
        else if (key == "playerSpeed"){ ls >> out.playerSpeed; }
        else if (key == "enemySpeed") { ls >> out.enemySpeed; }
        else if (key == "enemies")    { ls >> out.enemyCount; }
    }

    if (rows.empty()) return false;

    const int h = static_cast<int>(rows.size());
    int w = 0;
    for (const auto& r : rows) w = std::max<int>(w, static_cast<int>(r.size()));

    out.map.resize(w, h);
    for (int y = 0; y < h; ++y) {
        const std::string& r = rows[y];
        for (int x = 0; x < w; ++x) {
            const char c = (x < static_cast<int>(r.size())) ? r[x] : '#';
            switch (c) {
                case '#': out.map.set(x, y, Tile::Wall); break;
                case 'R': out.rocks.push_back({x, y});   break;
                case 'F': out.flags.push_back({x, y, 0}); break;
                case 'S': out.flags.push_back({x, y, 1}); break;
                case 'L': out.flags.push_back({x, y, 2}); break;
                case 'E': out.enemyPen.push_back({x, y}); break;
                case 'P': out.playerSpawn = {x, y};      break;
                default:  break;   // '.' and anything unknown is plain road
            }
        }
    }

    // Without an explicit count, every pen tile gets a car.
    if (out.enemyCount <= 0)
        out.enemyCount = static_cast<int>(out.enemyPen.size());

    out.valid = true;
    return true;
}

bool LevelLoader::loadFromFile(const std::string& path, LevelData& out) {
    std::string text;
    if (!FileSystem::readTextFile(path, text)) return false;
    return loadFromString(text, out);
}

} // namespace rx
