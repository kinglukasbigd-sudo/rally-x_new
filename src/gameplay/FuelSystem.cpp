#include "gameplay/FuelSystem.h"
#include <algorithm>

namespace rx {

void FuelSystem::reset(float capacity, float drainPerSecond) {
    capacity_ = capacity;
    fuel_     = capacity;
    drain_    = drainPerSecond;
}

void FuelSystem::update(float dt) {
    if (infinite_) { fuel_ = capacity_; return; }
    fuel_ = std::max(0.f, fuel_ - drain_ * dt);
}

bool FuelSystem::consume(float amount) {
    if (infinite_) return true;
    if (fuel_ < amount) { fuel_ = 0.f; return false; }
    fuel_ -= amount;
    return true;
}

} // namespace rx
