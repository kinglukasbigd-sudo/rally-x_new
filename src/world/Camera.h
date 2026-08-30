#pragma once
#include "core/Types.h"

namespace rx {

// Arcade-style follow camera: the world position is simply centred on the
// player and clamped to the map bounds.  No smoothing, no look-ahead, no
// cinematic easing -- the original scrolls rigidly with the car.
class Camera {
public:
    void setViewport(int w, int h) { vw_ = w; vh_ = h; }
    void centerOn(const Vec2& worldPos);
    void snapTo(const Vec2& worldPos) { centerOn(worldPos); }

    const Vec2& position() const { return pos_; }   // top-left, world pixels

    // World -> screen. Callers add the viewport origin themselves.
    float toScreenX(float wx) const { return wx - pos_.x; }
    float toScreenY(float wy) const { return wy - pos_.y; }

    bool visible(float wx, float wy, float margin = 16.f) const {
        const float sx = toScreenX(wx), sy = toScreenY(wy);
        return sx > -margin && sy > -margin && sx < vw_ + margin && sy < vh_ + margin;
    }

private:
    Vec2 pos_;
    int  vw_ = VIEW_W, vh_ = VIEW_H;
};

} // namespace rx
