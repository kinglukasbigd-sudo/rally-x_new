#include "core/InputManager.h"
#include <SDL.h>

namespace rx {

void InputManager::beginFrame() {
    prev_ = cur_;
    fnPrev_ = fnCur_;
}

void InputManager::setFromExternal(Action a, bool state) {
    cur_[idx(a)] = state;
    if (state) {
        switch (a) {
            case Action::Up:    lastDir_ = Direction::Up;    break;
            case Action::Down:  lastDir_ = Direction::Down;  break;
            case Action::Left:  lastDir_ = Direction::Left;  break;
            case Action::Right: lastDir_ = Direction::Right; break;
            default: break;
        }
    }
}

void InputManager::handleEvent(const SDL_Event& e) {
    if (e.type != SDL_KEYDOWN && e.type != SDL_KEYUP) return;
    if (e.key.repeat) return;

    const bool state = (e.type == SDL_KEYDOWN);
    const SDL_Scancode sc = e.key.keysym.scancode;

    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F9) {
        fnCur_[sc - SDL_SCANCODE_F1 + 1] = state;
        return;
    }

    switch (sc) {
        case SDL_SCANCODE_UP:    case SDL_SCANCODE_W: setFromExternal(Action::Up,    state); break;
        case SDL_SCANCODE_DOWN:  case SDL_SCANCODE_S: setFromExternal(Action::Down,  state); break;
        case SDL_SCANCODE_LEFT:  case SDL_SCANCODE_A: setFromExternal(Action::Left,  state); break;
        case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D: setFromExternal(Action::Right, state); break;
        case SDL_SCANCODE_SPACE: case SDL_SCANCODE_LCTRL:
                                                      setFromExternal(Action::Smoke, state); break;
        case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_1:
                                                      setFromExternal(Action::Start, state); break;
        case SDL_SCANCODE_P:     setFromExternal(Action::Pause, state); break;
        case SDL_SCANCODE_M:     setFromExternal(Action::MuteMusic, state); break;
        case SDL_SCANCODE_ESCAPE:setFromExternal(Action::Quit,  state); break;
        case SDL_SCANCODE_F12:   setFromExternal(Action::Debug, state); break;
        default: break;
    }
}

Direction InputManager::desiredDirection() const {
    // Prefer the most recently pressed direction while it is still held; this
    // is what makes cornering feel like the arcade original.
    auto held = [this](Direction d) {
        switch (d) {
            case Direction::Up:    return down(Action::Up);
            case Direction::Down:  return down(Action::Down);
            case Direction::Left:  return down(Action::Left);
            case Direction::Right: return down(Action::Right);
            default: return false;
        }
    };
    if (lastDir_ != Direction::None && held(lastDir_)) return lastDir_;
    if (down(Action::Up))    return Direction::Up;
    if (down(Action::Down))  return Direction::Down;
    if (down(Action::Left))  return Direction::Left;
    if (down(Action::Right)) return Direction::Right;
    return Direction::None;
}

} // namespace rx
