#include "core/NameEntry.h"
#include "data/ScoreStore.h"
#include <cctype>

namespace rx {

const char* NameEntry::charset() {
    // 4 rows of 10.  Letters first in reading order, then the digits, then the
    // two controls in the bottom-right corner where a thumb naturally lands.
    return "ABCDEFGHIJ"
           "KLMNOPQRST"
           "UVWXYZ0123"
           "456789-.<>";
}

void NameEntry::begin(const std::string& current) {
    text_   = ScoreStore::sanitizeName(current);
    // The stored default is a placeholder, not something anybody chose, so it
    // starts cleared rather than needing eight rubs to get rid of.
    if (text_ == ScoreRules::DEFAULT_NAME) text_.clear();
    cursor_ = 0;
    done_   = false;
}

void NameEntry::setCursor(int index) {
    const int total = COLS * ROWS;
    if (index < 0 || index >= total) return;
    cursor_ = index;
}

void NameEntry::move(Direction d) {
    int col = cursor_ % COLS;
    int row = cursor_ / COLS;
    switch (d) {
        case Direction::Left:  col = (col + COLS - 1) % COLS; break;
        case Direction::Right: col = (col + 1) % COLS;        break;
        case Direction::Up:    row = (row + ROWS - 1) % ROWS; break;
        case Direction::Down:  row = (row + 1) % ROWS;        break;
        default: return;
    }
    cursor_ = row * COLS + col;
}

char NameEntry::charAt(int index) const {
    if (index < 0 || index >= COLS * ROWS) return ' ';
    return charset()[index];
}

void NameEntry::typeChar(char c) {
    const char up = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (int i = 0; i < COLS * ROWS; ++i) {
        if (charset()[i] != up) continue;
        if (up == '<') { backspace(); return; }
        if (up == '>') { finish();    return; }
        if (text_.size() < ScoreRules::MAX_NAME_LENGTH) text_.push_back(up);
        return;
    }
}

void NameEntry::backspace() {
    if (!text_.empty()) text_.pop_back();
}

void NameEntry::commit() {
    const char c = currentChar();
    if (c == '<') { backspace(); return; }
    if (c == '>') { finish();    return; }
    if (text_.size() < ScoreRules::MAX_NAME_LENGTH) text_.push_back(c);
}

std::string NameEntry::result() const {
    return ScoreStore::sanitizeName(text_);
}

} // namespace rx
