#pragma once
#include "raylib.h"
#include <vector>

// A small pooled particle system (sparks, smoke, debris, engine exhaust, storm snow).
// The pool is reused, never reallocated during play (SRS performance: cap 200+ particles, no per-frame allocation).
class Particles {
public:
    struct Spawn {
        float x, y, vx, vy;
        float life;
        float size;
        Color color;
        float gravity = 0, drag = 0;
        bool fade = true, shrink = false, additive = false;
    };

    explicit Particles(size_t capacity = 400) : pool_(capacity) {}
    void emit(const Spawn& s);
    void burst(float x, float y, int count, float speed, float life, float size, Color c, float gravity = 0, bool additive = false);
    void update(float dt);
    void draw() const;
    void clear();
    size_t alive() const;

private:
    struct P { Spawn s; float age = 0; bool on = false; };
    std::vector<P> pool_;
    size_t next_ = 0;
};
