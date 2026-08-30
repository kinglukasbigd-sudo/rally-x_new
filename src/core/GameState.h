#pragma once

namespace rx {

// Central state machine.  Every transition in the game funnels through
// Game::setState so the flow stays in one readable place.
enum class GameState {
    StartScreen,
    Ready,            // "ROUND n / READY" pause before play begins
    Playing,
    ChallengingStage, // structurally a Playing variant, driven by ChallengeStage
    PlayerDeath,
    RoundComplete,
    GameOver
};

inline const char* stateName(GameState s) {
    switch (s) {
        case GameState::StartScreen:      return "START_SCREEN";
        case GameState::Ready:            return "READY";
        case GameState::Playing:          return "PLAYING";
        case GameState::ChallengingStage: return "CHALLENGING_STAGE";
        case GameState::PlayerDeath:      return "PLAYER_DEATH";
        case GameState::RoundComplete:    return "ROUND_COMPLETE";
        case GameState::GameOver:         return "GAME_OVER";
    }
    return "?";
}

} // namespace rx
