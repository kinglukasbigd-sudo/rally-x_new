#pragma once
#include "entities/SmokeCloud.h"
#include <vector>

namespace rx {

class FuelSystem;

// Manages the smoke screen: emission rate, fuel cost, puff lifetimes and the
// queries the enemy AI uses to find out whether it just drove into one.
class SmokeSystem {
public:
    static constexpr float FUEL_PER_PUFF   = 1.2f;   // smoke is never free
    static constexpr float EMIT_INTERVAL   = 0.10f;  // seconds between puffs
    static constexpr int   MAX_CLOUDS      = 24;
    // One press lays exactly this many puffs and then stops.  Holding the
    // button down does nothing further -- you have to let go and press again.
    static constexpr int   PUFFS_PER_BURST = 3;

    void reset();

    // Call once per step with the button state.  A press starts a short burst
    // of PUFFS_PER_BURST puffs behind the car; the burst finishes on its own.
    // Returns true on the step a puff was actually emitted.
    bool emit(const Vec2& behind, FuelSystem& fuel, bool buttonHeld, float dt);

    // True while a burst is still laying its puffs.
    bool bursting() const { return burstRemaining_ > 0; }

    void update(float dt);

    const std::vector<SmokeCloud>& clouds() const { return clouds_; }
    int  activeCount() const;

    // True when `p` is inside any live puff (used to stall enemy cars).
    bool contains(const Vec2& p, float extraRadius = 0.f) const;

private:
    std::vector<SmokeCloud> clouds_;
    float cooldown_       = 0.f;
    int   burstRemaining_ = 0;
    bool  wasHeld_        = false;   // for edge detection: press, not hold
};

} // namespace rx
