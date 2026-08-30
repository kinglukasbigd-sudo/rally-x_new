#pragma once

namespace rx {

// Development-only switches, toggled with F1..F9.  None of these are reachable
// from normal play; they exist so gameplay can be inspected while it is built.
struct DebugFlags {
    bool enabled       = false;  // master overlay (F12)
    bool collisionBoxes= false;  // F1
    bool navGraph      = false;  // F2
    bool enemyTargets  = false;  // F3
    bool coordinates   = false;  // F4
    bool radarDebug    = false;  // F5
    bool infiniteFuel  = false;  // F6
    bool freezeEnemies = false;  // F7
};

} // namespace rx
