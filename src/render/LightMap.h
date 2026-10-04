#pragma once
#include "logic/Session.h"
#include "raylib.h"
#include <map>
#include <string>

struct Cam;

// "Light is the progress bar": the scene is drawn fully lit, then multiplied by this light map.
// Unpowered rooms stay dark with a red emergency glow; powered rooms fill with warm light.
class LightMap {
public:
    LightMap();
    ~LightMap();
    LightMap(const LightMap&) = delete;
    LightMap& operator=(const LightMap&) = delete;

    void update(float dt, const Session& s);
    void flicker(const std::string& systemId) { flicker_[systemId] = 0.5f; }
    float roomLevel(const Session& s, const Room& r) const;

    void build(const Session& s, const Cam& cam, float t);
    void apply() const;   // multiplies the light map over the current canvas

private:
    void light(const Cam& cam, float x, float y, float r, Color c, float a) const;
    RenderTexture2D target_{};
    std::map<std::string, float> levels_, flicker_;
};
