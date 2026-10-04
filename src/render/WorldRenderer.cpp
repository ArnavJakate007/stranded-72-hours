#include "render/WorldRenderer.h"
#include "core/Palette.h"
#include "logic/Rng.h"
#include "render/LightMap.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {
inline void R(float x, float y, float w, float h, unsigned hex, unsigned char a = 255) { Ui::rect(x, y, w, h, pal::rgb(hex, a)); }
inline void Rc(float x, float y, float w, float h, Color c) { Ui::rect(x, y, w, h, c); }

void crate(float x, float y, float w, float h) {
    R(x, y, w, h, 0x5E3F20);
    R(x + 1, y + 1, w - 2, h - 2, 0x9A6B3A);
    R(x + 1, y + 1, w - 2, 2, 0xB8864E);
    R(x + 1, y + std::floor(h / 2), w - 2, 1, 0x6B4A28);
    R(x + std::floor(w / 2), y + 3, 1, h - 4, 0x6B4A28);
}
void bed(float x, float y) {
    R(x, y, 16, 30, 0x56627A);
    R(x + 1, y + 1, 14, 28, 0xDFE7F2);
    R(x + 2, y + 2, 12, 6, 0xF4F7FB);
    R(x + 1, y + 11, 14, 18, 0x2F6FB0);
    R(x + 1, y + 11, 14, 1, 0x3D90E5);
}
void rock(float x, float y, int s) {
    float w = static_cast<float>(s), h = std::round(s * 0.8f);
    R(x - 1, y + h - 1, w + 3, 3, 0x161C2A, 140);
    R(x, y + 2, w, h - 2, 0x252C3D);
    R(x + 2, y, w - 4, 2, 0x252C3D);
    R(x + 1, y + 2, std::round(w * 0.45f), h - 4, 0x343E55);
    R(x + w - 3, y + 3, 2, h - 5, 0x1A2030);
    R(x + 2, y, w - 4, 1, 0xD4DFF1);
    R(x + 1, y + 1, std::round(w * 0.6f), 1, 0xD4DFF1);
}
// The old 640x360 base layout is reused unchanged by shifting the matrix.
struct OldCoords {
    OldCoords() { rlPushMatrix(); rlTranslatef(static_cast<float>(World::OLD_OX), static_cast<float>(World::OLD_OY), 0); }
    ~OldCoords() { rlPopMatrix(); }
};
const int kCommsScreens[2][4] = {{208, 98, 14, 8}, {228, 98, 14, 8}};
constexpr int kCoreX = 210, kCoreY = 184;
}  // namespace

WorldRenderer::WorldRenderer(const SpriteBank& sprites) : sprites_(sprites) {}

WorldRenderer::~WorldRenderer() {
    if (ground_.id) UnloadRenderTexture(ground_);
    if (mini_.id) UnloadTexture(mini_);
    if (mapTable_.id) UnloadTexture(mapTable_);
}

void WorldRenderer::drawTile(const World& w, int x, int y, Tile t, Rng& r) const {
    float px = x * 16.0f, py = y * 16.0f;
    Tile up = w.tile(x, y - 1), dn = w.tile(x, y + 1);
    auto rnd = [&](int n) { return static_cast<float>(static_cast<int>(r.next() * n)); };
    switch (t) {
        case Tile::Path:
            R(px, py, 16, 16, 0x9CADCA);
            if (r.next() < 0.6f) { R(px + 3 + rnd(4), py + 2 + rnd(10), 2, 2, 0x8597BA); R(px + 9 + rnd(4), py + 2 + rnd(10), 2, 2, 0x8597BA); }
            break;
        case Tile::Rock:
            R(px, py, 16, 16, 0x232A3B);
            R(px + rnd(10), py + rnd(10), 5, 4, 0x2C3447);
            R(px + rnd(12), py + rnd(12), 3, 2, 0x343E55);
            if (up != Tile::Rock) { R(px, py, 16, 3, 0xD4DFF1); R(px, py + 3, 16, 1, 0x9FB0CC); R(px + rnd(12), py + 3, 3, 2, 0xD4DFF1); }
            if (dn != Tile::Rock) { R(px, py + 10, 16, 6, 0x171C29); for (int i = 0; i < 4; ++i) R(px + 1 + i * 4, py + 11, 1, 4, 0x262E40); }
            break;
        case Tile::CaveWall:
            R(px, py, 16, 16, 0x121726);
            R(px + rnd(10), py + rnd(10), 5, 4, 0x18202F);
            if (dn == Tile::CaveFloor || dn == Tile::Ice) { R(px, py + 10, 16, 6, 0x0C111B); for (int i = 0; i < 3; ++i) if (r.next() < 0.7f) R(px + 2 + i * 5, py + 12, 1, 2 + rnd(3), 0x8FB4DC); }
            if (up == Tile::CaveFloor || up == Tile::Ice) R(px, py, 16, 1, 0x3A4D70);
            if (up == Tile::Snow || up == Tile::Path) { R(px, py, 16, 3, 0xD4DFF1); R(px, py + 3, 16, 1, 0x7A8BA8); }
            if (w.tile(x - 1, y) == Tile::CaveFloor) R(px, py, 1, 16, 0x2C3C5A);
            if (w.tile(x + 1, y) == Tile::CaveFloor) R(px + 15, py, 1, 16, 0x2C3C5A);
            break;
        case Tile::CaveFloor:
        case Tile::Ice:
            R(px, py, 16, 16, 0x252F46);
            for (int i = 0; i < 4; ++i) R(px + rnd(15), py + rnd(15), 2, 1, r.next() < 0.5f ? 0x33415F : 0x3F5278);
            if (t == Tile::Ice) { R(px + 3, py + 4, 10, 10, 0x4F7FB0); R(px + 5, py + 2, 6, 10, 0x8FC4F0); R(px + 6, py + 3, 2, 6, 0xCFE8FF); R(px + 3, py + 13, 10, 2, 0x1A2236); }
            break;
        case Tile::Metal:   // scorched hull plates: mid grey so they read as metal, not as holes
            R(px, py, 16, 16, 0x8590A4);
            R(px + 1, py + 1, 14, 6, 0x95A0B3);
            R(px + 1, py + 9, 14, 6, 0x95A0B3);
            R(px, py + 7, 16, 2, 0x5E6878);
            R(px + 7, py, 1, 16, 0x6D7788);
            if (r.next() < 0.5f) R(px + rnd(12), py + rnd(12), 3, 2, 0x8A5A44);
            if (r.next() < 0.5f) R(px + rnd(12), py + rnd(12), 4, 1, 0xC1CEE6);
            break;
        case Tile::Hull:
            R(px, py, 16, 16, 0xAEB7C6);
            R(px, py + 7, 16, 1, 0x8F99AB);
            R(px + (x % 2) * 8, py, 1, 16, 0x8F99AB);
            if (up != Tile::Hull) R(px, py, 16, 2, 0xE4E9F1);
            if (dn != Tile::Hull) { R(px, py + 11, 16, 5, 0x6D7686); R(px, py + 15, 16, 1, 0x3E4452); }
            if (r.next() < 0.15f) R(px + 4, py + 4, 5, 3, 0x4A4F5C);
            break;
        case Tile::Pad:
            R(px, py, 16, 16, 0x353D4E);
            R(px, py, 16, 1, 0x424B5E);
            R(px, py, 1, 16, 0x424B5E);
            break;
        case Tile::Fence:
        case Tile::Gate: {
            Tile l = w.tile(x - 1, y), rt = w.tile(x + 1, y);
            bool horizontal = l == Tile::Fence || rt == Tile::Fence || l == Tile::Gate || rt == Tile::Gate;
            R(px + 7, py + 2, 2, 12, 0x4A505E);
            R(px + 7, py + 2, 2, 1, 0xC9D4E6);
            if (t == Tile::Gate) { for (int i = 0; i < 4; ++i) R(px + 2, py + i * 4, 12, 2, i % 2 ? 0x1C212C : 0xD9A020); }
            else if (horizontal) { R(px, py + 5, 16, 1, 0x7A8190); R(px, py + 10, 16, 1, 0x7A8190); }
            else { R(px + 5, py, 1, 16, 0x7A8190); R(px + 10, py, 1, 16, 0x7A8190); }
            break;
        }
        default: break;
    }
}

