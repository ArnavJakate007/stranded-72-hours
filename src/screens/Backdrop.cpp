#include "screens/Backdrop.h"
#include "core/Palette.h"
#include "gfx/Ui.h"
#include "logic/Rng.h"
#include "rlgl.h"
#include <cmath>
#include <vector>

namespace {
struct Star { int x, y; Color c; };
const std::vector<Star>& starField() {
    static std::vector<Star> s = [] {
        std::vector<Star> v;
        Rng r(77);
        for (int i = 0; i < 160; ++i) {
            float k = r.next();
            v.push_back({static_cast<int>(r.next() * 640), static_cast<int>(r.next() * 360), k < 0.2f ? WHITE : k < 0.5f ? pal::rgb(0x8FA0C0) : pal::rgb(0x4A5878)});
        }
        return v;
    }();
    return s;
}
}  // namespace

void Backdrop::stars(float t) {
    for (const Star& s : starField()) {
        if (s.c.r == 255 && static_cast<int>(t * 2 + s.x) % 7 == 0) continue;   // gentle twinkle
        DrawPixel(s.x, s.y, s.c);
    }
}

void Backdrop::gasGiant(int cx, int cy, int r, float t) {
    const Color bands[7] = {pal::rgb(0x3A1F5C), pal::rgb(0x4B2A74), pal::rgb(0x5E3590), pal::rgb(0x6F45A3), pal::rgb(0x5A2F86), pal::rgb(0x7D55B4), pal::rgb(0x4A2870)};
    for (int y = -r; y <= r; ++y) {
        int w = static_cast<int>(std::lround(std::sqrt(float(r * r - y * y))));
        int b = static_cast<int>(std::floor((y + r) / (r / 5.0f) + std::sin(y * 0.15f + t * 0.2f) * 0.6f));
        DrawRectangle(cx - w, cy + y, w * 2 + 1, 1, bands[((b % 7) + 7) % 7]);
        DrawRectangle(cx + static_cast<int>(std::lround(w * 0.35f)), cy + y, static_cast<int>(std::lround(w * 0.65f)) + 1, 1, Color{5, 8, 20, 115});
    }
    Ui::ring(cx, cy, r, pal::rgb(0x9A7AD0));
}

void Backdrop::station(int x, int y, float t) {
    DrawRectangle(x - 10, y - 1, 20, 3, pal::rgb(0x9AA5B6));
    DrawRectangle(x - 1, y - 8, 3, 17, pal::rgb(0x9AA5B6));
    DrawRectangle(x - 14, y - 3, 4, 7, pal::rgb(0x4A8FE0));
    DrawRectangle(x + 11, y - 3, 4, 7, pal::rgb(0x4A8FE0));
    if (static_cast<int>(t * 2) % 2) DrawPixel(x, y, pal::rgb(0xFF5A5A));
}

void Backdrop::lander(float x, float y, float angle, bool fire, float t) {
    rlPushMatrix();
    rlTranslatef(std::round(x), std::round(y), 0);
    rlRotatef(angle * 57.2958f, 0, 0, 1);
    DrawRectangle(-12, -6, 24, 12, pal::rgb(0xCFD6E2));
    DrawRectangle(-12, -6, 24, 2, pal::rgb(0xEEF2F8));
    DrawRectangle(-2, -6, 4, 12, pal::rgb(0xB3333D));
    DrawRectangle(6, -3, 4, 4, pal::rgb(0x24324C));
    DrawRectangle(-16, -4, 4, 8, pal::rgb(0x3B4252));
    if (fire) {
        int f = static_cast<int>(t * 20) % 2;
        DrawRectangle(-22 - f * 2, -2, 6 + f * 2, 4, pal::rgb(0xFFB040));
        DrawRectangle(-19, -1, 3, 2, pal::rgb(0xFFF0C0));
    }
    rlPopMatrix();
}

void Backdrop::moonHorizon() {
    Ui::disc(320, 920, 640, pal::rgb(0x5A6D92));
    Ui::disc(320, 924, 636, pal::rgb(0x8FA3C8));
    Rng r(5);
    for (int i = 0; i < 40; ++i) DrawRectangle(static_cast<int>(r.next() * 640), 300 + static_cast<int>(r.next() * 60), 4 + static_cast<int>(r.next() * 10), 1, pal::rgb(0xA9BBDC));
}

void Backdrop::parallaxStars(float t, float speedX, float speedY) {
    const auto& field = starField();
    for (size_t i = 0; i < field.size(); ++i) {
        const Star& s = field[i];
        float depth = 0.3f + (i % 3) * 0.35f;   // near stars move faster
        float x = std::fmod(s.x + t * speedX * depth, 640.0f), y = std::fmod(s.y + t * speedY * depth, 360.0f);
        if (x < 0) x += 640;
        if (y < 0) y += 360;
        DrawPixel(static_cast<int>(x), static_cast<int>(y), s.c);
        if (depth > 0.9f && (std::fabs(speedX) + std::fabs(speedY)) > 60)
            DrawLine(static_cast<int>(x), static_cast<int>(y), static_cast<int>(x - speedX * 0.04f), static_cast<int>(y - speedY * 0.04f), withAlpha(s.c, 0.5f));
    }
}

