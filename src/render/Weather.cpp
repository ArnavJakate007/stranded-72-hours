#include "render/Weather.h"
#include "core/Palette.h"
#include "logic/Rng.h"
#include "logic/Session.h"
#include "render/WorldRenderer.h"
#include <algorithm>
#include <cmath>

namespace {
Rng& respawn() {
    static Rng r(99);
    return r;
}
}  // namespace

Weather::Weather() {
    Rng r(3);
    for (int i = 0; i < 260; ++i) flakes_.push_back({r.next() * 640, r.next() * 360, 8 + r.next() * 16, r.next() * 6, r.next() < 0.3f ? 2 : 1});
}

void Weather::update(float dt, float time) {
    stormMix_ = std::clamp(stormMix_ + (storm_ ? dt : -dt) * 0.5f, 0.0f, 1.0f);
    float fall = 1 + stormMix_ * 7, drift = stormMix_ * 160;
    for (Flake& f : flakes_) {
        f.y += f.speed * fall * dt;
        f.x += (std::sin(time * 0.8f + f.phase) * 5 + 4 + drift) * dt;
        if (f.y > 360) {
            f.y = -2;
            f.x = respawn().next() * 640;
        }
        if (f.x > 640) f.x -= 640;
    }
    strikeGlow_ = std::max(0.0f, strikeGlow_ - dt * 3);
    if (stormMix_ > 0.6f) {
        nextStrike_ -= dt;
        if (nextStrike_ <= 0) {
            struck_ = true;
            strikeGlow_ = 1;
            nextStrike_ = 3.5f + respawn().next() * 6;   // well under 3 flashes per second
        }
    }
}

void Weather::drawSnow(const Session& s, const Cam& cam) const {
    const World& w = s.world();
    Color c{226, 234, 248, 216};
    size_t count = 140 + static_cast<size_t>(stormMix_ * 120);
    float streak = stormMix_ * 5;
    for (size_t i = 0; i < count && i < flakes_.size(); ++i) {
        const Flake& f = flakes_[i];
        int wx = static_cast<int>((f.x + cam.x) / 16), wy = static_cast<int>((f.y + cam.y) / 16);
        if (w.roomAt(wx, wy) != "out" || (w.inCave(wx, wy) && w.tile(wx, wy) != Tile::Snow)) continue;
        DrawRectangle(static_cast<int>(f.x), static_cast<int>(f.y), f.size, f.size, c);
        if (streak > 1) DrawLine(static_cast<int>(f.x), static_cast<int>(f.y), static_cast<int>(f.x - streak), static_cast<int>(f.y - streak * 0.4f), Color{226, 234, 248, 90});
    }
}

void Weather::drawStormShade(bool outside) const {
    if (stormMix_ <= 0) return;
    DrawRectangle(0, 0, 640, 360, Color{10, 14, 28, static_cast<unsigned char>(stormMix_ * (outside ? 90 : 40))});
    if (strikeGlow_ > 0 && outside) DrawRectangle(0, 0, 640, 360, Color{200, 215, 255, static_cast<unsigned char>(strikeGlow_ * 60)});
}

void Weather::drawScreenSnow(float alpha) const {
    for (size_t i = 0; i < 150; ++i) {
        const Flake& f = flakes_[i];
        DrawRectangle(static_cast<int>(f.x), static_cast<int>(f.y), f.size, f.size, withAlpha(pal::rgb(0xE2EAF8), alpha));
    }
}

void Weather::drawWind(float time) const {
    Rng r(static_cast<uint32_t>(time * 20));
    int n = 26 + static_cast<int>(stormMix_ * 40);
    for (int i = 0; i < n; ++i)
        DrawRectangle(static_cast<int>(r.next() * 640), static_cast<int>(r.next() * 360), 14 + static_cast<int>(r.next() * 30), 1, Color{235, 242, 255, 150});
}