void WorldRenderer::drawFurniture() {
    OldCoords o;
    Ui::disc(186, 80, 12, pal::rgb(0x6F7A8D)); Ui::disc(186, 79, 10, pal::rgb(0xAEB8C8)); Ui::disc(184, 77, 4, pal::rgb(0xD5DDE8));
    R(185, 79, 2, 8, 0x5A6578); R(183, 88, 6, 10, 0x3A4357); R(178, 96, 16, 4, 0x2B3346);
    R(204, 96, 44, 14, 0x2B3448); R(204, 96, 44, 3, 0x46526C);
    for (const auto& s : kCommsScreens) R(s[0], s[1], s[2], s[3], 0x162A42);
    R(232, 60, 18, 28, 0x222A3A); R(233, 60, 16, 1, 0x3A4660); for (int i = 0; i < 5; ++i) R(236, 64 + i * 5, 10, 2, 0x151B27);
    R(226, 114, 8, 8, 0x3A4357); R(227, 114, 6, 2, 0x4C5670); crate(168, 120, 14, 11);
    for (int tx : {262, 344}) { R(tx, 60, 30, 12, 0x4D6684); R(tx + 2, 60, 26, 12, 0x6F8DB0); R(tx + 2, 61, 26, 2, 0xA9C3DD); R(tx + 10, 60, 1, 12, 0x4D6684); R(tx + 20, 60, 1, 12, 0x4D6684); }
    Ui::disc(296, 66, 7, pal::rgb(0x2A3346)); Ui::disc(296, 66, 5, pal::rgb(0x45516B)); R(262, 73, 112, 2, 0x56617A);
    R(440, 58, 32, 12, 0x9AA6B8); R(440, 58, 32, 2, 0xC3CCD9); R(455, 58, 1, 12, 0x7F8BA0); bed(396, 84); bed(446, 84);
    R(467, 84, 2, 22, 0x7F8BA0); R(465, 84, 6, 4, 0xCFE2F2); R(470, 96, 8, 6, 0x222A3A); R(398, 50, 8, 8, 0x2A3240);
    R(166, 150, 4, 48, 0x56617A); R(170, 150, 1, 48, 0x3A4357);
    Ui::disc(kCoreX, kCoreY, 21, pal::rgb(0x232A38)); Ui::disc(kCoreX, kCoreY, 18, pal::rgb(0x4A5469)); Ui::disc(kCoreX, kCoreY, 14, pal::rgb(0x2A303E));
    Ui::ring(kCoreX, kCoreY, 16, pal::rgb(0x6B7690)); Ui::disc(kCoreX, kCoreY, 7, pal::rgb(0x3E1012));
    R(174, 200, 14, 16, 0x2F3849); R(174, 200, 14, 2, 0x47526B); Ui::disc(178, 207, 2, pal::rgb(0x1A1F2B)); Ui::disc(184, 207, 2, pal::rgb(0x1A1F2B));
    R(300, 94, 40, 12, 0x2B3448); R(300, 94, 40, 3, 0x46526C); R(306, 96, 10, 6, 0x162A42); R(322, 96, 10, 6, 0x162A42);
    R(294, 136, 52, 32, 0x1D2433); R(296, 138, 48, 28, 0x2B3448); R(299, 141, 42, 22, 0x0F2A35);
    R(282, 146, 8, 10, 0x3A4357); R(282, 146, 8, 2, 0x4C5670); R(350, 146, 8, 10, 0x3A4357); R(350, 146, 8, 2, 0x4C5670);
    R(360, 100, 10, 8, 0x7A5235); R(358, 92, 14, 9, 0x2E7A3C); R(361, 90, 8, 4, 0x4FAE55);
    R(262, 210, 10, 8, 0x7A5235); R(260, 202, 14, 9, 0x2E7A3C); R(263, 200, 8, 3, 0x4FAE55);
    for (int py : {162, 190}) { R(394, py, 76, 14, 0x4A301E); R(395, py + 1, 74, 12, 0x6B4A30); R(397, py + 3, 70, 8, 0x3D2A1E); for (int i = 0; i < 7; ++i) R(399 + i * 10, py + 5, 7, 3, 0x33241A); }
    for (int i = 0; i < 4; ++i) { int bx = 170 + i * 32; R(bx, 242, 20, 34, 0x3C4659); R(bx + 1, 243, 18, 32, 0x465166); R(bx + 2, 244, 16, 7, 0xCBD5E3); R(bx + 1, 253, 18, 22, 0x2C4D7A); R(bx + 1, 253, 18, 1, 0x3E6698); }
    for (int i = 0; i < 5; ++i) { R(172 + i * 14, 296, 12, 14, 0x34405A); R(172 + i * 14, 296, 12, 1, 0x4A5878); R(181 + i * 14, 302, 1, 2, 0x8B98B2); }
    R(288, 300, 8, 7, 0x7A5235); R(286, 293, 12, 8, 0x2E7A3C);
    crate(346, 240, 14, 12); crate(362, 240, 14, 12); crate(346, 254, 14, 12); crate(354, 268, 16, 13); crate(378, 286, 14, 12); crate(346, 290, 14, 12);
    R(396, 236, 32, 6, 0x3A4357); R(396, 236, 32, 1, 0x56617A); crate(410, 229, 9, 7);
    for (int i = 0; i < 3; ++i) { R(398 + i * 11, 288, 9, 13, 0x3A5A7A); R(398 + i * 11, 288, 9, 2, 0x5A80A6); }
    for (int i = 0; i < 40; i += 4) { R(432 + i, 262, 2, 1, 0x5E6878); R(432 + i, 301, 2, 1, 0x5E6878); R(432, 262 + i, 1, 2, 0x5E6878); R(471, 262 + i, 1, 2, 0x5E6878); }
    R(306, 321, 28, 6, 0x3C4456); for (int i = 0; i < 7; ++i) R(307 + i * 4, 322, 2, 4, i % 2 ? 0x1C212C : 0x8A7A3A);
}

