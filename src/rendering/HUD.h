#pragma once
#include "core/Types.h"
#include <string>

namespace rx {

class Renderer;

// Everything the HUD needs, so the HUD never reaches into game systems.
struct HudInfo {
    int   score          = 0;
    int   highScore      = 0;
    int   lives          = 0;
    int   round          = 1;
    float fuel           = 0.f;
    float fuelMax        = 1.f;
    int   flagsRemaining = 0;
    int   multiplier     = 1;
    bool  challenge      = false;
};

// Classic arcade instrumentation only: score strip along the top, radar and
// gauges down the right-hand side.  No panels, gradients or animation.
class HUD {
public:
    void draw(Renderer& r, const HudInfo& info) const;

private:
    void drawScoreStrip(Renderer& r, const HudInfo& info) const;
    void drawPanel(Renderer& r, const HudInfo& info) const;
    void drawFuelGauge(Renderer& r, const HudInfo& info) const;
    void drawLives(Renderer& r, const HudInfo& info) const;
};

std::string padNumber(int value, int digits);

} // namespace rx
