#include "render/LightMap.h"
#include "gfx/Ui.h"
#include "render/WorldRenderer.h"
#include <cmath>

LightMap::LightMap() {
    target_ = LoadRenderTexture(320, 180);   // half resolution: soft, chunky pixel light
    SetTextureFilter(target_.texture, TEXTURE_FILTER_POINT);
}

LightMap::~LightMap() { UnloadRenderTexture(target_); }

void LightMap::update(float dt, const Session& s) {
    for (const auto& sys : s.colony().systems()) {
        float& lv = levels_[sys->id()];
        float goal = sys->on() ? 1.0f : 0.0f;
        lv += (goal - lv) * std::min(1.0f, dt * 5.0f);
    }
    for (auto& kv : flicker_) kv.second = std::max(0.0f, kv.second - dt);
}

float LightMap::roomLevel(const Session& s, const Room& r) const {
    const Colony& c = s.colony();
    if (r.system == "core") return c.powered && !c.mainPowerOut ? 0.9f : 0.25f;
    if (!c.powered || c.mainPowerOut) return 0.0f;
    int used = c.usedUnits();
    if (!r.system.empty()) {
        auto it = levels_.find(r.system);
        float v = it == levels_.end() ? 0.0f : it->second;
        auto f = flicker_.find(r.system);
        if (f != flicker_.end() && f->second > 0) v = std::sin((0.5f - f->second) * 40.0f) > 0 ? v : 0.12f;   // flickers three times on
        return v;
    }
    if (r.key == "command" || r.key == "south") return std::min(1.0f, 0.4f + 0.12f * used);
    if (r.key == "storage") return std::min(1.0f, 0.3f + 0.1f * used);
    return 0.0f;
}

void LightMap::light(const Cam& cam, float x, float y, float r, Color c, float a) const {
    float sx = (x - cam.x) / 2.0f, sy = (y - cam.y) / 2.0f, sr = r / 2.0f;
    if (sx + sr < 0 || sy + sr < 0 || sx - sr > 320 || sy - sr > 180) return;
    Ui::radial(sx, sy, sr, c, a);
    Ui::radial(sx, sy, sr * 0.55f, c, a * 0.5f);
}

