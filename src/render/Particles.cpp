#include "render/Particles.h"
#include "logic/Rng.h"
#include <cmath>

namespace {
Rng& rng() {
    static Rng r(4242);
    return r;
}
}  // namespace

void Particles::emit(const Spawn& s) {
    P& p = pool_[next_];
    next_ = (next_ + 1) % pool_.size();   // oldest particle is recycled when the pool is full
    p.s = s;
    p.age = 0;
    p.on = true;
}

void Particles::burst(float x, float y, int count, float speed, float life, float size, Color c, float gravity, bool additive) {
    for (int i = 0; i < count; ++i) {
        float a = rng().next() * 6.2831853f, v = speed * (0.3f + rng().next() * 0.7f);
        emit({x, y, std::cos(a) * v, std::sin(a) * v, life * (0.5f + rng().next() * 0.5f), size, c, gravity, 1.5f, true, true, additive});
    }
}

void Particles::update(float dt) {
    for (P& p : pool_) {
        if (!p.on) continue;
        p.age += dt;
        if (p.age >= p.s.life) {
            p.on = false;
            continue;
        }
        p.s.vy += p.s.gravity * dt;
        float d = 1.0f / (1.0f + p.s.drag * dt);
        p.s.vx *= d;
        p.s.vy *= d;
        p.s.x += p.s.vx * dt;
        p.s.y += p.s.vy * dt;
    }
}

void Particles::draw() const {
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) BeginBlendMode(BLEND_ADDITIVE);
        for (const P& p : pool_) {
            if (!p.on || p.s.additive != (pass == 1)) continue;
            float k = 1.0f - p.age / p.s.life;
            Color c = p.s.color;
            if (p.s.fade) c.a = static_cast<unsigned char>(c.a * k);
            float size = p.s.shrink ? std::max(1.0f, p.s.size * k) : p.s.size;
            DrawRectangle(static_cast<int>(p.s.x - size / 2), static_cast<int>(p.s.y - size / 2), static_cast<int>(std::ceil(size)), static_cast<int>(std::ceil(size)), c);
        }
        if (pass == 1) EndBlendMode();
    }
}

void Particles::clear() {
    for (P& p : pool_) p.on = false;
}

size_t Particles::alive() const {
    size_t n = 0;
    for (const P& p : pool_) n += p.on;
    return n;
}