void WorldRenderer::drawBase(const World& w) const {
    const float BX = World::BASE_X, BY = World::BASE_Y;
    R(BX - 1, BY + 2, 330, 282, 0x283450, 115);
    R(BX - 4, BY - 4, 328, 280, 0x0B0F17);
    for (const Room& m : w.rooms()) { R(m.X() - 3, m.Y() - 3, m.W() + 6, m.H() + 6, 0x283246); R(m.X() - 3, m.Y() - 3, m.W() + 6, 1, 0x3F4B66); }
    for (const Room& m : w.rooms()) {
        float x0 = m.X() + 3.0f, y0 = m.Y() + 3.0f, fw = m.W() - 6.0f, fh = m.H() - 6.0f;
        Color a{m.floorA.r, m.floorA.g, m.floorA.b, 255}, b{m.floorB.r, m.floorB.g, m.floorB.b, 255};
        Rc(x0, y0, fw, fh, shade(b, 0.62f));
        for (int ty = 0; ty < fh; ty += 16)
            for (int tx = 0; tx < fw; tx += 16) {
                float ww = std::min(15.0f, fw - tx), hh = std::min(15.0f, fh - ty);
                Rc(x0 + tx, y0 + ty, ww, hh, ((tx + ty) / 16) % 2 ? a : b);
                Rc(x0 + tx, y0 + ty, ww, 1, Color{255, 255, 255, 30});
            }
    }
    for (const Room& m : w.rooms()) {
        if (m.key == "south") continue;
        R(m.X() + 3, m.Y() + 3, m.W() - 6, 8, 0x3A4560); R(m.X() + 3, m.Y() + 3, m.W() - 6, 1, 0x56637F);
        R(m.X() + 3, m.Y() + 7, m.W() - 6, 1, 0x4C5874); R(m.X() + 3, m.Y() + 11, m.W() - 6, 1, 0x1D2433);
        for (int lx = m.X() + 14; lx <= m.X() + m.W() - 20; lx += 26) R(lx, m.Y() + 4, 6, 2, 0x4D4232);
    }
    for (const Door& d : w.doors()) {
        if (d.vertical) {
            R(BX + d.a * 16 - 3, BY + d.b * 16, 6, d.len * 16, 0x8A7660);
            R(BX + d.a * 16 - 3, BY + d.b * 16, 6, 1, 0x141A26);
            R(BX + d.a * 16 - 3, BY + (d.b + d.len) * 16 - 1, 6, 1, 0x141A26);
        } else {
            R(BX + d.a * 16, BY + d.b * 16 - 3, d.len * 16, d.b == 17 ? 6 : 14, 0x8A7660);
        }
    }
    drawFurniture();
}

