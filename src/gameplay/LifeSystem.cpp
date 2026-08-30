#include "gameplay/LifeSystem.h"

namespace rx {

void LifeSystem::reset(int lives) {
    lives_ = lives;
    bonusGiven_ = false;
}

bool LifeSystem::loseLife() {
    if (lives_ > 0) --lives_;
    return lives_ > 0;
}

bool LifeSystem::checkBonusLife(int score) {
    if (bonusGiven_ || score < BONUS_LIFE_SCORE) return false;
    bonusGiven_ = true;
    ++lives_;
    return true;
}

} // namespace rx
