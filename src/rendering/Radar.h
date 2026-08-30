#pragma once
#include "core/Types.h"
#include "rendering/RoundTheme.h"

namespace rx {

class Renderer;
class Round;

// The right-hand radar.  It shows the whole maze at 1/8 scale as coloured
// blips -- player, enemies, flags -- exactly as an arcade radar would, with
// no map detail and no minimap styling.
class Radar {
public:
    static constexpr float SCALE = static_cast<float>(RADAR_W) / WORLD_W;

    // `blinkTick` drives the player blip's flash so it is easy to pick out.
    void draw(Renderer& r, const Round& round, int blinkTick, bool showMaze,
              const RoundTheme& theme) const;

private:
    void blip(Renderer& r, const Vec2& world, Color c, int size) const;
};

} // namespace rx
