#pragma once
#include "raylib.h"
#include <string>

// The colour theme (SRS UI-5). Light means progress: amber = powered, teal = healthy, red = critical, grey = offline.
namespace pal {
constexpr Color rgb(unsigned hex, unsigned char a = 255) {
    return Color{static_cast<unsigned char>((hex >> 16) & 255), static_cast<unsigned char>((hex >> 8) & 255),
                 static_cast<unsigned char>(hex & 255), a};
}
constexpr Color Ink = rgb(0xE6EDF7);
constexpr Color Dim = rgb(0x8B98B2);
constexpr Color Dim2 = rgb(0x5D6A85);
constexpr Color Soft = rgb(0xC3CDE0);
constexpr Color Amber = rgb(0xEF9F27);
constexpr Color AmberDark = rgb(0x9A5E10);
constexpr Color AmberLight = rgb(0xFFC76A);
constexpr Color Teal = rgb(0x1D9E75);
constexpr Color TealHi = rgb(0x2FC596);
constexpr Color Red = rgb(0xE24B4A);
constexpr Color RedSoft = rgb(0xE07A6E);
constexpr Color Offline = rgb(0x5E6878);
constexpr Color Ice = rgb(0x8FD3FF);
constexpr Color Blue = rgb(0x4AA8E8);
constexpr Color Void = rgb(0x05070D);
constexpr Color Bg = rgb(0x0B0F17);
constexpr Color PanelEdge = rgb(0x04070D);
constexpr Color PanelLine = rgb(0x2C3E60);
constexpr Color PanelLineHi = rgb(0x3A5280);
constexpr Color PanelFill = rgb(0x0F1828);
constexpr Color PanelFillDark = rgb(0x0B1220);
constexpr Color PanelHi = rgb(0x1D2C49);
constexpr Color SegEmpty = rgb(0x18233A);
constexpr Color Select = rgb(0x1C2C4A);
constexpr Color RowHi = rgb(0x16233A);
constexpr Color Maya = rgb(0x22B48C);
constexpr Color Arjun = rgb(0xEF9F27);
constexpr Color Elena = rgb(0xFF6361);
constexpr Color Hana = rgb(0x52A8F2);
constexpr Color Voss = rgb(0xA3ADBF);
}  // namespace pal

inline Color speakerColor(const std::string& who) {
    if (who == "maya") return pal::Maya;
    if (who == "arjun") return pal::Arjun;
    if (who == "elena") return pal::Elena;
    if (who == "hana") return pal::Hana;
    if (who == "voss") return pal::Voss;
    return pal::Ink;
}
inline Color shade(Color c, float f) {
    auto ch = [f](unsigned char v) { float r = v * f; return static_cast<unsigned char>(r > 255 ? 255 : r); };
    return Color{ch(c.r), ch(c.g), ch(c.b), c.a};
}
inline Color withAlpha(Color c, float a) { return Color{c.r, c.g, c.b, static_cast<unsigned char>(a < 0 ? 0 : a > 1 ? 255 : a * 255)}; }
inline Color lerpColor(Color a, Color b, float k) {
    k = k < 0 ? 0 : k > 1 ? 1 : k;
    auto m = [k](unsigned char x, unsigned char y) { return static_cast<unsigned char>(x + (y - x) * k); };
    return Color{m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}
