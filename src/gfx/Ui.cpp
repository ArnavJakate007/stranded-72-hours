#include "gfx/Ui.h"
#include <algorithm>
#include <cmath>

const PixelFont* Ui::font_ = nullptr;
Texture2D Ui::gradient_{};

const HitList::Hit* HitList::topAt(Vector2 p) const {
    for (auto it = last_.rbegin(); it != last_.rend(); ++it)
        if (CheckCollisionPointRec(p, it->rect)) return &*it;
    return nullptr;
}

void Ui::init(const PixelFont* font) {
    font_ = font;
    Image img = GenImageGradientRadial(64, 64, 0.0f, WHITE, Color{255, 255, 255, 0});
    gradient_ = LoadTextureFromImage(img);
    SetTextureFilter(gradient_, TEXTURE_FILTER_BILINEAR);
    UnloadImage(img);
}

void Ui::shutdown() {
    if (gradient_.id) UnloadTexture(gradient_);
    gradient_ = Texture2D{};
}

void Ui::rect(float x, float y, float w, float h, Color c) {
    DrawRectangle(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), static_cast<int>(w), static_cast<int>(h), c);
}

void Ui::chamfer(float x, float y, float w, float h, Color c) {
    rect(x + 2, y, w - 4, h, c);
    rect(x + 1, y + 1, w - 2, h - 2, c);
    rect(x, y + 2, w, h - 4, c);
}

void Ui::panel(float x, float y, float w, float h, Color line, Color fill, Color hi) {
    // soft drop shadow, so panels float above the world
    rect(x + 2, y + h, w - 2, 2, Color{0, 0, 0, 80});
    rect(x + w, y + 2, 2, h - 1, Color{0, 0, 0, 80});
    chamfer(x, y, w, h, pal::PanelEdge);
    chamfer(x + 1, y + 1, w - 2, h - 2, line);
    chamfer(x + 2, y + 2, w - 4, h - 4, fill);
    // glassy top: a short gradient from a lighter tint of the fill down into it
    int gh = static_cast<int>(std::min(12.0f, h - 6));
    if (gh > 2) DrawRectangleGradientV(static_cast<int>(x + 3), static_cast<int>(y + 3), static_cast<int>(w - 6), gh, shade(fill, 1.45f), fill);
    rect(x + 4, y + 2, w - 8, 1, hi);
    rect(x + 3, y + h - 3, w - 6, 1, Color{0, 0, 0, 90});
    // small corner accents in the line colour
    Color acc = shade(line, 1.5f);
    rect(x + 3, y + 3, 4, 1, acc); rect(x + 3, y + 3, 1, 3, acc);
    rect(x + w - 7, y + 3, 4, 1, acc); rect(x + w - 4, y + 3, 1, 3, acc);
}

void Ui::brackets(float x, float y, float w, float h, float t, Color c, int len, int th, bool pulse) {
    int p = pulse && std::sin(t * 6.0f) > 0 ? 1 : 0;
    float o = static_cast<float>(th + 1 + p);
    x -= o; y -= o; w += 2 * o; h += 2 * o;
    rect(x, y, len, th, c); rect(x, y, th, len, c);
    rect(x + w - len, y, len, th, c); rect(x + w - th, y, th, len, c);
    rect(x, y + h - th, len, th, c); rect(x, y + h - len, th, len, c);
    rect(x + w - len, y + h - th, len, th, c); rect(x + w - th, y + h - len, th, len, c);
}

void Ui::disc(int cx, int cy, int r, Color c) {
    for (int y = -r; y <= r; ++y) {
        int w = static_cast<int>(std::lround(std::sqrt(float(r * r - y * y))));
        DrawRectangle(cx - w, cy + y, w * 2 + 1, 1, c);
    }
}

void Ui::ring(int cx, int cy, int r, Color c, int th) {
    for (float a = 0; a < 6.2832f; a += 0.03f)
        DrawRectangle(static_cast<int>(std::lround(cx + std::cos(a) * r)), static_cast<int>(std::lround(cy + std::sin(a) * r)), th, th, c);
}

void Ui::radial(float x, float y, float r, Color c, float alpha) {
    if (alpha <= 0.0f || r <= 0.0f) return;
    DrawTexturePro(gradient_, Rectangle{0, 0, 64, 64}, Rectangle{x - r, y - r, r * 2, r * 2}, Vector2{0, 0}, 0, withAlpha(c, alpha));
}

void Ui::glow(float x, float y, float r, Color c, float alpha) {
    BeginBlendMode(BLEND_ADDITIVE);
    radial(x, y, r, c, alpha);
    EndBlendMode();
}

void Ui::text(const std::string& s, float x, float y, Color c, int scale, bool shadow) { font_->draw(s, x, y, c, scale, shadow); }
void Ui::textCentered(const std::string& s, float cx, float y, Color c, int scale) { text(s, std::round(cx - width(s, scale) / 2.0f), y, c, scale); }
void Ui::textRight(const std::string& s, float rx, float y, Color c, int scale) { text(s, rx - width(s, scale), y, c, scale); }
int Ui::width(const std::string& s, int scale) { return font_->width(s, scale); }

