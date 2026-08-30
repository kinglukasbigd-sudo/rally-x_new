#include "rendering/Renderer.h"
#include "rendering/Font.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace rx {

bool Renderer::init(const char* title, int scale, bool fullscreen) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");   // nearest neighbour

    // Both ways up.  The playfield is 288x224, so landscape fills more of the
    // screen, but portrait is perfectly playable -- the picture sits as a band
    // across the middle with the controls below it.  On Android this hint is
    // what actually sets the activity's orientation, so it must be set before
    // the window is created.
    SDL_SetHint(SDL_HINT_ORIENTATIONS,
                "LandscapeLeft LandscapeRight Portrait PortraitUpsideDown");

    // SDL synthesises mouse events from touches by default, so on a phone one
    // tap arrives twice: once as a finger and once as a mouse click.  That
    // double-counts every gesture.  Desktop mice are unaffected -- they are
    // real mouse events, not synthesised ones.
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");

    Uint32 flags = SDL_WINDOW_RESIZABLE;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    win_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            SCREEN_W * scale, SCREEN_H * scale, flags);
    if (!win_) { std::fprintf(stderr, "CreateWindow: %s\n", SDL_GetError()); return false; }

    ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren_) ren_ = SDL_CreateRenderer(win_, -1, SDL_RENDERER_SOFTWARE);
    if (!ren_) { std::fprintf(stderr, "CreateRenderer: %s\n", SDL_GetError()); return false; }

    SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);

    fb_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888,
                            SDL_TEXTUREACCESS_TARGET, SCREEN_W, SCREEN_H);
    if (!fb_) { std::fprintf(stderr, "CreateTexture: %s\n", SDL_GetError()); return false; }

    headless_ = std::strcmp(SDL_GetCurrentVideoDriver(), "dummy") == 0;
    return true;
}

void Renderer::shutdown() {
    if (fb_)  SDL_DestroyTexture(fb_);
    if (ren_) SDL_DestroyRenderer(ren_);
    if (win_) SDL_DestroyWindow(win_);
    fb_ = nullptr; ren_ = nullptr; win_ = nullptr;
    SDL_Quit();
}

void Renderer::beginFrame() {
    SDL_SetRenderTarget(ren_, fb_);
    SDL_RenderSetClipRect(ren_, nullptr);
}

void Renderer::beginPresent() {
    SDL_SetRenderTarget(ren_, nullptr);
    SDL_RenderSetClipRect(ren_, nullptr);
    SDL_GetRendererOutputSize(ren_, &winW_, &winH_);

    // Largest scale that still fits.  Whole-number steps keep the pixel grid
    // exact; filling gives up that exactness to reach the screen edge.  The
    // aspect ratio is held in both cases, so the art is never stretched.
    const float fitX = static_cast<float>(winW_) / SCREEN_W;
    const float fitY = static_cast<float>(winH_) / SCREEN_H;
    float scale = std::min(fitX, fitY);
    if (!fillScreen_) scale = std::max(1.f, std::floor(scale));
    if (scale < 1.f)  scale = 1.f;

    const int w = static_cast<int>(SCREEN_W * scale);
    const int h = static_cast<int>(SCREEN_H * scale);
    gameRect_ = Rect{ static_cast<float>((winW_ - w) / 2),
                      static_cast<float>((winH_ - h) / 2),
                      static_cast<float>(w), static_cast<float>(h) };

    SDL_SetRenderDrawColor(ren_, 0, 0, 0, 255);
    SDL_RenderClear(ren_);
    SDL_Rect dst{ static_cast<int>(gameRect_.x), static_cast<int>(gameRect_.y), w, h };
    SDL_RenderCopy(ren_, fb_, nullptr, &dst);
}

void Renderer::endPresent() {
    SDL_RenderPresent(ren_);
}

void Renderer::clear(Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_RenderClear(ren_);
}

void Renderer::fillRect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_Rect r{ x, y, w, h };
    SDL_RenderFillRect(ren_, &r);
}

void Renderer::drawRect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_Rect r{ x, y, w, h };
    SDL_RenderDrawRect(ren_, &r);
}

void Renderer::drawPixel(int x, int y, Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_RenderDrawPoint(ren_, x, y);
}

