#pragma once
#include "core/Types.h"
#include <string>

namespace rx {

// The arcade name-entry grid.
//
// It is driven by the four directions and one confirm button, which is the
// only vocabulary every one of this game's control schemes already shares: the
// keyboard's arrows, the touch d-pad, and a swipe all produce a Direction, and
// all three have a confirm.  A keyboard can also just type, and a finger can
// tap a letter directly, but neither is required -- the grid alone is enough.
//
// No state of its own beyond the cursor and the name, so it is trivially
// testable without a window.
class NameEntry {
public:
    static constexpr int COLS = 10;
    static constexpr int ROWS = 4;

    // The grid, row by row.  '<' rubs out the last character and '>' finishes;
    // both are glyphs the font already draws.  There is no space: names are at
    // most eight characters, and '-' does the job of separating words.
    static const char* charset();     // COLS * ROWS characters, no terminator run

    void begin(const std::string& current);

    // Grid navigation.  The cursor wraps in both axes, so the far cells are
    // never more than a few presses away.
    void move(Direction d);
    int  cursor() const { return cursor_; }
    void setCursor(int index);
    char charAt(int index) const;
    char currentChar() const { return charAt(cursor_); }

    // Acts on the highlighted cell: a letter is appended, '<' rubs out, '>'
    // finishes.  This is what the confirm button and a tap both call.
    void commit();

    // Typed straight in from a keyboard.  Anything the grid does not contain
    // is ignored rather than stored.
    void typeChar(char c);
    void backspace();
    void finish() { done_ = true; }

    bool done() const { return done_; }

    // What has been entered so far.  Never longer than the store allows.
    const std::string& text() const { return text_; }

    // The name as it would actually be filed, which is what the screen shows
    // so there is no surprise between typing and saving.
    std::string result() const;

private:
    std::string text_;
    int         cursor_ = 0;
    bool        done_   = false;
};

} // namespace rx
