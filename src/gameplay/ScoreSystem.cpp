#include "gameplay/ScoreSystem.h"
#include "core/Types.h"
#include <algorithm>

namespace rx {

void ScoreSystem::newGame() {
    score_ = 0;
    sequence_ = 0;
    multiplier_ = 1;
}

void ScoreSystem::newRound() {
    sequence_ = 0;
    multiplier_ = 1;
}

void ScoreSystem::onPlayerDeath() {
    // Losing a life ends the Special Flag effect but keeps the flags already
    // banked for this round.
    multiplier_ = 1;
}

int ScoreSystem::nextFlagValue() const {
    const int step = std::min(sequence_ + 1, FLAGS_PER_ROUND);
    return step * 100 * multiplier_;
}

int ScoreSystem::awardNormalFlag() {
    const int points = nextFlagValue();
    ++sequence_;
    bump(points);
    return points;
}

void ScoreSystem::addBonus(int points) { bump(points); }

void ScoreSystem::bump(int points) {
    score_ += points;
    high_ = std::max(high_, score_);
}

} // namespace rx
