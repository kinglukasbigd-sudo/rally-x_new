#pragma once
#include "core/Types.h"
#include "rendering/Renderer.h"
#include "rendering/RoundTheme.h"

namespace rx {

class TileMap;
class Camera;
struct Flag;

// Owns every piece of artwork and knows how to put the world on screen.
// All sprites are generated at start-up from the character-grid art in
// SpriteRenderer.cpp -- there are no external image files and nothing is
// derived from the original game's graphics.
class SpriteRenderer {
public:
    void create(Renderer& r);
    void destroy(Renderer& r);

    // The scrolling maze, drawn clipped to the viewport, in the round's colours.
    void drawMaze(Renderer& r, const TileMap& map, const Camera& cam,
                  const RoundTheme& theme) const;

    void drawPlayer(Renderer& r, const Camera& cam, const Vec2& world,
                    Direction dir, int frame) const;
    void drawEnemy(Renderer& r, const Camera& cam, const Vec2& world,
                   Direction dir, int frame, bool stunned, int blinkTick,
                   int themeIndex) const;
    void drawFlag(Renderer& r, const Camera& cam, const Flag& f) const;
    void drawRock(Renderer& r, const Camera& cam, const Vec2& world) const;
    void drawSmoke(Renderer& r, const Camera& cam, const Vec2& world,
                   float lifeFraction) const;

    // Screen-space draw, used by the attract/title screen where there is no
    // camera or world to speak of.
    void drawCarAtScreen(Renderer& r, int cx, int cy, Direction dir,
                         int frame, bool enemy, int themeIndex = 0) const;

    static int angleFor(Direction d);

private:
    Sprite carPlayer_[2]{};
    Sprite carEnemy_[ROUND_THEME_COUNT][2]{};
    Sprite carStunned_{};
    Sprite flag_[3]{};
    Sprite rock_{};
    Sprite smoke_[2]{};
};

} // namespace rx
