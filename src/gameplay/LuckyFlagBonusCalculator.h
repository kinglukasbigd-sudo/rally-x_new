#pragma once

namespace rx {

// The Lucky Flag pays out against whatever fuel is left in the tank, so it is
// worth most when it is grabbed early.  Isolated here so the exact curve can
// be retuned without touching any gameplay code.
namespace LuckyFlagBonusCalculator {

constexpr int MIN_BONUS  = 100;
constexpr int MAX_BONUS  = 2000;
constexpr int STEP       = 100;   // payouts are always round hundreds

// `fuel` and `capacity` are in tank units.
int bonusFor(float fuel, float capacity);

} // namespace LuckyFlagBonusCalculator
} // namespace rx
