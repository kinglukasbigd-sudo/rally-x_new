#include "rendering/HUD.h"
#include "rendering/Renderer.h"
#include "rendering/Palette.h"
#include <algorithm>

namespace rx {

std::string padNumber(int value, int digits) {
    std::string s = std::to_string(std::max(0, value));
    while (static_cast<int>(s.size()) < digits) s.insert(s.begin(), '0');
    return s;
}

void HUD::draw(Renderer& r, const HudInfo& info) const {
    drawScoreStrip(r, info);
    drawPanel(r, info);
}

void HUD::drawScoreStrip(Renderer& r, const HudInfo& info) const {
    r.fillRect(0, 0, SCREEN_W, VIEW_Y, pal::Black);

    // Two clear rows: labels on top, six-digit scores underneath.
    r.text(4, 2, "1UP", pal::Danger);
    r.text(4, 11, padNumber(info.score, 6), pal::Text);

    const std::string high = padNumber(info.highScore, 6);
    r.textCentered(SCREEN_W / 2 + 40, 2,  "HIGH SCORE", pal::Accent);
    r.textCentered(SCREEN_W / 2 + 40, 11, high, pal::Text);

    r.fillRect(0, VIEW_Y - 1, SCREEN_W, 1, pal::PanelLine);
}

void HUD::drawPanel(Renderer& r, const HudInfo& info) const {
    r.fillRect(PANEL_X, VIEW_Y, PANEL_W, SCREEN_H - VIEW_Y, pal::Black);
    r.fillRect(PANEL_X, VIEW_Y, 1, SCREEN_H - VIEW_Y, pal::PanelLine);

    const int cx = PANEL_X + PANEL_W / 2;

    r.textCentered(cx, RADAR_Y + RADAR_H + 6,
                   info.challenge ? "CHALLENGE" : ("ROUND " + std::to_string(info.round)),
                   info.challenge ? pal::Accent : pal::Text);

    r.text(PANEL_X + 8, RADAR_Y + RADAR_H + 18, "FLAGS", pal::TextDim);
    r.text(PANEL_X + 48, RADAR_Y + RADAR_H + 18, padNumber(info.flagsRemaining, 2),
           info.flagsRemaining == 0 ? pal::Accent : pal::Text);

    if (info.multiplier > 1)
        r.textCentered(cx, RADAR_Y + RADAR_H + 28, "SPECIAL X2", pal::FlagSpecial);

    drawFuelGauge(r, info);
    drawLives(r, info);
}

void HUD::drawFuelGauge(Renderer& r, const HudInfo& info) const {
    const int x = PANEL_X + 8;
    const int y = SCREEN_H - 46;
    const int w = PANEL_W - 16;
    const int h = 8;

    r.text(x, y - 10, "FUEL", pal::TextDim);
    r.drawRect(x, y, w, h, pal::PanelLine);

    const float frac = (info.fuelMax > 0.f)
                     ? std::clamp(info.fuel / info.fuelMax, 0.f, 1.f) : 0.f;
    const int fill = static_cast<int>(frac * (w - 2));
    if (fill > 0)
        r.fillRect(x + 1, y + 1, fill, h - 2, frac < 0.25f ? pal::FuelLow : pal::FuelHigh);
}

void HUD::drawLives(Renderer& r, const HudInfo& info) const {
    const int y = SCREEN_H - 24;
    r.text(PANEL_X + 8, y, "CARS", pal::TextDim);
    for (int i = 0; i < std::min(info.lives, 5); ++i)
        r.fillRect(PANEL_X + 8 + i * 8, y + 10, 5, 7, pal::PlayerBody);
}

} // namespace rx
