#pragma once

namespace rx {

// Lives and the bonus-car awards.  Nothing else in the game touches the life
// count, and nothing else decides what a bonus car costs.
class LifeSystem {
public:
    // The score milestones that earn a car, in ascending order.  Each one pays
    // out once per run: passing 20,000 gives a car, and staying above it does
    // not give another.  Change the table and the behaviour follows -- there
    // is no other place a threshold is written down.
    static constexpr int BONUS_LIFE_SCORES[] = { 20000, 60000, 110000 };
    static constexpr int BONUS_LIFE_COUNT = 3;

    // Kept for the callers and tests that only ever cared about the first one.
    static constexpr int BONUS_LIFE_SCORE = BONUS_LIFE_SCORES[0];

    // Starts a run: the lives are set and every milestone is unclaimed again.
    void reset(int lives);

    // Returns true when the player still has a car left to put on the track.
    bool loseLife();

    // Pays out every milestone the score has passed and not yet claimed, and
    // returns how many cars that was -- normally 0, occasionally 1, and more
    // only if a single bonus vaulted the score over two milestones at once.
    //
    // The test is "score has reached the milestone", not "score equals it", so
    // a jump from 19,950 straight to 20,150 still pays.  A milestone stays
    // claimed for the rest of the run whatever happens to the lives
    // afterwards: crashing does not put the reward back on the table.
    int checkBonusLife(int score);

    int  lives()    const { return lives_; }
    bool gameOver() const { return lives_ <= 0; }

    // Which milestones this run has already paid out.  Exposed so the tests
    // can see the claim state rather than having to infer it from the lives.
    bool bonusClaimed(int index) const {
        return index >= 0 && index < BONUS_LIFE_COUNT && claimed_[index];
    }
    int  bonusesAwarded() const;

private:
    int  lives_ = 0;
    bool claimed_[BONUS_LIFE_COUNT] = { false, false, false };
};

} // namespace rx