void Renderer::drawLine(int x0, int y0, int x1, int y1, Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(ren_, x0, y0, x1, y1);
}

void Renderer::setClip(int x, int y, int w, int h) {
    SDL_Rect r{ x, y, w, h };
    SDL_RenderSetClipRect(ren_, &r);
}

void Renderer::clearClip() { SDL_RenderSetClipRect(ren_, nullptr); }

Sprite Renderer::createSprite(int w, int h, const char* const* rows,
                              const Color* palette, int paletteSize) {
    Sprite s; s.w = w; s.h = h;
    std::vector<uint32_t> px(static_cast<size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y) {
        const char* row = rows[y];
        const int len = static_cast<int>(std::strlen(row));
        for (int x = 0; x < w; ++x) {
            const char ch = (x < len) ? row[x] : ' ';
            if (ch == ' ' || ch == '.') continue;          // transparent
            int idx = (ch >= '0' && ch <= '9') ? ch - '0' : -1;
            if (idx < 0 || idx >= paletteSize) continue;
            const Color& c = palette[idx];
            px[static_cast<size_t>(y) * w + x] =
                (static_cast<uint32_t>(c.r) << 24) | (static_cast<uint32_t>(c.g) << 16) |
                (static_cast<uint32_t>(c.b) << 8)  |  static_cast<uint32_t>(c.a);
        }
    }
    s.tex = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_STATIC, w, h);
    if (s.tex) {
        SDL_UpdateTexture(s.tex, nullptr, px.data(), w * 4);
        SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
    }
    return s;
}

void Renderer::destroySprite(Sprite& s) {
    if (s.tex) SDL_DestroyTexture(s.tex);
    s.tex = nullptr;
}

void Renderer::draw(const Sprite& s, int x, int y) {
    if (!s.valid()) return;
    SDL_Rect d{ x, y, s.w, s.h };
    SDL_RenderCopy(ren_, s.tex, nullptr, &d);
}

void Renderer::drawCentered(const Sprite& s, int cx, int cy) {
    draw(s, cx - s.w / 2, cy - s.h / 2);
}

void Renderer::drawRotated(const Sprite& s, int cx, int cy, int angleDeg) {
    if (!s.valid()) return;
    SDL_Rect d{ cx - s.w / 2, cy - s.h / 2, s.w, s.h };
    SDL_RenderCopyEx(ren_, s.tex, nullptr, &d, angleDeg, nullptr, SDL_FLIP_NONE);
}

bool Renderer::saveScreenshot(const std::string& path) {
    SDL_SetRenderTarget(ren_, fb_);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32,
                                                    SDL_PIXELFORMAT_RGBA8888);
    if (!s) return false;
    const bool ok = SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGBA8888,
                                         s->pixels, s->pitch) == 0
                 && SDL_SaveBMP(s, path.c_str()) == 0;
    SDL_FreeSurface(s);
    SDL_SetRenderTarget(ren_, nullptr);
    return ok;
}

void Renderer::text(int x, int y, const std::string& str, Color c) {
    SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    int cx = x;
    for (char raw : str) {
        char ch = raw;
        if (ch >= 'a' && ch <= 'z') ch = static_cast<char>(ch - 'a' + 'A');
        const Glyph* g = nullptr;
        for (int i = 0; i < FONT_GLYPH_COUNT; ++i)
            if (FONT_GLYPHS[i].c == ch) { g = &FONT_GLYPHS[i]; break; }
        if (g) {
            for (int gy = 0; gy < FONT_H; ++gy) {
                const char* row = g->rows[gy];
                for (int gx = 0; gx < FONT_W; ++gx)
                    if (row[gx] == '#') SDL_RenderDrawPoint(ren_, cx + gx, y + gy);
            }
        }
        cx += FONT_ADVANCE;
    }
}

void Renderer::textCentered(int cx, int y, const std::string& str, Color c) {
    text(cx - textWidth(str) / 2, y, str, c);
}

int Renderer::textWidth(const std::string& str) {
    if (str.empty()) return 0;
    return static_cast<int>(str.size()) * FONT_ADVANCE - 1;
}

} // namespace rx
