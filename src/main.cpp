#include "core/Game.h"
// Renames main() to SDL_main() on the platforms that need it (Android does;
// on desktop this header is a no-op).
#include <SDL_main.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
#if defined(__ANDROID__)
    // A phone has no window to size and no keyboard to type on.
    bool fullscreen = true, touchUi = true;
    int  scale = 1;
#else
    bool fullscreen = true, touchUi = false;
    int  scale = 3;
#endif
    // Swipe steering with a press-and-hold for smoke is the default on touch;
    // the on-screen pad is still there behind --touch-pad, and both can be
    // switched from the title screen.
    rx::TouchScheme scheme = rx::TouchScheme::Swipe;
    std::string dataDir = "levels";
    std::string capture;
    int captureFrames = 900, captureEvery = 150;
    int startRound = 1;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc)      scale = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--data") == 0 && i + 1 < argc)  dataDir = argv[++i];
        else if (std::strcmp(argv[i], "--touch") == 0) touchUi = true;
        else if (std::strcmp(argv[i], "--touch-pad") == 0)   { touchUi = true; scheme = rx::TouchScheme::Pad; }
        else if (std::strcmp(argv[i], "--touch-swipe") == 0) { touchUi = true; scheme = rx::TouchScheme::Swipe; }
        else if (std::strcmp(argv[i], "--windowed") == 0) fullscreen = false;
        else if (std::strcmp(argv[i], "--fullscreen") == 0) fullscreen = true;
        else if (std::strcmp(argv[i], "--round") == 0 && i + 1 < argc) startRound = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--capture") == 0 && i + 1 < argc) capture = argv[++i];
        else if (std::strcmp(argv[i], "--capture-frames") == 0 && i + 1 < argc) captureFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--capture-every") == 0 && i + 1 < argc) captureEvery = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("New Rally-X (clean-room reconstruction)\n"
                        "  --scale N   window scale factor (default 3)\n"
                        "  --data DIR  level data directory (default levels)\n"
                        "  --touch       enable touch controls (always on for Android)\n"
                        "  --touch-pad   touch controls as an on-screen four-way pad\n"
                        "  --touch-swipe touch controls as swipe + hold (default)\n"
                        "  --windowed    run in a window instead of fullscreen\n"
                        "  --fullscreen  start fullscreen (the default)\n"
                        "  --round N   dev only: start at round N\n"
                        "  --capture DIR  dev only: run a scripted demo and dump BMP frames\n");
            return 0;
        }
    }
    if (scale < 1) scale = 1;

    rx::Game game;
    if (!game.init(scale, dataDir, startRound, fullscreen, touchUi, scheme)) return 1;
    if (!capture.empty()) game.runCapture(capture, captureFrames, captureEvery);
    else                  game.run();
    game.shutdown();
    return 0;
}
