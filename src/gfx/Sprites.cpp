#include "gfx/Sprites.h"
#include "core/Palette.h"
#include "gfx/Ui.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
using Rows = std::vector<std::string>;
const Rows kHeadDown = {"...hhh...", "..hhhhh..", ".hhssssh.", "..sesse..", "...sss..."};
const Rows kHeadUp = {"...hhh...", "..hhhhh..", ".hhhhhhh.", "..hhhhh..", "...sss..."};
const Rows kHeadSide = {"...hhh...", "..hhhhh..", "..hhhsss.", "..hhhses.", "...sss..."};
const Rows kBodyDown = {"..ccccc..", ".ccccccc.", "cc.cac.cc", "c..cac..c", "s..ccc..s", "...ccc..."};
const Rows kBodyUp = {"..ccccc..", ".ckkkkkc.", "cckkkkkcc", "c.kkkkk.c", "s..ccc..s", "...ccc..."};
const Rows kBodySide = {"...ccc...", "..ccccc..", "..cccac..", "..cccac..", "...ccs...", "...ccc..."};
const Rows kLegs = {"...p.p...", "...p.p...", "...p.p...", "..bb.bb.."};
const Rows kLegsStride = {"...p.p...", "..p...p..", "..p...p..", ".bb...bb."};
const Rows kLegsSide = {"...pp....", "...pp....", "...pp....", "...bbb..."};
const Rows kLegsSideStride = {"...pp....", "..p..p...", ".p....p..", "bb....bb."};

struct Look { Color hair, skin, suit, pack; };
Look lookFor(const std::string& who) {
    if (who == "maya") return {pal::rgb(0x1C1418), pal::rgb(0xC88A64), pal::rgb(0x1F8F74), shade(pal::rgb(0x1F8F74), .8f)};
    if (who == "arjun") return {pal::rgb(0x15110F), pal::rgb(0xA8714E), pal::rgb(0xC98A2A), shade(pal::rgb(0xC98A2A), .8f)};
    if (who == "elena") return {pal::rgb(0x5A3420), pal::rgb(0xE0B08E), pal::rgb(0xB84545), shade(pal::rgb(0xB84545), .8f)};
    if (who == "hana") return {pal::rgb(0x101015), pal::rgb(0xE6BE9A), pal::rgb(0x3F7FC4), shade(pal::rgb(0x3F7FC4), .8f)};
    return {pal::rgb(0x1D1D26), pal::rgb(0xE0A882), pal::rgb(0xE0702E), pal::rgb(0x6B4F3A)};   // Kiran
}
std::string key(const std::string& who, Dir d, bool stride, bool flip) {
    return who + "_" + std::to_string(static_cast<int>(d)) + (stride ? "s" : "_") + (flip ? "f" : "_");
}
}  // namespace

Texture2D SpriteBank::makeCharacter(const std::string& who, Dir facing, bool stride, bool flip) const {
    bool frontBack = facing == Dir::Up || facing == Dir::Down;
    Rows rows;
    const Rows& head = facing == Dir::Up ? kHeadUp : facing == Dir::Down ? kHeadDown : kHeadSide;
    const Rows& body = facing == Dir::Up ? kBodyUp : facing == Dir::Down ? kBodyDown : kBodySide;
    const Rows& legs = frontBack ? (stride ? kLegsStride : kLegs) : (stride ? kLegsSideStride : kLegsSide);
    rows.insert(rows.end(), head.begin(), head.end());
    rows.insert(rows.end(), body.begin(), body.end());
    rows.insert(rows.end(), legs.begin(), legs.end());
    bool mirror = (facing == Dir::Left) != (flip && frontBack);
    if (mirror)
        for (auto& r : rows) std::reverse(r.begin(), r.end());
    Look lk = lookFor(who);
    auto colorOf = [&](char ch) -> Color {
        switch (ch) {
            case 'h': return lk.hair;
            case 's': return lk.skin;
            case 'e': return pal::rgb(0x14161C);
            case 'c': return lk.suit;
            case 'a': return shade(lk.suit, 0.6f);
            case 'k': return lk.pack;
            case 'p': return pal::rgb(0x2A3142);
            default: return pal::rgb(0x141821);
        }
    };
    Image img = GenImageColor(11, 17, BLANK);
    Color outline = pal::rgb(0x0A0D16);
    for (size_t r = 0; r < rows.size(); ++r)
        for (size_t k = 0; k < rows[r].size(); ++k)
            if (rows[r][k] != '.') {
                int x = static_cast<int>(k) + 1, y = static_cast<int>(r) + 1;
                ImageDrawPixel(&img, x - 1, y, outline);
                ImageDrawPixel(&img, x + 1, y, outline);
                ImageDrawPixel(&img, x, y - 1, outline);
                ImageDrawPixel(&img, x, y + 1, outline);
            }
    for (size_t r = 0; r < rows.size(); ++r)
        for (size_t k = 0; k < rows[r].size(); ++k)
            if (rows[r][k] != '.') ImageDrawPixel(&img, static_cast<int>(k) + 1, static_cast<int>(r) + 1, colorOf(rows[r][k]));
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    return t;
}

