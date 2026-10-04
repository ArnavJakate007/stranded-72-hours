#pragma once
#include "core/Palette.h"
#include "gfx/PixelFont.h"
#include "raylib.h"
#include <functional>
#include <string>
#include <vector>

// Clickable rectangles registered while drawing. The game checks them on the next frame,
// so every on-screen button also works with the mouse (keyboard remains the main input).
class HitList {
public:
    struct Hit {
        Rectangle rect;
        std::function<void()> onClick;
        std::function<void()> onHover;
    };
    void beginFrame() { last_.swap(current_); current_.clear(); }
    void add(Rectangle r, std::function<void()> onClick, std::function<void()> onHover = nullptr) {
        current_.push_back({r, std::move(onClick), std::move(onHover)});
    }
    const Hit* topAt(Vector2 p) const;   // searches last frame's list, topmost first

private:
    std::vector<Hit> current_, last_;
};

// The shared pixel-art UI kit: panels, chips, focus brackets, buttons and text.
// One consistent style everywhere (chamfered panels, 1 px borders, amber focus brackets).
class Ui {
public:
    static void init(const PixelFont* font);
    static void shutdown();

    static void rect(float x, float y, float w, float h, Color c);
    static void chamfer(float x, float y, float w, float h, Color c);
    static void panel(float x, float y, float w, float h, Color line = pal::PanelLine, Color fill = pal::PanelFill, Color hi = pal::PanelHi);
    static void card(float x, float y, float w, float h, Color accent = pal::PanelLine);   // light translucent HUD card
    static void thinBar(float x, float y, float w, float value, float ghost, Color fill, bool flash);
    static void brackets(float x, float y, float w, float h, float t, Color c = pal::Amber, int len = 6, int th = 2, bool pulse = true);
    static void disc(int cx, int cy, int r, Color c);
    static void ring(int cx, int cy, int r, Color c, int th = 1);
    static void glow(float x, float y, float r, Color c, float alpha);   // additive soft light
    static void radial(float x, float y, float r, Color c, float alpha);  // same, without changing blend mode

    static void text(const std::string& s, float x, float y, Color c = pal::Ink, int scale = 1, bool shadow = true);
    static void textCentered(const std::string& s, float cx, float y, Color c = pal::Ink, int scale = 1);
    static void textRight(const std::string& s, float rx, float y, Color c = pal::Ink, int scale = 1);
    static int width(const std::string& s, int scale = 1);
    static std::vector<std::string> wrap(const std::string& s, int maxWidth);

    static int chip(float x, float y, const std::string& s, Color c, Color border = pal::rgb(0x22314F));
    static int chip2(float x, float y, const std::string& a, const std::string& b, Color bc);
    static int keycap(float x, float y, const std::string& s);
    static void button(float x, float y, float w, float h, const std::string& label, bool focused, float t);
    static void segmentBar(float x, float y, int segments, float value, float ghost, Color fill, bool flash);
    static void downTriangle(float x, float y, Color c);

    static Texture2D gradient() { return gradient_; }
    static const PixelFont& font() { return *font_; }

private:
    static const PixelFont* font_;
    static Texture2D gradient_;
};
