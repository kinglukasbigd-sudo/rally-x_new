#include "gameplay/LifeSystem.h"

namespace rx {

constexpr int LifeSystem::BONUS_LIFE_SCORES[];

void LifeSystem::reset(int lives) {
    lives_ = lives;
    for (bool& c : claimed_) c = false;
}

bool LifeSystem::loseLife() {
    if (lives_ > 0) --lives_;
    return lives_ > 0;
}

int LifeSystem::checkBonusLife(int score) {
    int awarded = 0;
    for (int i = 0; i < BONUS_LIFE_COUNT; ++i) {
        if (claimed_[i] || score < BONUS_LIFE_SCORES[i]) continue;
        claimed_[i] = true;
        ++lives_;
        ++awarded;
    }
    return awarded;
}

int LifeSystem::bonusesAwarded() const {
    int n = 0;
    for (bool c : claimed_) if (c) ++n;
    return n;
}

} // namespace rx
