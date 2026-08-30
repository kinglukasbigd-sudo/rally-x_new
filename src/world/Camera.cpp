#include "world/Camera.h"
#include <algorithm>

namespace rx {

void Camera::centerOn(const Vec2& worldPos) {
    pos_.x = worldPos.x - vw_ * 0.5f;
    pos_.y = worldPos.y - vh_ * 0.5f;

    // The maze is bounded (it does not wrap), so hold the view inside it.
    const float maxX = static_cast<float>(WORLD_W - vw_);
    const float maxY = static_cast<float>(WORLD_H - vh_);
    pos_.x = std::clamp(pos_.x, 0.f, std::max(0.f, maxX));
    pos_.y = std::clamp(pos_.y, 0.f, std::max(0.f, maxY));
}

} // namespace rx