void WorldRenderer::bake(const World& w) {
    if (ground_.id) UnloadRenderTexture(ground_);
    ground_ = LoadRenderTexture(World::W * 16, World::H * 16);
    Rng r(11);
    BeginTextureMode(ground_);
    ClearBackground(pal::rgb(0xB0BFD9));
    for (int i = 0; i < 60000; ++i) {
        float v = r.next();
        DrawPixel(static_cast<int>(r.next() * World::W * 16), static_cast<int>(r.next() * World::H * 16), v < .45f ? pal::rgb(0xA2B2D0) : v < .86f ? pal::rgb(0xC1CEE6) : pal::rgb(0x90A3C6));
    }
    for (int i = 0; i < 500; ++i) {
        float x = std::floor(r.next() * World::W * 16), y = std::floor(r.next() * World::H * 16), wd = std::floor(10 + r.next() * 34);
        R(x, y, wd, 2, 0xC8D4EA);
        R(x + 2, y + 2, wd - 4, 1, 0x9BADCD);
    }
    for (int y = 0; y < World::H; ++y)
        for (int x = 0; x < World::W; ++x) {
            Tile t = w.tile(x, y);
            if (t != Tile::Snow && t != Tile::Base) drawTile(w, x, y, t, r);
        }
    for (int i = 0; i < 70; ++i) {
        float x = std::floor((64 + r.next() * 33) * 16), y = std::floor((40 + r.next() * 28) * 16);
        if (w.tile(static_cast<int>(x) / 16, static_cast<int>(y) / 16) != Tile::Snow) continue;
        R(x, y, std::floor(4 + r.next() * 6), 2, 0x5A6170);
        R(x + 1, y - 1, 2, 1, 0x8F99AB);
    }
    for (const Prop& b : w.boulders()) rock(b.tx * 16.0f + ((16 - b.size) >> 1), b.ty * 16.0f + 16 - std::round(b.size * 0.8f) - 1, b.size);
    R(74 * 16, 48 * 16 + 4, 13 * 16, 3, 0xB3333D);
    Ui::text("ARK-7", 78 * 16, 48 * 16 + 8, pal::rgb(0xB3333D), 1, false);
    R(89 * 16, 58 * 16 + 4, 8 * 16, 3, 0xD9A020);
    Ui::text("REACTOR", 90 * 16, 58 * 16 + 8, pal::rgb(0x5A4A10), 1, false);
    Ui::ring(86 * 16, 19 * 16 + 8, 64, pal::rgb(0xC4CFE2), 2);
    R(86 * 16 - 18, 19 * 16 - 14, 8, 44, 0xC4CFE2); R(86 * 16 + 10, 19 * 16 - 14, 8, 44, 0xC4CFE2); R(86 * 16 - 10, 19 * 16 + 4, 20, 8, 0xC4CFE2);
    Ui::text("LANDING ZONE", 79 * 16, 10 * 16, pal::rgb(0xC4CFE2), 1, false);
    R(World::ANTENNA.x * 16 + 2, World::ANTENNA.y * 16 + 6, 12, 8, 0x3A4252); R(World::ANTENNA.x * 16 + 4, World::ANTENNA.y * 16 + 4, 8, 2, 0x59606E);
    float pcx = World::PAD_TILE.x * 16 + 48.0f, pcy = World::PAD_TILE.y * 16 + 48.0f;
    Ui::ring(static_cast<int>(pcx), static_cast<int>(pcy), 34, pal::rgb(0xC4CFE2), 2);
    R(pcx - 14, pcy - 16, 6, 32, 0xC4CFE2); R(pcx + 8, pcy - 16, 6, 32, 0xC4CFE2); R(pcx - 8, pcy - 3, 16, 6, 0xC4CFE2);
    drawBase(w);
    for (const IVec& lp : w.lampPosts()) { R(lp.x, lp.y - 12, 2, 12, 0x2B3242); R(lp.x - 1, lp.y - 14, 4, 3, 0x596278); R(lp.x - 2, lp.y, 6, 2, 0x1E283E, 128); }
    float gx = World::GENERATOR.x * 16.0f, gy = World::GENERATOR.y * 16.0f;
    R(gx + 1, gy + 2, 14, 12, 0x2F3849); R(gx + 1, gy + 2, 14, 2, 0x47526B); R(gx + 4, gy + 6, 8, 5, 0x1A1F2B); R(gx + 12, gy - 2, 2, 6, 0x59606E);
    for (const IVec& wk : w.wreckage()) { R(wk.x * 16 + 1, wk.y * 16 + 5, 14, 9, 0x8F99AB); R(wk.x * 16 + 1, wk.y * 16 + 5, 14, 2, 0xCFD6E2); R(wk.x * 16 + 5, wk.y * 16 + 8, 6, 2, 0xB3333D); }
    EndTextureMode();

    // minimap: one pixel per tile
    Image mini = GenImageColor(World::W, World::H, BLANK);
    for (int y = 0; y < World::H; ++y)
        for (int x = 0; x < World::W; ++x) {
            unsigned c = 0x8FA0C0;
            switch (w.tile(x, y)) {
                case Tile::Rock: c = 0x262E40; break; case Tile::Path: c = 0xB8C6DE; break; case Tile::CaveFloor: c = 0x34425F; break;
                case Tile::CaveWall: c = 0x121726; break; case Tile::Metal: c = 0x59606E; break; case Tile::Hull: c = 0xC9D1DE; break;
                case Tile::Fence: c = 0x59606E; break; case Tile::Pad: c = 0x454E62; break; case Tile::Ice: c = 0x8FC4F0; break;
                case Tile::Base: c = 0xEF9F27; break; case Tile::Gate: c = 0xD9A020; break; default: break;
            }
            if (w.solidName(x, y) == "Tree") c = 0x2A4A52;
            ImageDrawPixel(&mini, x, y, pal::rgb(c));
        }
    if (mini_.id) UnloadTexture(mini_);
    mini_ = LoadTextureFromImage(mini);
    UnloadImage(mini);

    Image table = GenImageColor(42, 22, pal::rgb(0x1A5E86));
    Rng mr(4);
    for (int i = 0; i < 42; i += 6) ImageDrawRectangle(&table, i, 0, 1, 22, pal::rgb(0x24719C));
    for (int j = 0; j < 22; j += 6) ImageDrawRectangle(&table, 0, j, 42, 1, pal::rgb(0x24719C));
    for (int i = 0; i < 9; ++i) {
        int x = static_cast<int>(mr.next() * 36), y = static_cast<int>(mr.next() * 16), ww = 4 + static_cast<int>(mr.next() * 8), hh = 3 + static_cast<int>(mr.next() * 5);
        ImageDrawRectangle(&table, x, y, ww, hh, pal::rgb(0x4FBF86));
        ImageDrawRectangle(&table, x + 1, y, ww - 2, 1, pal::rgb(0x86E0AA));
    }
    if (mapTable_.id) UnloadTexture(mapTable_);
    mapTable_ = LoadTextureFromImage(table);
    UnloadImage(table);
}

void WorldRenderer::drawGround(const Cam& cam) const {
    float texH = static_cast<float>(ground_.texture.height);
    // only the part of the view that is inside the world; past the edge stays dark
    float x0 = std::max(0.0f, static_cast<float>(cam.x)), x1 = std::min(World::W * 16.0f, cam.x + 640.0f);
    if (x1 > x0)
        DrawTexturePro(ground_.texture, Rectangle{x0, texH - cam.y - 360.0f, x1 - x0, -360}, Rectangle{x0 - cam.x, 0, x1 - x0, 360}, Vector2{0, 0}, 0, WHITE);
    if (cam.x < 0) DrawRectangleGradientH(-cam.x, 0, 24, 360, Color{5, 7, 13, 255}, Color{5, 7, 13, 0});
    if (cam.x + 640 > World::W * 16) DrawRectangleGradientH(World::W * 16 - cam.x - 24, 0, 24, 360, Color{5, 7, 13, 0}, Color{5, 7, 13, 255});
}

