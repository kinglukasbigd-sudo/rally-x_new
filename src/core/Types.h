#pragma once
#include <cstdint>

namespace rx {

// ---------------------------------------------------------------------------
// Global tuning constants.  The original ran on 288x224 arcade hardware; we
// render into an internal framebuffer of exactly that size and scale it up
// with nearest-neighbour so the result stays pixel-exact.
// ---------------------------------------------------------------------------
constexpr int  SCREEN_W      = 288;
constexpr int  SCREEN_H      = 224;

constexpr int  TILE          = 16;   // world tile size in pixels
constexpr int  MAP_W         = 32;   // tiles
constexpr int  MAP_H         = 32;
constexpr int  WORLD_W       = MAP_W * TILE;   // 512
constexpr int  WORLD_H       = MAP_H * TILE;

// Scrolling viewport (left portion of the screen), below the score strip.
// The strip is 20px tall so a 7px label row, a 7px digit row and the divider
// each get their own pixels -- at 16 the divider clipped the score digits.
constexpr int  VIEW_X        = 0;
constexpr int  VIEW_Y        = 20;
constexpr int  VIEW_W        = 208;
constexpr int  VIEW_H        = SCREEN_H - VIEW_Y;

// Right-hand instrument panel: radar, fuel gauge, lives.
constexpr int  PANEL_X       = VIEW_W;
constexpr int  PANEL_W       = SCREEN_W - VIEW_W;

constexpr int  RADAR_X       = PANEL_X + 8;
constexpr int  RADAR_Y       = VIEW_Y + 8;
constexpr int  RADAR_W       = 64;   // WORLD_W / 8
constexpr int  RADAR_H       = 64;

constexpr double FIXED_DT    = 1.0 / 60.0;  // simulation step

constexpr int  CAR_SIZE      = 16;   // sprite footprint
constexpr int  FLAGS_PER_ROUND = 10; // the classic objective
constexpr int  START_LIVES   = 3;

// ---------------------------------------------------------------------------

enum class Direction : uint8_t { Up = 0, Down, Left, Right, None };

inline int dirDX(Direction d) {
    switch (d) { case Direction::Left: return -1; case Direction::Right: return 1; default: return 0; }
}
inline int dirDY(Direction d) {
    switch (d) { case Direction::Up: return -1; case Direction::Down: return 1; default: return 0; }
}
inline Direction opposite(Direction d) {
    switch (d) {
        case Direction::Up:    return Direction::Down;
        case Direction::Down:  return Direction::Up;
        case Direction::Left:  return Direction::Right;
        case Direction::Right: return Direction::Left;
        default:               return Direction::None;
    }
}
inline const char* dirName(Direction d) {
    switch (d) {
        case Direction::Up: return "UP"; case Direction::Down: return "DOWN";
        case Direction::Left: return "LEFT"; case Direction::Right: return "RIGHT";
        default: return "NONE";
    }
}

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2() = default;
    Vec2(float px, float py) : x(px), y(py) {}
};

// Axis-aligned collision box in world pixels.
struct Rect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;
    bool overlaps(const Rect& o) const {
        return x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y;
    }
};

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

} // namespace rx