void Backdrop::bigStation(float cx, float cy, float t) {
    // solar wings
    for (int side : {-1, 1}) {
        float wx = cx + side * 46.0f;
        DrawRectangle(static_cast<int>(wx - 18), static_cast<int>(cy - 10), 36, 20, pal::rgb(0x1E3E78));
        for (int i = 0; i < 6; ++i) DrawRectangle(static_cast<int>(wx - 18 + i * 6), static_cast<int>(cy - 10), 1, 20, pal::rgb(0x3D6CC0));
        DrawRectangle(static_cast<int>(wx - 18), static_cast<int>(cy - 1), 36, 1, pal::rgb(0x3D6CC0));
    }
    DrawRectangle(static_cast<int>(cx - 28), static_cast<int>(cy - 2), 56, 4, pal::rgb(0x8F99AB));
    // spinning habitat ring, drawn as an ellipse of dots with moving windows
    for (int i = 0; i < 64; ++i) {
        float a = i / 64.0f * 6.2831853f;
        float x = cx + std::cos(a) * 30, y = cy + std::sin(a) * 12;
        bool window = (i + static_cast<int>(t * 8)) % 8 == 0;
        DrawRectangle(static_cast<int>(x) - 1, static_cast<int>(y) - 1, 3, 3, window ? pal::rgb(0xFFD890) : pal::rgb(0xAEB7C6));
    }
    DrawRectangle(static_cast<int>(cx - 6), static_cast<int>(cy - 14), 12, 28, pal::rgb(0xCFD6E2));
    DrawRectangle(static_cast<int>(cx - 6), static_cast<int>(cy - 14), 12, 2, pal::rgb(0xEEF2F8));
    DrawRectangle(static_cast<int>(cx - 2), static_cast<int>(cy - 8), 4, 3, pal::rgb(0x5EC8FF));
    DrawRectangle(static_cast<int>(cx - 2), static_cast<int>(cy + 2), 4, 3, pal::rgb(0x5EC8FF));
    DrawRectangle(static_cast<int>(cx), static_cast<int>(cy - 22), 1, 8, pal::rgb(0x8F99AB));
    if (static_cast<int>(t * 2) % 2) DrawRectangle(static_cast<int>(cx - 1), static_cast<int>(cy - 24), 3, 3, pal::rgb(0xFF5A5A));
    Ui::glow(cx, cy, 40, pal::rgb(0x8FB4FF), 0.12f);
}

void Backdrop::shuttle(float x, float y, float flame, float t) {
    int ix = static_cast<int>(std::round(x)), iy = static_cast<int>(std::round(y));
    if (flame > 0) {   // engine plume under the shuttle
        int f = static_cast<int>(t * 24) % 3;
        int len = static_cast<int>(10 + flame * 26) + f * 2;
        Ui::glow(static_cast<float>(ix), static_cast<float>(iy + 16 + len / 2), 18 + flame * 16, pal::rgb(0xFF9A40), 0.5f);
        DrawRectangle(ix - 6, iy + 16, 4, len, pal::rgb(0xFFB040));
        DrawRectangle(ix + 2, iy + 16, 4, len, pal::rgb(0xFFB040));
        DrawRectangle(ix - 5, iy + 16, 2, len * 2 / 3, pal::rgb(0xFFF0C0));
        DrawRectangle(ix + 3, iy + 16, 2, len * 2 / 3, pal::rgb(0xFFF0C0));
    }
    DrawRectangle(ix - 20, iy + 4, 8, 12, pal::rgb(0x8F99AB));     // fins
    DrawRectangle(ix + 12, iy + 4, 8, 12, pal::rgb(0x8F99AB));
    DrawRectangle(ix - 12, iy - 20, 24, 36, pal::rgb(0xDDE3EC));   // body
    DrawRectangle(ix - 12, iy - 20, 3, 36, pal::rgb(0xF4F7FB));
    DrawRectangle(ix + 9, iy - 20, 3, 36, pal::rgb(0xAEB7C6));
    DrawRectangle(ix - 8, iy - 28, 16, 8, pal::rgb(0xDDE3EC));     // nose
    DrawRectangle(ix - 4, iy - 32, 8, 4, pal::rgb(0xCFD6E2));
    DrawRectangle(ix - 6, iy - 16, 12, 5, pal::rgb(0x24324C));     // cockpit
    DrawRectangle(ix - 5, iy - 15, 4, 1, pal::rgb(0x8FC4F0));
    DrawRectangle(ix - 12, iy - 4, 24, 3, pal::rgb(0xEF9F27));     // HALCYON stripe
    DrawRectangle(ix - 8, iy + 16, 16, 3, pal::rgb(0x3B4252));     // engines
    DrawRectangle(ix - 16, iy + 16, 2, 6, pal::rgb(0x59606E));     // legs
    DrawRectangle(ix + 14, iy + 16, 2, 6, pal::rgb(0x59606E));
    if (static_cast<int>(t * 2) % 2) {
        DrawRectangle(ix - 21, iy + 4, 2, 2, pal::rgb(0xFF5A5A));
        DrawRectangle(ix + 19, iy + 4, 2, 2, pal::rgb(0x5EFF8A));
    }
}

void Backdrop::lightning(float x, float y0, float y1, unsigned seed, Color c) {
    Rng r(seed);
    float px = x, py = y0;
    while (py < y1) {
        float nx = px + (r.next() - 0.5f) * 26, ny = py + 8 + r.next() * 14;
        DrawLineEx(Vector2{px, py}, Vector2{nx, ny}, 2, c);
        if (r.next() < 0.2f) DrawLine(static_cast<int>(nx), static_cast<int>(ny), static_cast<int>(nx + (r.next() - 0.5f) * 40), static_cast<int>(ny + 20), withAlpha(c, 0.6f));
        px = nx;
        py = ny;
    }
    Ui::glow(x, y0 + (y1 - y0) * 0.3f, 60, c, 0.3f);
}

void Backdrop::letterbox(float amount) {
    int h = static_cast<int>(std::round(30 * amount));
    if (h <= 0) return;
    DrawRectangle(0, 0, 640, h, BLACK);
    DrawRectangle(0, 360 - h, 640, h, BLACK);
}
