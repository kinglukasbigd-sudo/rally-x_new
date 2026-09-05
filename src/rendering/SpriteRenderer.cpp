#include "rendering/SpriteRenderer.h"
#include "rendering/Palette.h"
#include "world/TileMap.h"
#include "world/Camera.h"
#include "entities/Flag.h"
#include <algorithm>

namespace rx {
namespace {

// --------------------------------------------------------------------------
// Original placeholder artwork.  Digits index the palette passed alongside;
// spaces are transparent.  The car is drawn facing UP and rotated in 90-degree
// steps at draw time, which keeps the pixel grid exact.
// --------------------------------------------------------------------------

const char* CAR_FRAME_A[16] = {
    "                ",
    "     000000     ",
    "    00000000    ",
    "  220000000022  ",
    "  220033330022  ",
    "    00333300    ",
    "    00000000    ",
    "    01111110    ",
    "    01111110    ",
    "    00000000    ",
    "  220033330022  ",
    "  220033330022  ",
    "    00000000    ",
    "     000000     ",
    "                ",
    "                ",
};

const char* CAR_FRAME_B[16] = {
    "                ",
    "     000000     ",
    "    00000000    ",
    "    00000000    ",
    "  220033330022  ",
    "  220033330022  ",
    "    00000000    ",
    "    01111110    ",
    "    01111110    ",
    "    00000000    ",
    "    00333300    ",
    "  220033330022  ",
    "  220000000022  ",
    "     000000     ",
    "                ",
    "                ",
};

const char* FLAG_ART[12] = {
    "            ",
    "  0111111   ",
    "  0111111   ",
    "  0111111   ",
    "  011111    ",
    "  0111      ",
    "  01        ",
    "  0         ",
    "  0         ",
    "  0         ",
    "  000       ",
    "            ",
};

const char* ROCK_ART[12] = {
    "            ",
    "    1111    ",
    "   110011   ",
    "  11000011  ",
    "  10000001  ",
    " 1100000011 ",
    " 1000000001 ",
    " 1100000011 ",
    "  10000001  ",
    "  11000011  ",
    "   111111   ",
    "            ",
};

const char* TURBO_ART[12] = {
    "            ",
    "     11     ",
    "    1221    ",
    "   122221   ",
    "  12222221  ",
    "   111111   ",
    "     11     ",
    "    1221    ",
    "   122221   ",
    "  12222221  ",
    "   111111   ",
    "            ",
};

const char* TURBO_ART_BRIGHT[12] = {
    "     11     ",
    "    1221    ",
    "   122221   ",
    "  12222221  ",
    " 1222222221 ",
    "  11111111  ",
    "    1221    ",
    "   122221   ",
    "  12222221  ",
    " 1222222221 ",
    "  11111111  ",
    "            ",
};

const char* SMOKE_SMALL[16] = {
    "                ",
    "                ",
    "                ",
    "                ",
    "                ",
    "      1111      ",
    "     110011     ",
    "    11000011    ",
    "    11000011    ",
    "     110011     ",
    "      1111      ",
    "                ",
    "                ",
    "                ",
    "                ",
    "                ",
};

const char* SMOKE_LARGE[16] = {
    "                ",
    "                ",
    "     111111     ",
    "   1111111111   ",
    "  110000000011  ",
    "  100000000001  ",
    " 11000000000011 ",
    " 10000000000001 ",
    " 10000000000001 ",
    " 11000000000011 ",
    "  100000000001  ",
    "  110000000011  ",
    "   1111111111   ",
    "     111111     ",
    "                ",
    "                ",
};

} // namespace

int SpriteRenderer::angleFor(Direction d) {
    switch (d) {
        case Direction::Up:    return 0;
        case Direction::Right: return 90;
        case Direction::Down:  return 180;
        case Direction::Left:  return 270;
        default:               return 0;
    }
}

void SpriteRenderer::create(Renderer& r) {
    const Color playerPal[4] = { pal::PlayerBody, pal::PlayerTrim, pal::Tyre, pal::Glass };
    const Color stunPal[4]   = { {120,120,132,255}, {200,200,210,255}, pal::Tyre, {90,90,100,255} };

    carPlayer_[0] = r.createSprite(16, 16, CAR_FRAME_A, playerPal, 4);
    carPlayer_[1] = r.createSprite(16, 16, CAR_FRAME_B, playerPal, 4);
    carStunned_   = r.createSprite(16, 16, CAR_FRAME_A, stunPal,   4);

    // One set of pursuit cars per round theme, built once at start-up.
    for (int t = 0; t < ROUND_THEME_COUNT; ++t) {
        const RoundTheme& th = themeByIndex(t);
        const Color enemyPal[4] = { th.enemyBody, th.enemyTrim, pal::Tyre, pal::Glass };
        carEnemy_[t][0] = r.createSprite(16, 16, CAR_FRAME_A, enemyPal, 4);
        carEnemy_[t][1] = r.createSprite(16, 16, CAR_FRAME_B, enemyPal, 4);
    }

    const Color flagN[2] = { pal::FlagPole, pal::FlagNormal  };
    const Color flagS[2] = { pal::FlagPole, pal::FlagSpecial };
    const Color flagL[2] = { pal::FlagPole, pal::FlagLucky   };
    flag_[0] = r.createSprite(12, 12, FLAG_ART, flagN, 2);
    flag_[1] = r.createSprite(12, 12, FLAG_ART, flagS, 2);
    flag_[2] = r.createSprite(12, 12, FLAG_ART, flagL, 2);

    const Color rockPal[2] = { pal::Rock, pal::RockDark };
    rock_ = r.createSprite(12, 12, ROCK_ART, rockPal, 2);

    const Color turboPal[2] = { pal::TurboDark, pal::Turbo };
    turbo_[0] = r.createSprite(12, 12, TURBO_ART,        turboPal, 2);
    turbo_[1] = r.createSprite(12, 12, TURBO_ART_BRIGHT, turboPal, 2);

    const Color smokePal[2] = { pal::Smoke, pal::SmokeDark };
    smoke_[0] = r.createSprite(16, 16, SMOKE_SMALL, smokePal, 2);
    smoke_[1] = r.createSprite(16, 16, SMOKE_LARGE, smokePal, 2);
}

void SpriteRenderer::destroy(Renderer& r) {
    for (auto& s : carPlayer_) r.destroySprite(s);
    for (auto& set : carEnemy_) for (auto& s : set) r.destroySprite(s);
    r.destroySprite(carStunned_);
    for (auto& s : flag_)      r.destroySprite(s);
    r.destroySprite(rock_);
    for (auto& s : turbo_)     r.destroySprite(s);
    for (auto& s : smoke_)     r.destroySprite(s);
}

void SpriteRenderer::drawCarAtScreen(Renderer& r, int cx, int cy, Direction dir,
                                     int frame, bool enemy, int themeIndex) const {
    const Sprite& s = enemy ? carEnemy_[themeIndex % ROUND_THEME_COUNT][frame & 1]
                            : carPlayer_[frame & 1];
    r.drawRotated(s, cx, cy, angleFor(dir));
}

void SpriteRenderer::drawMaze(Renderer& r, const TileMap& map, const Camera& cam,
                              const RoundTheme& theme) const {
    // Only the tiles under the viewport are considered.
    const int tx0 = std::max(0, static_cast<int>(cam.position().x) / TILE - 1);
    const int ty0 = std::max(0, static_cast<int>(cam.position().y) / TILE - 1);
    const int tx1 = std::min(map.width()  - 1, (static_cast<int>(cam.position().x) + VIEW_W) / TILE + 1);
    const int ty1 = std::min(map.height() - 1, (static_cast<int>(cam.position().y) + VIEW_H) / TILE + 1);

    for (int ty = ty0; ty <= ty1; ++ty) {
        for (int tx = tx0; tx <= tx1; ++tx) {
            if (!map.isWall(tx, ty)) continue;
            const int sx = VIEW_X + static_cast<int>(cam.toScreenX(static_cast<float>(tx * TILE)));
            const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(static_cast<float>(ty * TILE)));

            r.fillRect(sx, sy, TILE, TILE, theme.wall);
            // Hard 1px edge highlights only where the block actually ends.
            if (!map.isWall(tx, ty - 1)) r.fillRect(sx, sy, TILE, 1, theme.wallLight);
            if (!map.isWall(tx - 1, ty)) r.fillRect(sx, sy, 1, TILE, theme.wallLight);
            if (!map.isWall(tx, ty + 1)) r.fillRect(sx, sy + TILE - 1, TILE, 1, theme.wallDark);
            if (!map.isWall(tx + 1, ty)) r.fillRect(sx + TILE - 1, sy, 1, TILE, theme.wallDark);
        }
    }
}

