#pragma once
#include "entities/Flag.h"

namespace rx {

// The single owner of the score.  Entities never touch a score variable; they
// report an event here and this class decides what it is worth.
class ScoreSystem {
public:
    void newGame();       // full reset, keeps the high score
    void newRound();      // flag sequence restarts each round
    void onPlayerDeath(); // temporary bonuses expire with the life

    // Awards the next flag in the sequence: 100, 200, ... 1000, doubled while
    // the Special Flag effect is active.  Returns the points actually given.
    int awardNormalFlag();

    // The Special Flag itself scores nothing; it doubles what follows.
    void activateSpecialFlag() { multiplier_ = 2; }

    void addBonus(int points);

    int  score()      const { return score_; }
    int  highScore()  const { return high_; }
    int  multiplier() const { return multiplier_; }
    int  flagsScored()const { return sequence_; }

    // Value the next normal flag would be worth (used by the HUD).
    int  nextFlagValue() const;

private:
    void bump(int points);

    int score_      = 0;
    int high_       = 0;
    int sequence_   = 0;   // normal flags taken this round
    int multiplier_ = 1;
};

} // namespace rx