void SpriteBank::build() {
    for (const char* who : {"kiran", "maya", "arjun", "elena", "hana"})
        for (Dir d : {Dir::Up, Dir::Down, Dir::Left, Dir::Right})
            for (bool stride : {false, true})
                for (bool flip : {false, true}) chars_[key(who, d, stride, flip)] = makeCharacter(who, d, stride, flip);
    for (int s = 4; s <= 7; ++s) {
        Image img = GenImageColor(26, 42, BLANK);
        const int bx = 13, by = 40;
        auto fill = [&](int x, int y, int w, int h, Color c) { ImageDrawRectangle(&img, x, y, w, h, c); };
        fill(bx - 4, by, 9, 2, Color{40, 52, 82, 115});
        fill(bx - 1, by - 3, 2, 3, pal::rgb(0x2B211D));
        int h = s * 4;
        float th = h / 3.0f;
        for (int i = 0; i < h; ++i) {
            int t = static_cast<int>(i / th);
            float loc = i - t * th;
            int half = static_cast<int>(std::lround((loc / th) * s * (0.65f + 0.3f * t))) + 1, yy = by - 3 - h + i;
            fill(bx - half, yy, half * 2, 1, pal::rgb(0x14303A));
            fill(bx - half, yy, std::max(1, static_cast<int>(std::lround(half * 0.7f))), 1, pal::rgb(0x1E454C));
            if (loc < 1.5f) fill(bx - half + 1, yy, std::max(1, half * 2 - 2), 1, pal::rgb(0xD6E2F4));
        }
        pines_[s] = LoadTextureFromImage(img);
        UnloadImage(img);
    }
}

void SpriteBank::unload() {
    for (auto& kv : chars_) UnloadTexture(kv.second);
    for (auto& kv : pines_) UnloadTexture(kv.second);
    chars_.clear();
    pines_.clear();
}

void SpriteBank::drawCharacter(const std::string& who, float fx, float fy, Dir facing, bool stride, bool flip) const {
    auto it = chars_.find(key(who, facing, stride, flip));
    if (it == chars_.end()) return;
    int x = static_cast<int>(std::lround(fx)), y = static_cast<int>(std::lround(fy));
    DrawRectangle(x - 4, y, 9, 2, Color{0, 0, 0, 90});
    DrawTexture(it->second, x - 5, y - 15, WHITE);
}

void SpriteBank::drawPine(int size, float bx, float by) const {
    auto it = pines_.find(std::clamp(size, 4, 7));
    if (it != pines_.end()) DrawTexture(it->second, static_cast<int>(bx) - 13, static_cast<int>(by) - 40, WHITE);
}

// ---------------------------------------------------------------- Icons

void Icons::item(const std::string& n, float x, float y) {
    auto R = [&](float a, float b, float w, float h, unsigned c) { Ui::rect(x + a, y + b, w, h, pal::rgb(c)); };
    if (n == "Scrap") { R(1, 3, 10, 7, 0x8A93A3); R(1, 3, 10, 1, 0xB9C1CD); R(3, 5, 1, 1, 0x3D4452); R(8, 7, 1, 1, 0x3D4452); }
    else if (n == "Cables") { Ui::ring(static_cast<int>(x) + 6, static_cast<int>(y) + 6, 4, pal::rgb(0xD24A3A), 2); R(9, 9, 3, 2, 0xE8C440); }
    else if (n == "Antenna dish") { Ui::disc(static_cast<int>(x) + 6, static_cast<int>(y) + 5, 5, pal::rgb(0x9AA5B6)); Ui::disc(static_cast<int>(x) + 6, static_cast<int>(y) + 5, 3, pal::rgb(0xC7CFDB)); R(5, 9, 2, 3, 0x5A6578); }
    else if (n == "Power cell" || n == "Large power cell") { R(3, 2, 6, 9, 0xE8C440); R(4, 1, 4, 1, 0x3D4452); R(3, 6, 6, 1, 0x9C7F1E); if (n[0] == 'L') R(2, 3, 1, 7, 0xE8C440); }
    else if (n == "Amplifier") { R(1, 3, 10, 7, 0x2F4C75); R(3, 5, 3, 3, 0x8FD3FF); R(7, 5, 3, 1, 0xEF9F27); }
    else if (n == "Sealant") { R(2, 4, 7, 5, 0xC9D2DF); R(9, 5, 2, 3, 0xD24A3A); }
    else if (n == "Trauma kit") { R(1, 3, 10, 8, 0xDFE5EE); R(5, 4, 2, 6, 0xD24A3A); R(3, 6, 6, 2, 0xD24A3A); }
    else if (n == "Transmitter") { R(2, 4, 8, 7, 0x3B4252); R(3, 5, 6, 2, 0x5EC8FF); R(5, 0, 1, 4, 0x9AA5B6); R(4, 0, 3, 1, 0xC7CFDB); }
    else if (n == "Cutter") { R(1, 6, 7, 3, 0xE8A040); R(8, 5, 3, 5, 0x9AA5B6); R(10, 6, 2, 3, 0xFF8A5A); }
    else if (n == "Flight recorder") { R(2, 2, 8, 9, 0xE86A2A); R(3, 3, 6, 2, 0x2A2F3A); R(3, 7, 6, 1, 0xFFD0A0); }
    else { R(2, 1, 8, 10, 0xC8B890); for (int i = 0; i < 4; ++i) R(3, 3 + i * 2.0f, 6, 1, 0x7A6A48); }
}

