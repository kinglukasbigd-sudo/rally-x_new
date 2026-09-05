#include "rendering/Radar.h"
#include "rendering/Renderer.h"
#include "rendering/Palette.h"
#include "gameplay/Round.h"
#include "entities/Enemy.h"

namespace rx {

void Radar::blip(Renderer& r, const Vec2& world, Color c, int size) const {
    const int x = RADAR_X + static_cast<int>(world.x * SCALE) - size / 2;
    const int y = RADAR_Y + static_cast<int>(world.y * SCALE) - size / 2;
    r.fillRect(x, y, size, size, c);
}

void Radar::draw(Renderer& r, const Round& round, int blinkTick, bool showMaze,
                 const RoundTheme& theme) const {
    r.fillRect(RADAR_X, RADAR_Y, RADAR_W, RADAR_H, pal::RadarBack);
    r.drawRect(RADAR_X - 1, RADAR_Y - 1, RADAR_W + 2, RADAR_H + 2, pal::PanelLine);

    // Off by default: the original radar shows objectives, not the maze.
    if (showMaze) {
        const TileMap& m = round.map();
        for (int ty = 0; ty < m.height(); ++ty)
            for (int tx = 0; tx < m.width(); ++tx)
                if (m.isWall(tx, ty))
                    r.drawPixel(RADAR_X + static_cast<int>(tx * TILE * SCALE),
                                RADAR_Y + static_cast<int>(ty * TILE * SCALE), theme.wallDark);
    }

    for (const auto& f : round.flags()) {
        if (f.collected) continue;
        Color c = pal::FlagNormal;
        if (f.type == FlagType::Special) c = pal::FlagSpecial;
        if (f.type == FlagType::Lucky)   c = pal::FlagLucky;
        blip(r, f.pos, c, 2);
    }

    // Boosts show on the radar like objectives do -- knowing where the next
    // one is is half of using it.
    for (const auto& t : round.turbos()) {
        if (t.collected) continue;
        blip(r, t.pos, pal::Turbo, 2);
    }

    for (const auto& e : round.enemies()) {
        if (!e.onTrack()) continue;
        blip(r, e.position(), e.stunned() ? pal::TextDim : theme.enemyBody, 2);
    }

    // The player blip flashes so it never gets lost among the flags.
    if ((blinkTick / 12) % 2 == 0 || !round.player().alive())
        blip(r, round.player().position(), pal::PlayerTrim, 3);
    else
        blip(r, round.player().position(), pal::PlayerBody, 3);
}

} // namespace rx
