#pragma once
#include "core/Types.h"
#include "core/InputManager.h"
#include <array>

namespace rx {

// Touch input, in two schemes.
//
//   Pad   - an on-screen four-way pad and a smoke button.  The arcade control
//           scheme, transplanted onto glass.
//   Swipe - flick to set the car's direction, press and hold to lay smoke.
//           Nothing is drawn over the playfield, so the picture can use the
//           whole screen.
//
// Both emit exactly the same Action values the keyboard does, so Player and
// the rest of the game never learn that touch exists.
enum class TouchScheme { Pad, Swipe };

class TouchControls {
public:
    static constexpr int   MAX_FINGERS         = 8;
    // Press and hold this long to start laying smoke.
    static constexpr float HOLD_TO_SMOKE       = 2.0f;
    // A flick has to travel this share of the short screen edge to count.
    static constexpr float SWIPE_DISTANCE_FRAC = 0.035f;
    // A press that neither travelled far enough to be a flick nor was held
    // long enough to fire on its own is a tap, whatever its length.  During
    // play a tap is the smoke button; on a menu it is Start.
    static constexpr float TAP_MAX_SECONDS     = 0.35f;   // kept for reference

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const     { return enabled_; }

    void setScheme(TouchScheme s);
    TouchScheme scheme() const { return scheme_; }

    // Where the pad ended up for the current window.
    enum class PadPlacement { Sides, Below, Overlay };

    // Positions the pad for the current window.  `game` is where the playfield
    // sits, so the pad can use whichever empty band the screen leaves: the
    // sides in landscape, the strip underneath in portrait, or -- when there
    // is no room at all -- overlaid on the bottom corners.  Also sets the
    // swipe distance for this screen.
    void layout(int winW, int winH, const Rect& game);

    PadPlacement placement() const { return placement_; }

    // Advances hold timers.  Call once per simulation step.
    void update(float dt);

    // Window-pixel coordinates.
    void fingerDown(int64_t id, float x, float y);
    void fingerMove(int64_t id, float x, float y);
    void fingerUp(int64_t id);
    void releaseAll();

    // Pushes the current state into the shared action set.
    void applyTo(InputManager& input) const;

    // True once per tap that was not a flick, a completed hold, or a press on
    // one of the pad's own buttons.  The caller decides what a tap means: in
    // play it fires the smoke, on a menu it is Start.
    bool consumeTap();

    // --- pad scheme ---
    Action hitTest(float x, float y) const;
    const Rect& rect(Action a) const { return rects_[index(a)]; }
    bool  pressed(Action a) const    { return held_[index(a)]; }
    bool  overlaid() const           { return placement_ == PadPlacement::Overlay; }

    // --- swipe scheme ---
    Direction steering() const   { return steer_; }
    bool  smoking() const        { return smoking_; }
    // 0..1 while a hold is charging towards smoke; 0 when nothing is charging.
    float holdProgress() const;
    bool  holdCharging() const;
    Vec2  holdPoint() const      { return holdPoint_; }

private:
    static int index(Action a) { return static_cast<int>(a); }
    static constexpr int N = static_cast<int>(Action::Count);

    struct Finger {
        int64_t id      = -1;
        bool    down    = false;
        Action  action  = Action::Count;   // pad scheme
        Vec2    origin;                    // swipe scheme
        Vec2    current;
        float   downAt  = 0.f;
        bool    flicked = false;           // travelled far enough to be a swipe
        bool    charged = false;           // held long enough to lay smoke
    };

    Finger* find(int64_t id);
    Finger* acquire(int64_t id);
    void    assign(int64_t id, Action a);
    void    refresh();
    void    resolveSwipe(Finger& f);

    std::array<Rect, N> rects_{};
    std::array<bool, N> held_{};
    std::array<Finger, MAX_FINGERS> fingers_{};

    TouchScheme scheme_   = TouchScheme::Swipe;
    bool  enabled_        = false;
    PadPlacement placement_ = PadPlacement::Sides;
    bool  tapPending_     = false;

    Direction steer_      = Direction::None;
    bool  smoking_        = false;
    Vec2  holdPoint_;
    float holdElapsed_    = 0.f;
    bool  holdActive_     = false;

    float now_            = 0.f;
    float swipeDistance_  = 24.f;
};

} // namespace rx
