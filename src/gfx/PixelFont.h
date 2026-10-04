#pragma once
#include "raylib.h"
#include <map>
#include <string>
#include <vector>

// A crisp 5x7 bitmap font (8x12 grid with descenders), baked into one texture atlas at start.
// Drawn only at whole-number scales so it never blurs (SRS accessibility: readable text).
class PixelFont {
public:
    PixelFont() = default;
    ~PixelFont();
    PixelFont(const PixelFont&) = delete;
    PixelFont& operator=(const PixelFont&) = delete;

    void load();
    int width(const std::string& text, int scale = 1) const;
    void draw(const std::string& text, float x, float y, Color color, int scale = 1, bool shadow = true) const;
    static constexpr int LINE_HEIGHT = 11;

private:
    struct Glyph { Rectangle src; int w; };
    const Glyph* glyph(int codepoint) const;
    void drawRaw(const std::string& text, float x, float y, Color color, int scale) const;
    Texture2D atlas_{};
    std::map<int, Glyph> glyphs_;
};