void WorldRenderer::drawEntities(Session& s, float t) const {
    const Colony& col = s.colony();
    {
        OldCoords o;
        if (col.isOn("oxygen")) {
            float a = t * 9.0f;
            for (int k = 0; k < 3; ++k) {
                float an = a + k * 2.094f;
                for (int d = 1; d < 5; ++d) R(std::round(296 + std::cos(an) * d), std::round(66 + std::sin(an) * d), 1, 1, 0x8FA0BB);
            }
        } else {
            R(292, 66, 9, 1, 0x8FA0BB);
        }
        CrewState hs = s.hana().state();
        if (hs == CrewState::Injured || hs == CrewState::Recovering) { R(449, 86, 8, 3, 0x101015); R(450, 89, 6, 3, 0xE6BE9A); R(447, 95, 12, 2, 0xD24A3A); }
    }
    if (!col.leakSealed) {
        float lx = World::LEAK.x * 16.0f, ly = World::LEAK.y * 16.0f;
        R(lx + 3, ly + 6, 10, 3, 0x56617A);
        R(lx + 7, ly + 5, 2, 5, 0x2A303E);
        for (int i = 0; i < 3; ++i) {
            float k = std::fmod(t * 2 + i / 3.0f, 1.0f);
            Rc(lx + 8 + std::round(k * 6), ly + 6 - std::round(k * 8), 2, 2, withAlpha(pal::rgb(0xE6F0FF), 1 - k));
        }
    }
    drawStoryProps(s, t);
    if (s.hana().state() == CrewState::Missing)
        for (const IVec& d : s.world().debris()) { R(d.x * 16, d.y * 16 + 3, 16, 11, 0x6D7686); R(d.x * 16, d.y * 16 + 3, 16, 2, 0xAEB7C6); R(d.x * 16 + 3, d.y * 16 + 7, 10, 2, 0x3E4452); }

    std::vector<std::pair<float, std::function<void()>>> layer;
    const Player& p = s.player();
    int tx0 = p.tile().x - 22, ty0 = p.tile().y - 14;
    auto near = [&](IVec v) { return v.x >= tx0 && v.x <= tx0 + 44 && v.y >= ty0 && v.y <= ty0 + 28; };
    for (const Prop& tr : s.world().trees())
        if (near({tr.tx, tr.ty})) {
            float x = tr.tx * 16 + 8.0f, y = tr.ty * 16 + 14.0f;
            int size = tr.size;
            layer.push_back({y, [this, size, x, y] { sprites_.drawPine(size, x, y); }});
        }
    for (const WorldItem& it : s.items())
        if (!it.taken && near(it.tile)) {
            float x = it.tile.x * 16.0f, y = it.tile.y * 16.0f, bob = std::round(std::sin(t * 3 + it.tile.x));
            std::string name = it.name;
            layer.push_back({y + 12, [=] { R(x + 3, y + 12, 10, 2, 0x000000, 80); Icons::item(name, x + 2, y + 1 + bob); }});
        }
    for (const CrewMember& m : col.crew()) {
        if (!m.alive() || m.state() == CrewState::Following) continue;
        if (m.id() == "hana" && (m.state() == CrewState::Injured || m.state() == CrewState::Recovering)) continue;
        if (m.state() == CrewState::Missing) {
            float x = m.tile().x * 16.0f, y = m.tile().y * 16.0f;
            layer.push_back({y + 10, [=] { R(x + 2, y + 8, 12, 5, 0x3F7FC4); R(x + 1, y + 7, 4, 4, 0xE6BE9A); R(x, y + 6, 4, 2, 0x101015); R(x + 12, y + 9, 3, 3, 0x141821); }});
            continue;
        }
        bool working = m.state() == CrewState::Working;
        int bob = working && static_cast<int>(t * 2.5f) % 2 ? 1 : 0;
        float x = m.tile().x * 16 + 8.0f, fy = m.tile().y * 16 + 13.0f;
        std::string id = m.id();
        Dir f = m.facing();
        layer.push_back({fy, [this, id, x, fy, bob, f] { sprites_.drawCharacter(id, x, fy - bob, f, bob != 0, false); }});
    }
    bool stride = p.moving() && p.progress() > 0.05f && p.progress() < 0.6f;
    if (s.hana().state() == CrewState::Following) {
        float hx, hy;
        Dir hd;
        s.hanaFollowPos(hx, hy, hd);
        int par = p.stepParity() ^ 1;
        layer.push_back({hy - 0.5f, [this, hx, hy, hd, stride, par] { sprites_.drawCharacter("hana", hx, hy, hd, stride, par != 0); }});
    }
    float px = p.pixelX(), py = p.pixelY();
    bool heavy = s.heavyLoad();
    Dir pf = p.facing();
    int par = p.stepParity();
    layer.push_back({py, [this, px, py, pf, stride, par, heavy] {
                         sprites_.drawCharacter("kiran", px, py, pf, stride, par != 0);
                         if (heavy) { R(std::round(px) - 4, std::round(py) - 19, 8, 4, 0x8A6236); R(std::round(px) - 4, std::round(py) - 19, 8, 1, 0xB8864E); }
                     }});
    std::stable_sort(layer.begin(), layer.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& e : layer) e.second();
}

