#pragma once

namespace rx {

// Lives and the single arcade bonus-life award.  Nothing else in the game
// touches the life count.
class LifeSystem {
public:
    static constexpr int BONUS_LIFE_SCORE = 20000;

    void reset(int lives);

    // Returns true when the player still has a car left to put on the track.
    bool loseLife();

    // Award the one-off bonus car; returns true the frame it is granted.
    bool checkBonusLife(int score);

    int  lives()    const { return lives_; }
    bool gameOver() const { return lives_ <= 0; }

private:
    int  lives_       = 0;
    bool bonusGiven_  = false;
};

} // namespace rx
