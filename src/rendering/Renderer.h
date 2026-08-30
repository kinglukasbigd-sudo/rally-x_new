#pragma once
#include "core/Types.h"
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace rx {

// A sprite is a small nearest-filtered texture built from character-grid
// artwork defined in SpriteRenderer.  All artwork in this project is original.
struct Sprite {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    bool valid() const { return tex != nullptr; }
};

// Owns the window and the 288x224 internal framebuffer.  Everything the game
// draws goes into that framebuffer; it is blitted to the window once per
// frame with integer nearest-neighbour scaling, which is what preserves the
// hard-edged arcade look.
class Renderer {
public:
    bool init(const char* title, int scale, bool fullscreen = false);
    void shutdown();

    void beginFrame();

    // Presenting is split so an overlay (the touch controls) can be drawn in
    // window pixels, outside the 288x224 playfield, before the flip.
    void beginPresent();
    void endPresent();
    void present() { beginPresent(); endPresent(); }

    // Where the playfield lands inside the window, in window pixels.
    Rect gameRect() const { return gameRect_; }
    void windowSize(int& w, int& h) const { w = winW_; h = winH_; }

    // Whole-number scaling keeps every game pixel exactly square, but it
    // throws away up to a whole step of screen.  Filling instead scales to
    // the screen edge, which is what a phone wants; the aspect ratio is
    // preserved either way, so the picture is never stretched.
    void setFillScreen(bool on) { fillScreen_ = on; }
    bool fillScreen() const     { return fillScreen_; }

    void clear(Color c);
    void fillRect(int x, int y, int w, int h, Color c);
    void drawRect(int x, int y, int w, int h, Color c);   // outline
    void drawPixel(int x, int y, Color c);
    void drawLine(int x0, int y0, int x1, int y1, Color c);

    void setClip(int x, int y, int w, int h);
    void clearClip();

    Sprite createSprite(int w, int h, const char* const* rows,
                        const Color* palette, int paletteSize);
    void   destroySprite(Sprite& s);
    void   draw(const Sprite& s, int x, int y);              // top-left
    void   drawCentered(const Sprite& s, int cx, int cy);
    // 90-degree steps only: exact, so the pixel grid survives rotation.
    void   drawRotated(const Sprite& s, int cx, int cy, int angleDeg);

    void text(int x, int y, const std::string& str, Color c);
    void textCentered(int cx, int y, const std::string& str, Color c);
    static int textWidth(const std::string& str);

    // Dev tool: dumps the internal framebuffer (288x224, unscaled) to a BMP.
    bool saveScreenshot(const std::string& path);

    SDL_Renderer* sdl() const { return ren_; }
    bool headless() const { return headless_; }

private:
    SDL_Window*   win_  = nullptr;
    SDL_Renderer* ren_  = nullptr;
    SDL_Texture*  fb_   = nullptr;
    bool          headless_ = false;
    int           winW_ = SCREEN_W, winH_ = SCREEN_H;
    Rect          gameRect_{};
    bool          fillScreen_ = false;
};

} // namespace rx
