#pragma once
#include "gfx/Sprites.h"
#include "gfx/Ui.h"
#include "logic/Session.h"
#include "raylib.h"
#include <algorithm>
#include <cmath>

class LightMap;

struct Cam {
    // the camera may look past the left/right map edge by this much, so the side HUD columns never hide
    // places at the edge of the world (the reactor room, the deep Ice Cave)
    static constexpr int MARGIN_X = 136;
    int x = 0, y = 0;
    static Cam follow(float px, float py) {
        Cam c;
        c.x = std::clamp(static_cast<int>(std::lround(px - 320)), -MARGIN_X, World::W * 16 - 640 + MARGIN_X);
        c.y = std::clamp(static_cast<int>(std::lround(py - 8 - 180)), 0, World::H * 16 - 360);
        return c;
    }
    bool sees(float px, float py, float w, float h) const { return px + w > x && py + h > y && px < x + 640 && py < y + 360; }
    Camera2D camera2D() const { return Camera2D{Vector2{0, 0}, Vector2{static_cast<float>(x), static_cast<float>(y)}, 0.0f, 1.0f}; }
};

// Draws the world: a ground texture baked once (terrain, the base, props), then the living layer
// (characters, items, emissive lights) and the world-space labels. It only reads the session (NFR-12).
class WorldRenderer {
public:
    explicit WorldRenderer(const SpriteBank& sprites);
    ~WorldRenderer();
    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    void bake(const World& world);
    void drawGround(const Cam& cam) const;
    void drawEntities(Session& s, float t) const;
    void drawEmissive(Session& s, const LightMap& lights, const Cam& cam, float t) const;
    void drawLabels(Session& s, const LightMap& lights, const Cam& cam, float t, Vector2 mouseWorld, bool mouseFree, bool promptsOn) const;

    const Texture2D& minimap() const { return mini_; }

private:
    void drawTile(const World& w, int x, int y, Tile t, class Rng& r) const;
    void drawBase(const World& w) const;
    static void drawFurniture();
    void drawStoryProps(const Session& s, float t) const;

    const SpriteBank& sprites_;
    RenderTexture2D ground_{};
    Texture2D mini_{}, mapTable_{};
};