// Everything Acts 3 and 4 add to the map: pod, purifier, planters, braces, reactor, pad and beacon.
void WorldRenderer::drawStoryProps(const Session& s, float t) const {
    const World& w = s.world();
    const Colony& col = s.colony();
    const StoryFlags& fl = s.flags();
    // the landing-zone gate slides open at Hour 60 (the gate is baked into the ground texture)
    if (w.gateOpen())
        for (int y = 19; y <= 20; ++y) {
            R(76 * 16, y * 16, 16, 16, 0x9CADCA);
            R(76 * 16 + 1, y * 16, 2, 16, 0x6E7890);
            R(76 * 16 + 13, y * 16, 2, 16, 0x6E7890);
        }
    {
        OldCoords o;
        // planters: seedlings grow over the first 8 hours, the tarp covers them for the storm
        const System* gh = col.system("greenhouse");
        if (gh && gh->built()) {
            bool dead = col.isDamaged("greenhouse");
            float grow = std::min(1.0f, col.greenhouseGrow / 8.0f);
            for (int py : {162, 190})
                for (int i = 0; i < 7; ++i) {
                    float x = 399 + i * 10.0f;
                    int h = 2 + static_cast<int>(grow * 7);
                    if (dead) { R(x + 2, py + 6, 4, 2, 0x5A4A2A); continue; }
                    R(x + 3, py + 8 - h, 1, h, 0x2E7A3C);
                    R(x + 1, py + 8 - h, 3, 2, grow > 0.6f ? 0x5FCF66 : 0x4FAE55);
                    if (grow >= 1) R(x + 4, py + 4 - h / 2, 2, 2, 0xE8C548);
                }
            if (fl.greenhouseCovered && !fl.stormPassed) {
                R(392, 156, 80, 50, 0x3C5A7A, 220);
                for (int i = 0; i < 80; i += 10) R(392 + i, 156, 1, 50, 0x2A4058);
                R(392, 156, 80, 2, 0x6F8DB0);
            }
        }
        // the ice purifier in the storage plot
        const System* pu = col.system("purifier");
        if (pu && pu->built()) {
            bool on = pu->on();
            R(438, 266, 28, 30, 0x3A4357); R(439, 267, 26, 3, 0x56617A);
            R(442, 272, 20, 14, 0x16304A);
            float lv = on ? 0.4f + 0.3f * std::sin(t * 2) : 0.15f;
            R(443, 273 + std::round(12 * (1 - lv)), 18, std::round(12 * lv), on ? 0x4AA8E8 : 0x2A4A68);
            R(446, 289, 12, 4, col.isDamaged("purifier") ? 0xE24B4A : on ? 0x2FC596 : 0x5E6878);
            R(462, 260, 3, 8, 0x5E6878);
        }
    }
    // supply pod (lands at Hour 20 when contact was made early)
    if (fl.podLanded) {
        float x = World::POD.x * 16.0f, y = World::POD.y * 16.0f;
        R(x - 6, y + 14, 28, 4, 0x1E283E, 120);
        R(x - 2, y + 2, 20, 14, 0x8F99AB); R(x - 2, y + 2, 20, 2, 0xCFD6E2);
        R(x + 4, y - 2, 8, 4, 0x6D7686);
        if (fl.podOpened) { R(x, y + 5, 16, 8, 0x1A1F2B); R(x - 4, y + 12, 6, 3, 0x8F99AB); }
        else { R(x + 1, y + 6, 14, 2, 0xEF9F27); if (static_cast<int>(t * 2) % 2) R(x + 7, y - 4, 2, 2, 0xFF5A3A); }
        R(x - 10, y + 10, 4, 2, 0x5A6170); R(x + 22, y + 8, 3, 2, 0x5A6170);
    }
    // storm braces: dashed outline until placed, then a steel strut
    if (fl.act3 || fl.stormWarned) {
        const auto& br = s.braces();
        for (size_t i = 0; i < w.bracePoints().size(); ++i) {
            const IVec& b = w.bracePoints()[i];
            float x = b.x * 16.0f, y = b.y * 16.0f;
            if (i < br.size() && br[i]) {
                R(x + 1, y + 2, 3, 13, 0x6D7686); R(x + 12, y + 2, 3, 13, 0x6D7686);
                for (int k = 0; k < 11; ++k) { R(x + 3 + k, y + 3 + k, 2, 2, 0xAEB7C6); R(x + 12 - k, y + 3 + k, 2, 2, 0x8F99AB); }
            } else if (!fl.stormPassed) {
                for (int k = 0; k < 16; k += 4) { R(x + k, y, 2, 1, 0xEF9F27); R(x + k, y + 15, 2, 1, 0xEF9F27); R(x, y + k, 1, 2, 0xEF9F27); R(x + 15, y + k, 1, 2, 0xEF9F27); }
            }
        }
    }
    // reactor hatch and the reactor itself
    if (!fl.hatchCut) {
        for (const IVec& h : w.reactorHatch()) {
            float x = h.x * 16.0f, y = h.y * 16.0f;
            R(x, y, 16, 16, 0x3A4252);
            for (int k = 0; k < 4; ++k) R(x + 1, y + 2 + k * 4, 14, 2, k % 2 ? 0x1C212C : 0xD9A020);
        }
    } else {
        for (const IVec& h : w.reactorHatch()) { R(h.x * 16.0f, h.y * 16 + 13.0f, 16, 3, 0x5A6170); R(h.x * 16 + 3.0f, h.y * 16 + 12.0f, 3, 2, 0xFF8A3A); }
    }
    {
        float x = World::REACTOR.x * 16.0f, y = World::REACTOR.y * 16.0f;
        R(x - 6, y - 10, 28, 26, 0x232A38); R(x - 4, y - 8, 24, 22, 0x4A5469);
        Ui::disc(static_cast<int>(x + 8), static_cast<int>(y + 3), 8, pal::rgb(0x2A303E));
        Ui::disc(static_cast<int>(x + 8), static_cast<int>(y + 3), 5, fl.reactorOn ? pal::rgb(0x5EFF8A) : pal::rgb(0x1E3A26));
        R(x - 4, y + 12, 24, 2, 0xD9A020);
    }
    // landing pad debris and light mounts
    const auto& pc = s.padCleared();
    for (size_t i = 0; i < w.padDebris().size(); ++i) {
        if (i < pc.size() && pc[i]) continue;
        const IVec& d = w.padDebris()[i];
        float x = d.x * 16.0f, y = d.y * 16.0f;
        R(x - 2, y + 4, 20, 11, 0x6D7686); R(x - 2, y + 4, 20, 2, 0xAEB7C6); R(x + 2, y + 8, 12, 2, 0x3E4452); R(x + 10, y, 4, 6, 0x59606E);
    }
    const auto& lit = s.lights();
    for (size_t i = 0; i < w.lightMounts().size(); ++i) {
        const IVec& m = w.lightMounts()[i];
        float x = m.x * 16.0f, y = m.y * 16.0f;
        R(x + 5, y + 10, 6, 5, 0x2B3242);
        if (i < lit.size() && lit[i]) { R(x + 7, y + 2, 2, 9, 0x59606E); R(x + 4, y, 8, 3, 0x8F99AB); }
        else if (fl.act4) { for (int k = 0; k < 16; k += 4) R(x + k, y + 15, 2, 1, 0xEF9F27); }
    }
    // beacon tower
    {
        float x = World::BEACON_SITE.x * 16.0f, y = World::BEACON_SITE.y * 16.0f;
        R(x, y + 10, 16, 6, 0x3A4252); R(x, y + 10, 16, 1, 0x59606E);
        const System* bc = col.system("beacon");
        if (bc && bc->built()) {
            R(x + 6, y - 22, 4, 32, 0x8F99AB); R(x + 7, y - 22, 1, 32, 0xCFD6E2);
            for (int k = 0; k < 4; ++k) R(x + 3 + k, y - 14 + k * 8, 10 - k * 2, 1, 0x6D7686);
            R(x + 4, y - 26, 8, 5, bc->on() ? 0xFF5A3A : 0x4A2A2A);
        }
    }
}

