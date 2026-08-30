#include "gameplay/ChallengeStage.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"

namespace rx {

void ChallengeStage::begin() {
    running_ = true;
    perfect_ = false;
}

ChallengeStage::Outcome ChallengeStage::update(const Round& round, bool playerDied,
                                               bool roundComplete, ScoreSystem& score) {
    Outcome out;
    if (!running_) return out;

    if (roundComplete) {
        // Every flag taken: pay the perfect bonus.
        running_ = false;
        perfect_ = true;
        out.finished = true;
        out.perfect  = true;
        out.bonus    = PERFECT_BONUS;
        score.addBonus(out.bonus);
        return out;
    }

    if (playerDied) {
        // The run is over, but no car is lost and no bonus is paid.
        running_ = false;
        out.finished = true;
        return out;
    }
    return out;
}

} // namespace rx
