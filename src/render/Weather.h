#pragma once
#include <vector>

class Session;
struct Cam;

// Falling snow (two layers), wind streaks on the ridge, and the Hour 48 storm: heavy diagonal snow,
// a darker sky and lightning (at most a few strikes, never strobing; NFR-1 / accessibility).
class Weather {
public:
    Weather();
    void update(float dt, float time);
    void drawSnow(const Session& s, const Cam& cam) const;
    void drawScreenSnow(float alpha) const;     // title and intro screens
    void drawWind(float time) const;
    void drawStormShade(bool outside) const;

    void setStorm(bool on) { storm_ = on; }
    bool storm() const { return storm_; }
    bool lightningStruck() { bool s = struck_; struck_ = false; return s; }   // true once per strike

private:
    struct Flake { float x, y, speed, phase; int size; };
    std::vector<Flake> flakes_;
    bool storm_ = false, struck_ = false;
    float stormMix_ = 0, nextStrike_ = 4, strikeGlow_ = 0;
};