void WorldRenderer::drawEmissive(Session& s, const LightMap& lights, const Cam& cam, float t) const {
    const World& w = s.world();
    const Colony& col = s.colony();
    if (cam.sees(World::BASE_X - 8, World::BASE_Y - 8, 336, 290)) {
        for (const Room& m : w.rooms()) {
            if (m.key == "south" || lights.roomLevel(s, m) <= 0.5f) continue;
            for (int lx = m.X() + 14; lx <= m.X() + m.W() - 20; lx += 26) {
                R(lx, m.Y() + 4, 6, 2, 0xFFE2A6);
                Ui::glow(lx + 3.0f, m.Y() + 5.0f, 7, pal::rgb(0xFFBE6E), 0.35f);
            }
        }
        OldCoords o;
        if (col.isOn("comms"))
            for (const auto& sc : kCommsScreens) {
                R(sc[0], sc[1], sc[2], sc[3], 0x5EC8FF);
                R(sc[0] + 1, sc[1] + 2, sc[2] - 2, 1, 0xBFEAFF);
                Ui::glow(sc[0] + 7.0f, sc[1] + 4.0f, 12, pal::rgb(0x5EC8FF), 0.3f);
            }
        if (col.isOn("medical")) {
            R(398, 50, 8, 8, 0xF4F7FB); R(401, 51, 2, 6, 0xE24B4A); R(399, 53, 6, 2, 0xE24B4A);
            R(471, 97, 6, 4, 0x0A1A12); R(471 + static_cast<int>(t * 4) % 6, 98, 1, 2, 0x4CFF9A);
        }
        const Room* cmd = w.room("command");
        float cl = lights.roomLevel(s, *cmd);
        if (cl > 0.05f) {
            DrawTexture(mapTable_, 299, 141, withAlpha(WHITE, std::min(1.0f, cl * 1.4f)));
            R(306, 96, 10, 6, 0x5EC8FF); R(322, 96, 10, 6, 0x7BE0B0);
        }
        float cp = col.powered ? 0.6f + 0.4f * std::sin(t * 2.4f) : 0.25f;
        Ui::disc(kCoreX, kCoreY, 5, col.powered ? (cp > 0.7f ? pal::rgb(0xFF6A48) : pal::rgb(0xE2453A)) : pal::rgb(0x5A1A16));
        Ui::glow(kCoreX, kCoreY, 18, pal::rgb(0xFF5032), 0.35f * cp);
    }
    if (col.usedUnits())
        for (const IVec& lp : w.lampPosts())
            if (cam.sees(lp.x - 8.0f, lp.y - 20.0f, 16, 24)) {
                R(lp.x, lp.y - 12, 2, 1, 0xFFD890);
                Ui::glow(lp.x + 1.0f, lp.y - 12.0f, 8, pal::rgb(0xFFC878), 0.5f);
            }
    if (static_cast<int>(t * 1.2f) % 2 == 0) { R(75 * 16 - 1, 48 * 16 - 1, 3, 3, 0xFF5A5A); R(86 * 16 - 1, 49 * 16 - 1, 3, 3, 0xFF5A5A); }
    R(76 * 16 + 6, 19 * 16 + 12, 4, 3, w.gateOpen() ? 0x5EFF8A : 0xFF5A3A);
    // landing lights, beacon strobe, reactor core
    const auto& lit = s.lights();
    for (size_t i = 0; i < w.lightMounts().size() && i < lit.size(); ++i) {
        if (!lit[i]) continue;
        const IVec& m = w.lightMounts()[i];
        bool pulse = col.isOn("beacon") && static_cast<int>(t * 4 + i) % 6 == 0;
        R(m.x * 16 + 5, m.y * 16 + 1, 6, 2, pulse ? 0xFFFFFF : 0xFFE08A);
        Ui::glow(m.x * 16 + 8.0f, m.y * 16 + 2.0f, pulse ? 16 : 10, pal::rgb(0xFFD070), 0.5f);
    }
    if (col.isOn("beacon")) {
        float bx = World::BEACON_SITE.x * 16 + 8.0f, by = World::BEACON_SITE.y * 16 - 24.0f;
        float k = 0.5f + 0.5f * std::sin(t * 6);
        R(bx - 3, by - 1, 6, 3, 0xFF8A6A);
        Ui::glow(bx, by, 14 + 10 * k, pal::rgb(0xFF5A3A), 0.6f);
    }
    if (s.flags().reactorOn) Ui::glow(World::REACTOR.x * 16 + 8.0f, World::REACTOR.y * 16 + 3.0f, 16 + 3 * std::sin(t * 3), pal::rgb(0x5EFF8A), 0.45f);
    for (const WorldItem& it : s.items()) {
        if (it.taken) continue;
        float x = it.tile.x * 16.0f, y = it.tile.y * 16.0f;
        if (!cam.sees(x, y, 16, 16)) continue;
        float ph = std::fmod(t * 0.7f + it.tile.x * 0.13f, 1.0f);
        if (ph < 0.12f) { R(x + 12, y + 1, 1, 5, 0xFFFFFF); R(x + 10, y + 3, 5, 1, 0xFFFFFF); }
    }
}