void Icons::bar(int index, float x, float y) {
    static const Rows shapes[4] = {
        {".....##..", "....#..#.", "....#..#.", ".##..##..", "#..#.....", "#..#..##.", ".##..#..#", ".....#..#", "......##."},
        {"....#....", "...##....", "...###...", "..####.#.", ".######..", ".##o###..", ".#ooo##..", "..#oo#...", "...##...."},
        {"....#....", "...###...", "..#####..", "..#####..", ".##o####.", ".#o#####.", ".#######.", "..#####..", "...###..."},
        {".......##", ".....####", "...######", "..####.##", ".####.###", ".###.####", "..#.####.", ".#..###..", "#........"}};
    static const unsigned main[4] = {0x8FD3FF, 0xFF7A2F, 0x3A9CF5, 0x45C06A}, accent[4] = {0x8FD3FF, 0xFFD166, 0xC4E6FF, 0x45C06A};
    for (int r = 0; r < 9; ++r)
        for (int k = 0; k < 9; ++k) {
            char ch = shapes[index][r][k];
            if (ch != '.') Ui::rect(x + k, y + r, 1, 1, pal::rgb(ch == 'o' ? accent[index] : main[index]));
        }
}

void Icons::menu(const std::string& name, float x, float y, Color c) {
    static const std::map<std::string, Rows> shapes = {
        {"bolt", {"....###..", "...###...", "..###....", ".#######.", "....###..", "...###...", "..##.....", ".#.......", "........."}},
        {"crew", {"...###...", "..#####..", "..#####..", "...###...", ".#######.", "#########", "#########", "#########", "........."}},
        {"pin", {"...###...", "..#####..", ".###.###.", ".##...##.", ".###.###.", "..#####..", "...###...", "....#....", "....#...."}},
        {"book", {"#########", "#...#...#", "#.#.#.#.#", "#...#...#", "#.#.#.#.#", "#...#...#", "#.#.#.#.#", "#...#...#", "#########"}}};
    auto it = shapes.find(name);
    if (it == shapes.end()) return;
    for (int r = 0; r < 9; ++r)
        for (int k = 0; k < 9; ++k)
            if (it->second[r][k] == '#') Ui::rect(x + k, y + r, 1, 1, c);
}

void Icons::portrait(const std::string& who, float x, float y) {
    struct P { unsigned skin, hair, suit; };
    static const std::map<std::string, P> looks = {{"maya", {0xC88A64, 0x1C1418, 0x1F7A66}}, {"arjun", {0xA8714E, 0x15110F, 0xB87A24}},
                                                   {"elena", {0xE0B08E, 0x5A3420, 0xA63D3D}}, {"hana", {0xE6BE9A, 0x101015, 0x2F6FB8}},
                                                   {"voss", {0xE4B896, 0x22191A, 0x59606E}}, {"kiran", {0xD39A74, 0x16161C, 0xD9662B}}};
    auto it = looks.find(who);
    if (it == looks.end()) return;
    Color sk = pal::rgb(it->second.skin), hr = pal::rgb(it->second.hair), su = pal::rgb(it->second.suit);
    auto R = [&](float a, float b, float w, float h, Color c) { Ui::rect(x + a, y + b, w, h, c); };
    R(0, 0, 28, 28, pal::PanelEdge);
    R(1, 1, 26, 26, shade(speakerColor(who), 0.22f));
    R(4, 21, 20, 6, su);
    R(6, 20, 16, 1, su);
    R(11, 18, 6, 3, shade(sk, .85f));
    Ui::chamfer(x + 8, y + 6, 12, 13, sk);
    if (who == "maya") { R(7, 3, 14, 5, hr); R(6, 6, 3, 15, hr); R(19, 6, 3, 15, hr); }
    if (who == "arjun") { R(8, 3, 12, 4, hr); R(8, 15, 12, 4, shade(hr, 1.6f)); R(11, 15, 6, 1, sk); }
    if (who == "elena") { R(7, 3, 14, 5, hr); R(6, 5, 3, 11, hr); R(19, 5, 3, 11, hr); }
    if (who == "hana") { R(8, 3, 12, 4, hr); R(8, 6, 5, 2, hr); R(7, 5, 2, 6, hr); }
    if (who == "voss") { R(8, 3, 12, 4, hr); R(7, 4, 3, 10, hr); R(12, 21, 4, 6, pal::rgb(0xDFE5EE)); R(13, 22, 2, 5, pal::rgb(0xA33A3A)); }
    if (who == "kiran") { R(7, 2, 14, 5, hr); R(8, 6, 3, 2, hr); }
    R(11, 11, 2, 2, pal::rgb(0x14161C));
    R(16, 11, 2, 2, pal::rgb(0x14161C));
    R(12, 16, 4, 1, shade(sk, .7f));
}