void LightMap::build(const Session& s, const Cam& cam, float t) {
    const World& w = s.world();
    const Colony& col = s.colony();
    int tier = col.usedUnits();
    BeginTextureMode(target_);
    if (col.storm) ClearBackground(Color{62, 70, 98, 255});   // the storm swallows the moonlight
    else ClearBackground(Color{104, 120, 162, 255});          // moonlight on the snow
    int tx0 = cam.x / 16, ty0 = cam.y / 16;
    for (int y = ty0; y <= ty0 + 23; ++y)
        for (int x = tx0; x <= tx0 + 40; ++x)
            if (w.inBounds(x, y) && w.inCave(x, y) && w.tile(x, y) != Tile::Snow && w.tile(x, y) != Tile::Path)
                DrawRectangle((x * 16 - cam.x) / 2, (y * 16 - cam.y) / 2, 8, 8, Color{10, 12, 22, 255});
    bool baseVisible = cam.sees(World::BASE_X - 8, World::BASE_Y - 8, 336, 290);
    auto floorRect = [&](const Room& r) {
        return Rectangle{std::floor((r.X() + 3 - cam.x) / 2.0f), std::floor((r.Y() + 3 - cam.y) / 2.0f), std::ceil((r.W() - 6) / 2.0f), std::ceil((r.H() - 6) / 2.0f)};
    };
    if (baseVisible)
        for (const Room& r : w.rooms()) {
            Rectangle f = floorRect(r);
            DrawRectangleRec(f, Color{static_cast<unsigned char>(12 + tier * 3), static_cast<unsigned char>(14 + tier * 3), static_cast<unsigned char>(24 + tier * 4), 255});
        }

    BeginBlendMode(BLEND_ADDITIVE);
    const Player& p = s.player();
    std::string playerRoom = w.roomAt(p.tile().x, p.tile().y);
    if (baseVisible)
        for (const Room& r : w.rooms()) {
            Rectangle f = floorRect(r);
            int sx = std::max(0, static_cast<int>(f.x)), sy = std::max(0, static_cast<int>(f.y));
            int ex = std::min(320, static_cast<int>(f.x + f.width)), ey = std::min(180, static_cast<int>(f.y + f.height));
            if (ex <= sx || ey <= sy) continue;
            BeginScissorMode(sx, sy, ex - sx, ey - sy);
            float lv = roomLevel(s, r);
            Color lc{r.light.r, r.light.g, r.light.b, 255};
            if (lv > 0.02f) {
                DrawRectangle(0, 0, 320, 180, withAlpha(lc, 0.42f * lv));
                light(cam, r.X() + r.W() / 2.0f, r.Y() + r.H() / 2.0f + 4, std::max(r.W(), r.H()) * 0.8f, lc, 0.85f * lv);
                for (int lx = r.X() + 14; lx <= r.X() + r.W() - 20; lx += 26) light(cam, lx + 3, r.Y() + 8, 26, Color{255, 214, 150, 255}, 0.45f * lv);
            } else {
                light(cam, r.X() + r.W() / 2.0f, r.Y() + 12, 40, Color{160, 32, 26, 255}, 0.6f * (0.85f + 0.15f * std::sin(t * 3)));
            }
            if (r.key == "eng")
                light(cam, 210 + World::OLD_OX, 184 + World::OLD_OY, 40, Color{255, 70, 50, 255}, col.powered ? 0.5f + 0.2f * std::sin(t * 2.4f) : 0.2f);
            if (r.key == playerRoom) light(cam, p.pixelX(), p.pixelY() - 8, 48, Color{190, 210, 255, 255}, 0.5f);
            EndScissorMode();
        }
    bool cave = w.inCave(p.tile().x, p.tile().y);
    if (playerRoom == "out") light(cam, p.pixelX(), p.pixelY() - 8, cave ? 90.0f : 44.0f, Color{190, 210, 255, 255}, cave ? 0.75f : 0.35f);
    if (s.hana().state() == CrewState::Following) {
        float hx, hy;
        Dir hd;
        s.hanaFollowPos(hx, hy, hd);
        light(cam, hx, hy - 8, 30, Color{160, 190, 255, 255}, 0.35f);
    }
    if (tier)
        for (const IVec& lp : w.lampPosts()) light(cam, lp.x + 1.0f, lp.y - 10.0f, 34, Color{255, 190, 120, 255}, 0.6f);
    if (baseVisible) {
        const Room* south = w.room("south");
        light(cam, World::BASE_X + 160.0f, World::BASE_Y + 284.0f, 30, Color{255, 200, 140, 255}, 0.6f * roomLevel(s, *south));
    }
    if (static_cast<int>(t * 1.2f) % 2 == 0) {
        light(cam, 75 * 16, 48 * 16, 30, Color{255, 40, 40, 255}, 0.55f);
        light(cam, 86 * 16, 49 * 16, 26, Color{255, 40, 40, 255}, 0.45f);
    }
    light(cam, 93 * 16, 62 * 16, 60, Color{110, 255, 120, 255}, (s.flags().reactorOn ? 0.7f : 0.35f) + 0.1f * std::sin(t * 2));
    const auto& lit = s.lights();
    for (size_t i = 0; i < w.lightMounts().size() && i < lit.size(); ++i)
        if (lit[i]) light(cam, w.lightMounts()[i].x * 16 + 8.0f, w.lightMounts()[i].y * 16 + 4.0f, 50, Color{255, 214, 140, 255}, 0.7f);
    if (col.isOn("beacon"))
        light(cam, World::BEACON_SITE.x * 16 + 8.0f, World::BEACON_SITE.y * 16 - 20.0f, 70, Color{255, 90, 60, 255}, 0.45f + 0.3f * std::sin(t * 6));
    if (s.flags().podLanded && !s.flags().podOpened)
        light(cam, World::POD.x * 16 + 8.0f, World::POD.y * 16.0f, 30, Color{255, 120, 80, 255}, 0.4f + 0.2f * std::sin(t * 6));
    for (const WorldItem& it : s.items())
        if (!it.taken && w.inCave(it.tile.x, it.tile.y)) light(cam, it.tile.x * 16 + 8.0f, it.tile.y * 16 + 8.0f, 22, Color{150, 200, 255, 255}, 0.4f);
    for (int y = std::max(33, ty0); y < std::min(World::H, ty0 + 24); ++y)
        for (int x = std::max(0, tx0); x < std::min(32, tx0 + 41); ++x)
            if (w.tile(x, y) == Tile::Ice) light(cam, x * 16 + 8.0f, y * 16 + 8.0f, 18, Color{120, 180, 255, 255}, 0.25f);
    if (!s.flags().antenna) light(cam, World::ANTENNA.x * 16 + 8.0f, World::ANTENNA.y * 16 + 8.0f, 24, Color{255, 190, 110, 255}, 0.25f + 0.15f * std::sin(t * 3));
    light(cam, 76 * 16 + 8, 19.5f * 16, 16, w.gateOpen() ? Color{80, 255, 120, 255} : Color{255, 60, 40, 255}, 0.5f);
    EndBlendMode();
    EndTextureMode();
}

void LightMap::apply() const {
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(target_.texture, Rectangle{0, 0, 320, -180}, Rectangle{0, 0, 640, 360}, Vector2{0, 0}, 0, WHITE);
    EndBlendMode();
}