std::vector<std::string> Ui::wrap(const std::string& s, int maxWidth) {
    std::vector<std::string> out;
    std::string cur, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string t = cur.empty() ? word : cur + " " + word;
        if (width(t) > maxWidth && !cur.empty()) {
            out.push_back(cur);
            cur = word;
        } else {
            cur = t;
        }
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flushWord();
        else word += ch;
    }
    flushWord();
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int Ui::chip(float x, float y, const std::string& s, Color c, Color border) {
    int w = width(s) + 10;
    rect(x + 1, y + 11, w - 1, 1, Color{0, 0, 0, 90});
    rect(x, y, w, 11, pal::PanelEdge);
    DrawRectangleGradientV(static_cast<int>(x + 1), static_cast<int>(y + 1), w - 2, 9, pal::rgb(0x16223A), pal::rgb(0x0A1120));
    rect(x + 1, y + 1, w - 2, 1, border);
    text(s, x + 5, y + 2, c);   // 5 px padding each side
    return w;
}

int Ui::chip2(float x, float y, const std::string& a, const std::string& b, Color bc) {
    int w = std::max(width(a), width(b)) + 10;
    rect(x + 1, y + 22, w - 1, 1, Color{0, 0, 0, 90});
    rect(x, y, w, 22, pal::PanelEdge);
    DrawRectangleGradientV(static_cast<int>(x + 1), static_cast<int>(y + 1), w - 2, 20, pal::rgb(0x16223A), pal::rgb(0x0A1120));
    rect(x + 1, y + 1, w - 2, 1, pal::rgb(0x2A3C60));
    text(a, x + 5, y + 3, pal::Ink);
    text(b, x + 5, y + 12, bc);
    return w;
}

int Ui::keycap(float x, float y, const std::string& s) {
    int w = width(s) + 8;
    rect(x, y, w, 12, pal::PanelEdge);
    rect(x + 1, y + 1, w - 2, 10, pal::rgb(0x1C2840));
    rect(x + 1, y + 1, w - 2, 1, pal::rgb(0x3D527A));
    rect(x + 1, y + 10, w - 2, 1, pal::rgb(0x0A0F19));
    text(s, x + 4, y + 2, pal::Ink, 1, false);
    return w;
}

void Ui::button(float x, float y, float w, float h, const std::string& label, bool focused, float t) {
    rect(x + 1, y + h, w - 1, 1, Color{0, 0, 0, 90});
    rect(x, y, w, h, pal::PanelEdge);
    Color top = focused ? pal::rgb(0x2A4270) : pal::rgb(0x18243A), bottom = focused ? pal::Select : pal::rgb(0x0E1626);
    DrawRectangleGradientV(static_cast<int>(x + 1), static_cast<int>(y + 1), static_cast<int>(w - 2), static_cast<int>(h - 2), top, bottom);
    rect(x + 1, y + 1, w - 2, 1, focused ? pal::Amber : pal::rgb(0x26324A));
    textCentered(label, x + w / 2, y + std::floor((h - 7) / 2), focused ? pal::Ink : pal::Dim);
    if (focused) brackets(x, y, w, h, t);
}

void Ui::segmentBar(float x, float y, int segments, float value, float ghost, Color fill, bool flash) {
    rect(x - 1, y - 1, segments * 5 + 1, 8, pal::rgb(0x05080F));
    int filled = static_cast<int>(std::lround(value / 100.0f * segments));
    int gh = static_cast<int>(std::lround(ghost / 100.0f * segments));
    for (int s = 0; s < segments; ++s) {
        Color c = s < filled ? (flash ? pal::rgb(0xFFD4D0) : fill) : s < gh ? pal::rgb(0xF2A093) : pal::SegEmpty;
        rect(x + s * 5, y, 4, 6, c);
        if (s < filled) {
            rect(x + s * 5, y, 4, 1, Color{255, 255, 255, 90});
            rect(x + s * 5, y + 5, 4, 1, Color{0, 0, 0, 70});
        }
    }
}

void Ui::downTriangle(float x, float y, Color c) {
    rect(x, y, 7, 1, c);
    rect(x + 1, y + 1, 5, 1, c);
    rect(x + 2, y + 2, 3, 1, c);
    rect(x + 3, y + 3, 1, 1, c);
}

// A quiet HUD card: translucent fill so the world shows through, a hairline border and a soft top line.
void Ui::card(float x, float y, float w, float h, Color accent) {
    rect(x + 1, y + h, w - 1, 1, Color{0, 0, 0, 70});
    rect(x, y, w, h, Color{6, 10, 20, 205});
    rect(x, y, w, 1, withAlpha(shade(accent, 1.3f), 0.9f));
    rect(x, y + h - 1, w, 1, Color{20, 30, 50, 200});
    rect(x, y + 1, 1, h - 2, Color{20, 30, 50, 200});
    rect(x + w - 1, y + 1, 1, h - 2, Color{20, 30, 50, 200});
}

// A slim 3 px bar with a fading "ghost" of the recent loss.
void Ui::thinBar(float x, float y, float w, float value, float ghost, Color fill, bool flash) {
    rect(x, y, w, 3, pal::rgb(0x141D30));
    float g = std::round(w * std::clamp(ghost, 0.0f, 100.0f) / 100.0f), v = std::round(w * std::clamp(value, 0.0f, 100.0f) / 100.0f);
    if (g > v) rect(x + v, y, g - v, 3, pal::rgb(0x9A4A44));
    rect(x, y, v, 3, flash ? pal::rgb(0xFFD4D0) : fill);
    if (v > 0) rect(x, y, v, 1, shade(flash ? pal::rgb(0xFFD4D0) : fill, 1.35f));
}
