#include "gameplay/SmokeSystem.h"
#include "gameplay/FuelSystem.h"
#include <algorithm>

namespace rx {

void SmokeSystem::reset() {
    clouds_.clear();
    cooldown_ = 0.f;
    burstRemaining_ = 0;
    wasHeld_ = false;
}

bool SmokeSystem::emit(const Vec2& behind, FuelSystem& fuel, bool buttonHeld, float dt) {
    cooldown_ = std::max(0.f, cooldown_ - dt);

    // Only the moment the button goes down starts anything.  Holding it after
    // that is ignored, which is what stops one press draining the whole tank.
    const bool pressed = buttonHeld && !wasHeld_;
    wasHeld_ = buttonHeld;
    if (pressed && burstRemaining_ == 0) burstRemaining_ = PUFFS_PER_BURST;

    if (burstRemaining_ <= 0 || cooldown_ > 0.f) return false;
    if (activeCount() >= MAX_CLOUDS) { burstRemaining_ = 0; return false; }
    if (!fuel.consume(FUEL_PER_PUFF)) { burstRemaining_ = 0; return false; }

    --burstRemaining_;

    SmokeCloud c;
    c.pos      = behind;
    c.lifetime = c.maxLifetime;
    c.active   = true;

    // Reuse an expired slot before growing the list.
    auto it = std::find_if(clouds_.begin(), clouds_.end(),
                           [](const SmokeCloud& s) { return !s.active; });
    if (it != clouds_.end()) *it = c; else clouds_.push_back(c);

    cooldown_ = EMIT_INTERVAL;
    return true;
}

void SmokeSystem::update(float dt) {
    for (auto& c : clouds_) {
        if (!c.active) continue;
        c.lifetime -= dt;
        if (c.lifetime <= 0.f) { c.lifetime = 0.f; c.active = false; }
    }
}

int SmokeSystem::activeCount() const {
    return static_cast<int>(std::count_if(clouds_.begin(), clouds_.end(),
                                          [](const SmokeCloud& c) { return c.active; }));
}

bool SmokeSystem::contains(const Vec2& p, float extraRadius) const {
    for (const auto& c : clouds_) {
        if (!c.active) continue;
        const float dx = p.x - c.pos.x, dy = p.y - c.pos.y;
        const float r  = c.radius + extraRadius;
        if (dx * dx + dy * dy <= r * r) return true;
    }
    return false;
}

} // namespace rx
