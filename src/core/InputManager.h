#pragma once
#include "core/Types.h"
#include <array>

union SDL_Event;

namespace rx {

// Abstract game actions.  Nothing downstream of this class knows about SDL
// scancodes, so a different source (touch, gamepad) can be added later by
// feeding the same action set -- Player movement never has to change.
enum class Action {
    Up, Down, Left, Right, Smoke, Start, Pause, Quit, Debug, MuteMusic,
    Count
};

class InputManager {
public:
    void beginFrame();                  // roll current state into previous
    void handleEvent(const SDL_Event& e);

    bool down(Action a)     const { return cur_[idx(a)]; }
    bool pressed(Action a)  const { return cur_[idx(a)] && !prev_[idx(a)]; }
    bool released(Action a) const { return !cur_[idx(a)] && prev_[idx(a)]; }

    // Most recently requested movement direction (arcade behaviour: the last
    // key the player touched wins, so diagonal mashing never deadlocks).
    Direction desiredDirection() const;

    // Debug function keys F1..F9, edge-triggered.
    bool debugKeyPressed(int n) const { return n >= 1 && n <= 9 && fnCur_[n] && !fnPrev_[n]; }

    void setFromExternal(Action a, bool state);   // used by headless tests

private:
    static constexpr int idx(Action a) { return static_cast<int>(a); }
    static constexpr int N = static_cast<int>(Action::Count);

    std::array<bool, N> cur_{}, prev_{};
    std::array<bool, 10> fnCur_{}, fnPrev_{};
    Direction lastDir_ = Direction::None;
};

} // namespace rx
