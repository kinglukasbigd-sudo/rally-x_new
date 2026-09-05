#pragma once

namespace rx {

// The turbo boost: how strong it is, how long it lasts, and how many pickups
// a round gets.  All four numbers live here and nowhere else, so retuning the
// power-up is a one-file change.
namespace TurboRules {

// A noticeable shove rather than a different game.  At the standard 1.35 the
// boosted car runs at 1.82 px/step -- comfortably faster than a round-12
// pursuit car (1.28), and still slower than a challenging stage's chase pack.
constexpr float SPEED_MULTIPLIER = 1.35f;
constexpr float DURATION_SECONDS = 4.0f;
constexpr float PICKUP_RADIUS    = 9.f;   // matches the flags

// How many pickups a round carries.  The ramp is deliberately late: the early
// rounds are meant to be driven, not boosted through.
//   rounds  1-4  -> 0
//   rounds  5-7  -> 2
//   rounds  8-10 -> 3
//   rounds 11+   -> 5
int countForLevel(int level);

} // namespace TurboRules

// The live boost.  It owns nothing but a timer; the Round asks it what the
// car's speed should be, which keeps the power-up out of the movement code.
class TurboSystem {
public:
    void  reset()               { timer_ = 0.f; }
    // Picking up a second turbo while boosted restarts the clock rather than
    // stacking the speed, so the boost can never compound.
    void  activate()            { timer_ = TurboRules::DURATION_SECONDS; }
    void  update(float dt)      { if (timer_ > 0.f) timer_ = (timer_ > dt) ? timer_ - dt : 0.f; }

    bool  active()    const     { return timer_ > 0.f; }
    float remaining() const     { return timer_; }
    float fraction()  const     { return timer_ / TurboRules::DURATION_SECONDS; }

    // The speed the car should be running at right now.
    float speedFor(float baseSpeed) const {
        return active() ? baseSpeed * TurboRules::SPEED_MULTIPLIER : baseSpeed;
    }

private:
    float timer_ = 0.f;
};

} // namespace rx
