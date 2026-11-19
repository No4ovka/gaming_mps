/** @file screen.cpp @brief Буферизованный ANSI-экран с UTF-8. */
#include "screen.h"
#include <algorithm>
#include <cstdio>

/** @brief Добавляет один Unicode-символ к строке UTF-8. */
static void utf8(std::string& text, char32_t glyph) {
    if (glyph < 0x80) text += static_cast<char>(glyph);
    else if (glyph < 0x800) {
        text += static_cast<char>(0xc0 | (glyph >> 6));
        text += static_cast<char>(0x80 | (glyph & 63));
    } else if (glyph < 0x10000) {
        text += static_cast<char>(0xe0 | (glyph >> 12));
        text += static_cast<char>(0x80 | ((glyph >> 6) & 63));
        text += static_cast<char>(0x80 | (glyph & 63));
    } else {
        text += '?'; // The fixed-cell UI deliberately excludes emoji and wide glyphs.
    }
}
/** @brief Читает один символ доверенного UTF-8-текста интерфейса. */
static char32_t readUtf8(std::string_view text, std::size_t& index) {
    auto first = static_cast<unsigned char>(text[index++]);
    if (first < 128) return first;
    unsigned count = (first & 0xe0) == 0xc0 ? 1 : (first & 0xf0) == 0xe0 ? 2 : 3;
    char32_t value = first & (count == 1 ? 31 : count == 2 ? 15 : 7);
    while (count--) {
        if (index >= text.size() || (static_cast<unsigned char>(text[index]) & 0xc0) != 0x80) return U'?';
        value = (value << 6) | (static_cast<unsigned char>(text[index++]) & 63);
    }
    return value;
}
/** @brief Выводит байты в stdout в сборке для ПК. */
static void writeStdout(const char* bytes, std::size_t size, void*) {
    std::fwrite(bytes, 1, size, stdout);
    std::fflush(stdout);
}
Screen::Screen(Output output, void* context)
    : output_(output ? output : writeStdout), context_(context) {}
void Screen::begin() { next_.fill(Pixel{}); }
void Screen::put(int x, int y, char32_t glyph, Color foreground, Color background) {
    if (x >= 0 && x < 80 && y >= 0 && y < 30 && !(x == 79 && y == 29))
        next_[y * 80 + x] = Pixel{glyph >= 32 ? glyph : U' ', foreground, background};
}
void Screen::clippedText(int x, int y, std::string_view value, Color foreground, Color background,
                         int left, int top, int w, int h) {
    if (y < top || y >= top + h) return;
    for (std::size_t i = 0; i < value.size() && x < left + w; ++x) {
        const char32_t glyph = readUtf8(value, i);
        if (x >= left) put(x, y, glyph, foreground, background);
    }
}
void Screen::canvasText(int x, int y, std::string_view value, Color foreground, Color background) {
    clippedText(x, y, value, foreground, background, 0, 0, 80, 30);
}
void Screen::text(int x, int y, std::string_view value, Color foreground, Color background) {
    clippedText(x + 2, y + 5, value, foreground, background, 2, 5, width, height);
}
void Screen::number(int x, int y, std::int64_t value, Color foreground, Color background) {
    text(x, y, std::to_string(value), foreground, background);
}
void Screen::canvasFill(int x, int y, int w, int h, Color background) {
    for (int row = std::max(0, y); row < std::min(30, y + h); ++row)
        for (int col = std::max(0, x); col < std::min(80, x + w); ++col)
            put(col, row, U' ', Theme::text, background);
}
void Screen::fill(int x, int y, int w, int h, Color background) {
    for (int row = std::max(0, y); row < std::min(height, y + h); ++row)
        for (int col = std::max(0, x); col < std::min(width, x + w); ++col)
            put(col + 2, row + 5, U' ', Theme::text, background);
}
void Screen::cell(int x, int y, Color foreground, Color background, char32_t glyph) {
    if (y < 0 || y >= height) return;
    if (x >= 0 && x < width) put(x + 2, y + 5, glyph, foreground, background);
    if (x + 1 >= 0 && x + 1 < width) put(x + 3, y + 5, U' ', foreground, background);
}
void Screen::canvasFrame(int x, int y, int w, int h, Color foreground) {
    if (w < 2 || h < 2) return;
    for (int i = 1; i < w - 1; ++i) {
        put(x + i, y, U'═', foreground, Theme::background);
        put(x + i, y + h - 1, U'═', foreground, Theme::background);
    }
    for (int i = 1; i < h - 1; ++i) {
        put(x, y + i, U'║', foreground, Theme::background);
        put(x + w - 1, y + i, U'║', foreground, Theme::background);
    }
    put(x, y, U'╔', foreground, Theme::background);
    put(x + w - 1, y, U'╗', foreground, Theme::background);
    put(x, y + h - 1, U'╚', foreground, Theme::background);
    put(x + w - 1, y + h - 1, U'╝', foreground, Theme::background);
}
void Screen::frame(int x, int y, int w, int h, Color foreground) {
    if (w < 2 || h < 2) return;
    for (int row = 0; row < h; ++row)
        for (int col = 0; col < w; ++col) {
            if (row && row != h - 1 && col && col != w - 1) continue;
            const int px = x + col, py = y + row;
            if (px < 0 || px >= width || py < 0 || py >= height) continue;
            const bool top = row == 0, bottom = row == h - 1;
            const char32_t glyph = col == 0 ? (top ? U'┌' : bottom ? U'└' : U'│')
                : col == w - 1 ? (top ? U'┐' : bottom ? U'┘' : U'│') : U'─';
            put(px + 2, py + 5, glyph, foreground, Theme::background);
        }
}
void Screen::panelText(int row, std::string_view value, Color foreground) {
    clippedText(57, row + 5, value, foreground, Theme::background, 57, 5, panelWidth, panelHeight);
}
void Screen::panelNumber(int row, std::string_view label, std::int64_t value, Color foreground) {
    panelText(row, std::string(label) + " " + std::to_string(value), foreground);
}
void Screen::flush() {
    std::string output;
    if (!initialized_) output = "\x1b[?1049h\x1b[?25l\x1b[?1003h\x1b[?1006h\x1b[2J";
    for (int y = 0; y < 30; ++y) {
        int x = 0;
        while (x < 80) {
            if ((x == 79 && y == 29) || (initialized_ && next_[y * 80 + x] == shown_[y * 80 + x])) { ++x; continue; }
            const Pixel first = next_[y * 80 + x];
            char control[64];
            std::snprintf(control, sizeof(control), "\x1b[%d;%dH\x1b[38;5;%u;48;5;%um", y + 1, x + 1,
                          static_cast<unsigned>(first.foreground), static_cast<unsigned>(first.background));
            output += control;
            do {
                utf8(output, next_[y * 80 + x].glyph);
                shown_[y * 80 + x] = next_[y * 80 + x];
                ++x;
            } while (x < 80 && !(x == 79 && y == 29)
                     && next_[y * 80 + x].foreground == first.foreground
                     && next_[y * 80 + x].background == first.background
                     && (!initialized_ || !(next_[y * 80 + x] == shown_[y * 80 + x])));
        }
    }
    initialized_ = true;
    if (!output.empty()) {
        output += "\x1b[0m";
        output_(output.data(), output.size(), context_);
    }
}
std::string Screen::plain() const {
    std::string result;
    for (int y = 0; y < 30; ++y) {
        for (int x = 0; x < 80; ++x) utf8(result, next_[y * 80 + x].glyph);
        result += '\n';
    }
    return result;
}
