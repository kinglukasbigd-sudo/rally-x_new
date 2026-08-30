#pragma once

namespace rx {

class Round;
class ScoreSystem;

// The rules that make a Challenging Stage different from a normal round, kept
// in one place instead of being sprinkled through the enemy AI as special
// cases.  A challenging stage has no pursuit at all: it is a straight race
// against the fuel gauge for flags.
//
// Behaviour reconstructed for this phase:
//   * no enemy cars are placed (Round::spawnEnemies skips them entirely)
//   * the stage ends when every flag is taken, or when the run ends early
//     because the tank ran dry or the car hit a rock
//   * ending early does NOT cost a car -- a bonus stage never takes a life
//   * clearing every flag pays a perfect bonus on top of the flag scores
class ChallengeStage {
public:
    static constexpr int PERFECT_BONUS = 5000;

    struct Outcome {
        bool finished = false;
        bool perfect  = false;
        int  bonus    = 0;
    };

    void begin();

    // Fed the round's events once per step while the stage is running.
    Outcome update(const Round& round, bool playerDied, bool roundComplete,
                   ScoreSystem& score);

    bool  running() const { return running_; }
    bool  perfect() const { return perfect_; }

    // A challenging stage never costs the player a car.
    static bool costsALife() { return false; }

private:
    bool running_ = false;
    bool perfect_ = false;
};

} // namespace rx
