#pragma once
#include "core/Types.h"
#include "core/GameState.h"
#include "core/InputManager.h"
#include "core/Debug.h"
#include "core/TouchControls.h"
#include "gameplay/Round.h"
#include "gameplay/ScoreSystem.h"
#include "gameplay/LifeSystem.h"
#include "gameplay/ChallengeStage.h"
#include "rendering/Renderer.h"
#include "rendering/SpriteRenderer.h"
#include "rendering/HUD.h"
#include "rendering/Radar.h"
#include "rendering/RoundTheme.h"
#include "audio/AudioManager.h"
#include <string>
#include <vector>

namespace rx {

// Owns the whole application: window, fixed-step loop, state machine.
// Every state transition goes through setState so the flow stays in one place.
class Game {
public:
    bool init(int scale, const std::string& dataDir, int startRound = 1,
              bool fullscreen = false, bool touchUi = false,
              TouchScheme scheme = TouchScheme::Swipe, uint32_t seed = 0);
    void run();
    // Dev tool: runs a scripted demo with no player at the keyboard and dumps
    // BMP frames, so rendering and gameplay can be checked without a display.
    void runCapture(const std::string& outDir, int frames, int everyN);
    void shutdown();

    // Exposed for the headless test harness, which drives the same simulation
    // without ever opening a window.  pumpInput() drains the SDL event queue
    // exactly as the real loop does, so pushed touch events take the same path
    // through the game as a finger on a screen.
    void pumpInput() { handleEvents(); }
    void step(float dt);
    GameState state() const { return state_; }
    bool paused() const { return paused_; }
    Rect pauseButtonRect() const { return pauseButtonRect_; }
    const Round& round() const { return round_; }
    const ScoreSystem& score() const { return score_; }
    const LifeSystem&  lifeSystem() const { return lives_; }
    const AudioManager& audio() const { return audio_; }
    int  roundNumber() const { return roundNumber_; }
    InputManager& input() { return input_; }
    // Development shortcut (F8): banks every remaining flag in the round.
    void debugCollectAllFlags() { round_.debugCollectAllFlags(score_); }

private:
    void handleEvents();
    void fixedUpdate(float dt);
    void render();

    void setState(GameState s);
    void playRoundSounds(const Round::Events& ev);
    void updateLowFuelWarning(float dt);
    void startNewGame();
    void onPlayerDied(DeathCause cause);
    void loadRound(int roundNumber);
    std::string levelPath(int roundNumber) const;

    void renderStartScreen();
    void renderWorld();
    void renderOverlayText();
    void renderDebug();
    void drawTouchControls();
    void drawSwipeFeedback();
    void drawPauseButton();
    void updatePauseButtonRect();
    void setPaused(bool on);
    bool canPause() const;

    Renderer       renderer_;
    SpriteRenderer sprites_;
    HUD            hud_;
    AudioManager   audio_;
    Radar          radar_;
    InputManager   input_;
    ScoreSystem    score_;
    LifeSystem     lives_;
    Round          round_;
    ChallengeStage challenge_;
    DebugFlags     debug_;
    TouchControls  touch_;
    RoundTheme     theme_ = themeFor(1);

    GameState state_      = GameState::StartScreen;
    float     stateTimer_ = 0.f;
    DeathCause deathCause_ = DeathCause::None;
    int       luckyBonusShown_ = 0;
    int       challengeBonusShown_ = 0;
    bool      challengePerfect_ = false;
    int       roundNumber_= 1;
    bool      running_    = false;
    int       tick_       = 0;
    float     fps_        = 0.f;
    float     lowFuelTimer_ = 0.f;
    int       lastLives_  = START_LIVES;
    Rect      schemeToggleRect_{};
    Rect      musicToggleRect_{};
    Rect      soundToggleRect_{};
    Rect      pauseButtonRect_{};
    bool      paused_ = false;
    float     muteNoticeTimer_ = 0.f;
    bool      hasTouchDevice_ = false;

    std::string dataDir_ = "levels";
    int  levelCount_ = 0;
    int  startRound_ = 1;   // dev option: jump straight to a later round

    // Drives the per-round flag shuffle.  Fixed with --seed for a repeatable
    // game; otherwise it comes from the clock, so no two games lay out alike.
    uint32_t gameSeed_ = 0;
    uint32_t flagSeedFor(int roundNumber) const;
};

} // namespace rx
