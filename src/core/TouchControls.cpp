#include "core/TouchControls.h"
#include <algorithm>
#include <cmath>

namespace rx {

namespace {
bool inside(const Rect& r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
}

void TouchControls::setScheme(TouchScheme s) {
    if (s == scheme_) return;
    scheme_ = s;
    releaseAll();
    steer_ = Direction::None;
}

void TouchControls::layout(int winW, int winH, const Rect& game) {
    // A flick is measured against the short edge, so it feels the same on a
    // phone and on a tablet.
    swipeDistance_ = std::max(12.f, std::min(winW, winH) * SWIPE_DISTANCE_FRAC);

    for (auto& r : rects_) r = Rect{};
    if (scheme_ != TouchScheme::Pad) { placement_ = PadPlacement::Sides; return; }

    const float leftBar   = game.x;
    const float rightBar  = static_cast<float>(winW) - (game.x + game.w);
    const float sideBar   = std::min(leftBar, rightBar);
    const float bottomBar = static_cast<float>(winH) - (game.y + game.h);

    float padCx, padCy, btnCx, btnCy, unit;

    if (sideBar >= winH * 0.22f) {
        // Landscape: wide margins either side of the picture.
        placement_ = PadPlacement::Sides;
        unit  = std::min(sideBar * 0.30f, winH * 0.20f);
        padCx = leftBar * 0.5f;
        btnCx = static_cast<float>(winW) - rightBar * 0.5f;
        padCy = btnCy = winH * 0.62f;
    } else if (bottomBar >= winW * 0.30f) {
        // Portrait: a deep band below the picture, pad left, button right,
        // both well inside thumb reach.
        placement_ = PadPlacement::Below;
        unit  = std::min(winW * 0.12f, bottomBar * 0.24f);
        padCx = winW * 0.26f;
        btnCx = winW * 0.76f;
        padCy = btnCy = game.y + game.h + bottomBar * 0.46f;
    } else {
        // Neither: overlay the bottom corners of the picture.
        placement_ = PadPlacement::Overlay;
        unit  = std::min(winW * 0.075f, winH * 0.14f);
        padCx = game.x + unit * 2.0f;
        btnCx = game.x + game.w - unit * 1.8f;
        padCy = btnCy = game.y + game.h - unit * 2.0f;
    }

    const float s = unit;
    rects_[index(Action::Up)]    = Rect{ padCx - s * 0.5f, padCy - s * 1.5f, s, s };
    rects_[index(Action::Down)]  = Rect{ padCx - s * 0.5f, padCy + s * 0.5f, s, s };
    rects_[index(Action::Left)]  = Rect{ padCx - s * 1.5f, padCy - s * 0.5f, s, s };
    rects_[index(Action::Right)] = Rect{ padCx + s * 0.5f, padCy - s * 0.5f, s, s };

    const float b = s * 1.5f;
    rects_[index(Action::Smoke)] = Rect{ btnCx - b * 0.5f, btnCy - b * 0.5f, b, b };
}

// ---------------------------------------------------------------------------
// Finger bookkeeping
// ---------------------------------------------------------------------------

TouchControls::Finger* TouchControls::find(int64_t id) {
    for (auto& f : fingers_) if (f.down && f.id == id) return &f;
    return nullptr;
}

TouchControls::Finger* TouchControls::acquire(int64_t id) {
    if (Finger* f = find(id)) return f;
    for (auto& f : fingers_) if (!f.down) { f = Finger{}; f.id = id; f.down = true; return &f; }
    return nullptr;
}

void TouchControls::update(float dt) {
    now_ += dt;
    if (scheme_ != TouchScheme::Swipe) return;

    // A still finger charges towards smoke; the longest-held one owns the
    // indicator, so a second finger steering does not reset it.
    holdActive_  = false;
    holdElapsed_ = 0.f;
    bool anyCharged = false;

    for (auto& f : fingers_) {
        if (!f.down || f.flicked) continue;
        const float heldFor = now_ - f.downAt;
        if (heldFor >= HOLD_TO_SMOKE) f.charged = true;
        if (f.charged) anyCharged = true;

        if (heldFor > holdElapsed_) {
            holdElapsed_ = heldFor;
            holdPoint_   = f.current;
            holdActive_  = true;
        }
    }
    smoking_ = anyCharged;
}

void TouchControls::fingerDown(int64_t id, float x, float y) {
    Finger* f = acquire(id);
    if (!f) return;
    f->origin = f->current = Vec2{ x, y };
    f->downAt = now_;
    f->flicked = f->charged = false;

    if (scheme_ == TouchScheme::Pad) {
        f->action = hitTest(x, y);
        refresh();
    }
}

void TouchControls::fingerMove(int64_t id, float x, float y) {
    Finger* f = find(id);
    if (!f) return;
    f->current = Vec2{ x, y };

    if (scheme_ == TouchScheme::Pad) {
        f->action = hitTest(x, y);
        refresh();
        return;
    }
    resolveSwipe(*f);
}

void TouchControls::resolveSwipe(Finger& f) {
    const float dx = f.current.x - f.origin.x;
    const float dy = f.current.y - f.origin.y;
    if (std::fabs(dx) < swipeDistance_ && std::fabs(dy) < swipeDistance_) return;

    // The bigger axis wins outright: there are no diagonals in this maze.
    if (std::fabs(dx) >= std::fabs(dy)) steer_ = (dx > 0.f) ? Direction::Right : Direction::Left;
    else                                steer_ = (dy > 0.f) ? Direction::Down  : Direction::Up;

    f.flicked = true;
    f.charged = false;          // a flick can never also be a hold
    // Re-anchor so a second flick in the same drag registers.
    f.origin = f.current;
}

void TouchControls::fingerUp(int64_t id) {
    Finger* f = find(id);
    if (!f) return;

    // Any press that did not become a flick and did not already fire as a
    // hold counts as a tap, however long it lasted.  Requiring it to be quick
    // meant a slightly slow press on the screen did nothing at all.
    const bool wasTap = !f->flicked && !f->charged &&
                        (scheme_ == TouchScheme::Pad ? f->action == Action::Count : true);
    if (wasTap) tapPending_ = true;

    *f = Finger{};
    refresh();
    if (scheme_ == TouchScheme::Swipe) update(0.f);
}

void TouchControls::releaseAll() {
    for (auto& f : fingers_) f = Finger{};
    held_.fill(false);
    smoking_ = false;
    holdActive_ = false;
    holdElapsed_ = 0.f;
}

void TouchControls::refresh() {
    held_.fill(false);
    for (const auto& f : fingers_)
        if (f.down && f.action != Action::Count) held_[index(f.action)] = true;
}

bool TouchControls::consumeTap() {
    const bool t = tapPending_;
    tapPending_ = false;
    return t;
}

bool  TouchControls::holdCharging() const {
    return holdActive_ && !smoking_ && holdElapsed_ > 0.15f;
}

float TouchControls::holdProgress() const {
    if (!holdActive_) return 0.f;
    return std::clamp(holdElapsed_ / HOLD_TO_SMOKE, 0.f, 1.f);
}

void TouchControls::applyTo(InputManager& input) const {
    if (!enabled_) return;

    // Every play action is written every step, false as well as true.  A
    // keyboard releases itself with a key-up event; touch has no such thing,
    // so writing only the presses would latch an action on forever -- which
    // silently costs you every smoke burst after the first, because the button
    // never goes back up to be pressed again.
    bool want[5] = { false, false, false, false, false };
    const Action order[5] = { Action::Up, Action::Down, Action::Left,
                              Action::Right, Action::Smoke };

    if (scheme_ == TouchScheme::Pad) {
        for (int i = 0; i < 5; ++i) want[i] = held_[index(order[i])];
    } else {
        // Swipe: the last flick keeps steering until the next one, which is
        // what makes the car behave like it does under a held joystick.
        switch (steer_) {
            case Direction::Up:    want[0] = true; break;
            case Direction::Down:  want[1] = true; break;
            case Direction::Left:  want[2] = true; break;
            case Direction::Right: want[3] = true; break;
            default: break;
        }
        want[4] = smoking_;
    }

    // Releases first, so the direction that is still held is the one that ends
    // up as the most recently pressed.
    for (int i = 0; i < 5; ++i) if (!want[i]) input.setFromExternal(order[i], false);
    for (int i = 0; i < 5; ++i) if (want[i])  input.setFromExternal(order[i], true);
}

Action TouchControls::hitTest(float x, float y) const {
    if (scheme_ != TouchScheme::Pad) return Action::Count;
    for (Action a : { Action::Up, Action::Down, Action::Left, Action::Right, Action::Smoke })
        if (inside(rects_[index(a)], x, y)) return a;
    return Action::Count;
}

} // namespace rx