void SpriteRenderer::drawPlayer(Renderer& r, const Camera& cam, const Vec2& world,
                                Direction dir, int frame) const {
    const int sx = VIEW_X + static_cast<int>(cam.toScreenX(world.x));
    const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(world.y));
    r.drawRotated(carPlayer_[frame & 1], sx, sy, angleFor(dir));
}

void SpriteRenderer::drawEnemy(Renderer& r, const Camera& cam, const Vec2& world,
                               Direction dir, int frame, bool stunned, int blinkTick,
                               int themeIndex) const {
    const int sx = VIEW_X + static_cast<int>(cam.toScreenX(world.x));
    const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(world.y));
    if (stunned) {
        // Flash between the stunned and normal palettes so the player can see
        // the stun is about to wear off.
        const Sprite& s = ((blinkTick / 6) % 2)
                        ? carEnemy_[themeIndex % ROUND_THEME_COUNT][0] : carStunned_;
        r.drawRotated(s, sx, sy, angleFor(dir));
        return;
    }
    r.drawRotated(carEnemy_[themeIndex % ROUND_THEME_COUNT][frame & 1], sx, sy, angleFor(dir));
}

void SpriteRenderer::drawFlag(Renderer& r, const Camera& cam, const Flag& f) const {
    if (f.collected) return;
    const int sx = VIEW_X + static_cast<int>(cam.toScreenX(f.pos.x));
    const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(f.pos.y));
    r.drawCentered(flag_[static_cast<int>(f.type)], sx, sy);

    // The two bonus flags carry a letter, exactly like the original marks them.
    if (f.type == FlagType::Special) r.text(sx - 1, sy - 3, "S", pal::Black);
    if (f.type == FlagType::Lucky)   r.text(sx - 1, sy - 3, "L", pal::Black);
}

void SpriteRenderer::drawRock(Renderer& r, const Camera& cam, const Vec2& world) const {
    const int sx = VIEW_X + static_cast<int>(cam.toScreenX(world.x));
    const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(world.y));
    r.drawCentered(rock_, sx, sy);
}

void SpriteRenderer::drawTurbo(Renderer& r, const Camera& cam, const Vec2& world,
                               int tick) const {
    const int sx = VIEW_X + static_cast<int>(cam.toScreenX(world.x));
    const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(world.y));
    // Two frames, alternating a few times a second: enough motion to catch the
    // eye across the maze without anything as soft as a fade.
    r.drawCentered(turbo_[(tick / 10) % 2], sx, sy);
}

void SpriteRenderer::drawSmoke(Renderer& r, const Camera& cam, const Vec2& world,
                               float lifeFraction) const {
    const int sx = VIEW_X + static_cast<int>(cam.toScreenX(world.x));
    const int sy = VIEW_Y + static_cast<int>(cam.toScreenY(world.y));
    // Puffs start small, bloom once, then vanish -- two frames, no fading.
    r.drawCentered(smoke_[lifeFraction > 0.75f ? 0 : 1], sx, sy);
}

} // namespace rx
