#pragma once

namespace rx {

// Fuel is a hard timer on every round.  It drains whenever the car is in play
// and drops sharply when the smoke screen is used, so smoke is never free.
class FuelSystem {
public:
    void  reset(float capacity, float drainPerSecond);

    void  update(float dt);
    bool  consume(float amount);      // false when there is not enough left

    float fuel()     const { return fuel_; }
    float capacity() const { return capacity_; }
    float fraction() const { return capacity_ > 0.f ? fuel_ / capacity_ : 0.f; }
    bool  empty()    const { return fuel_ <= 0.f; }

    void  setInfinite(bool on) { infinite_ = on; }
    bool  infinite() const     { return infinite_; }

private:
    float fuel_     = 100.f;
    float capacity_ = 100.f;
    float drain_    = 1.f;
    bool  infinite_ = false;
};

} // namespace rx