void WorldRenderer::drawLabels(Session& s, const LightMap& lights, const Cam& cam, float t, Vector2 mouseWorld, bool mouseFree, bool promptsOn) const {
    (void)lights;
    const Colony& col = s.colony();
    const Player& kiran = s.player();
    const std::string here = s.world().roomAt(kiran.tile().x, kiran.tile().y);
    if (cam.sees(World::BASE_X - 8, World::BASE_Y - 8, 336, 290)) {
        OldCoords o;
        Vector2 mouseOld{mouseWorld.x - World::OLD_OX, mouseWorld.y - World::OLD_OY};
        // Quiet by default: a one-line tag with a status dot. The full label shows for the room Kiran is in,
        // or the one under the mouse, so the base never turns into a wall of text.
        auto tag = [&](const char* room, float x, float y, const std::string& name, const std::string& detail, Color c) {
            bool full = here == room || (mouseFree && std::fabs(mouseOld.x - (x + 24)) < 40 && std::fabs(mouseOld.y - (y + 8)) < 16);
            if (full && !detail.empty()) {
                Ui::chip2(x, y, name, detail, c);
                return;
            }
            float w = Ui::width(name) + 13.0f;
            Ui::rect(x, y, w, 10, Color{6, 10, 20, 170});
            Ui::rect(x + 3, y + 3, 4, 4, c);
            Ui::text(name, x + 10, y + 2, pal::Soft, 1, false);
        };
        auto st = [&](const char* id) -> std::pair<std::string, Color> {
            const System* sys = col.system(id);
            if (col.isDamaged(id)) return {"Damaged", pal::Red};
            if (!sys->built()) return {"Not built", pal::Offline};
            return sys->on() ? std::make_pair(std::string("Online"), pal::TealHi) : std::make_pair(std::string("Offline"), pal::Offline);
        };
        auto c = st("comms");
        bool txBuilt = col.system("comms")->built();
        tag("comms", 206, 62, "Comms", txBuilt ? c.first : "No transmitter", txBuilt ? c.second : pal::Offline);
        auto ox = st("oxygen");
        tag("north", 300, 50, "O2 Plant", ox.first, ox.second);
        auto md = st("medical");
        tag("medical", 408, 62, "Medical Bay", md.first, md.second);
        bool coreUp = col.powered && !col.mainPowerOut;
        tag("eng", 166, 156, "Power Core",
            coreUp ? std::to_string(col.usedUnits()) + "/" + std::to_string(col.powerUnits()) + " units" : col.mainPowerOut ? "Storm outage" : "Dead",
            coreUp ? pal::Amber : pal::Red);
        auto gh = st("greenhouse");
        if (gh.first == "Online" && col.greenhouseGrow < 8) gh = {"Growing " + std::to_string(col.greenhouseGrow) + "/8h", pal::TealHi};
        tag("green", 416, 150, "Greenhouse", gh.first, gh.second);
        if (col.system("purifier")->built()) {
            auto pu = st("purifier");
            tag("storage", 420, 248, "Purifier", pu.first, pu.second);
        }
        auto ht = st("heater");
        tag("quarters", 166, 228, "Quarters", "Heater " + ht.first, ht.second);
        if (here == "command") tag("command", 262, 92, "Command", "", pal::Amber);
        if (here == "storage") tag("storage", 344, 228, "Storage", "", pal::Amber);
    }
    for (const CrewMember& m : col.crew()) {
        if (!m.alive() || m.state() == CrewState::Following || m.state() == CrewState::Missing) continue;
        float x, y;
        if (m.id() == "hana" && (m.state() == CrewState::Injured || m.state() == CrewState::Recovering)) { x = 58 * 16 + 4.0f; y = 27 * 16 + 8.0f; }
        else { x = m.tile().x * 16 + 8.0f; y = m.tile().y * 16 - 15.0f; }
        if (!cam.sees(x - 30, y, 60, 12)) continue;
        std::string label;
        Color fg, border;
        switch (m.state()) {
            case CrewState::Idle: label = "FREE FOR A JOB"; fg = pal::Amber; border = pal::rgb(0x6B4A12); break;
            case CrewState::Working: label = "WORKING " + std::to_string(m.hoursLeft()) + "H"; fg = pal::TealHi; border = pal::rgb(0x14503E); break;
            case CrewState::Recovering: label = "RECOVERING"; fg = pal::TealHi; border = pal::rgb(0x14503E); break;
            default: label = "INJURED"; fg = pal::Red; border = pal::rgb(0x5A1E1E); break;
        }
        bool near = manhattan(kiran.tile(), m.tile()) <= 3 || (mouseFree && std::fabs(mouseWorld.x - x) < 12 && std::fabs(mouseWorld.y - (y + 20)) < 16);
        if (near) {
            Ui::chip(x - std::floor((Ui::width(label) + 10) / 2.0f), y, label, fg, border);
        } else {   // a small diamond above the head, gently bobbing
            float by = y + 6 + std::round(std::sin(t * 3 + x) * 1);
            Ui::rect(x - 1, by - 2, 3, 5, fg);
            Ui::rect(x - 2, by - 1, 5, 3, fg);
            Ui::rect(x, by - 1, 1, 1, shade(fg, 1.5f));
        }
    }
    // bouncing marker over the current goal
    if (auto tg = s.goalTarget()) {
            float x = tg->x * 16 + 8.0f, y = tg->y * 16.0f;
            if (cam.sees(x - 8, y - 20, 16, 24)) {
                float yy = y - 12 + std::round(std::sin(t * 5) * 2);
                R(x - 4, yy, 9, 2, 0xEF9F27); R(x - 3, yy + 2, 7, 1, 0xEF9F27); R(x - 2, yy + 3, 5, 1, 0xEF9F27); R(x - 1, yy + 4, 3, 1, 0xEF9F27); R(x, yy + 5, 1, 1, 0xEF9F27);
            }
        }
    const Player& p = s.player();
    float px = std::round(p.pixelX()), py = std::round(p.pixelY());
    if (promptsOn && !p.moving() && s.holdProgress() < 0) {
        IVec f = p.front();
        if (s.world().inBounds(f.x, f.y) && s.world().passEdge(p.tile(), f))
            if (auto a = s.actionAt(f))
                if (!a->quiet) {
                    std::string lab = (a->hold > 0 ? "Hold " : "") + a->label;
                    float w = Ui::width(lab) + 22.0f, x = px - std::floor(w / 2), y = py - 34;
                    R(x, y, w, 13, 0x04070D); R(x + 1, y + 1, w - 2, 11, 0x0C1422); R(x + 1, y + 1, w - 2, 1, 0xEF9F27); R(x + 3, y + 2, 9, 9, 0x1C2840);
                    Ui::text("Z", x + 5, y + 3, pal::Amber, 1, false);
                    Ui::text(lab, x + 15, y + 3, pal::Ink);
                }
    }
    float hp = s.holdProgress();
    if (hp >= 0) {
        R(px - 16, py - 26, 32, 6, 0x04070D);
        R(px - 15, py - 25, std::round(30 * hp), 4, 0xEF9F27);
        Ui::textCentered(s.holdLabel(), px, py - 38, pal::Ink);
    }
    if (!p.path.empty() && static_cast<int>(t * 3) % 2 == 0) { IVec e = p.path.back(); R(e.x * 16 + 6, e.y * 16 + 6, 4, 4, 0xEF9F27); }
    if (mouseFree) {
        int tx = static_cast<int>(std::floor(mouseWorld.x / 16)), ty = static_cast<int>(std::floor(mouseWorld.y / 16));
        if (s.world().inBounds(tx, ty)) {
            auto a = s.actionAt({tx, ty});
            bool ok = (a && !a->quiet) || s.canStand({tx, ty});
            Ui::brackets(tx * 16 + 3.0f, ty * 16 + 3.0f, 10, 10, t, ok ? pal::Amber : pal::rgb(0x7A869E), 4, 1, false);
            if (a && !a->quiet) {
                float w = Ui::width(a->label) + 10.0f;
                Ui::chip(tx * 16 + 8 - std::floor(w / 2), ty * 16 - 14.0f, a->label, pal::Ink, pal::Amber);
            }
        }
    }
}
