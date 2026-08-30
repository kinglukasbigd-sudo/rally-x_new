#include "gameplay/LuckyFlagBonusCalculator.h"
#include <algorithm>
#include <cmath>

namespace rx {
namespace LuckyFlagBonusCalculator {

int bonusFor(float fuel, float capacity) {
    if (capacity <= 0.f) return MIN_BONUS;
    const float frac = std::clamp(fuel / capacity, 0.f, 1.f);

    // Linear in remaining fuel, then snapped down to the nearest 100 so the
    // award always reads like an arcade score.
    const int raw = static_cast<int>(std::lround(MIN_BONUS + frac * (MAX_BONUS - MIN_BONUS)));
    const int snapped = (raw / STEP) * STEP;
    return std::clamp(snapped, MIN_BONUS, MAX_BONUS);
}

} // namespace LuckyFlagBonusCalculator
} // namespace rx
