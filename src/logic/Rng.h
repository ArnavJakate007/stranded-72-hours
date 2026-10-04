#pragma once
#include <cstdint>

// Deterministic random numbers (mulberry32), so the world is the same every run.
class Rng {
public:
    explicit Rng(uint32_t seed) : state_(seed) {}

    float next() {
        state_ += 0x6D2B79F5u;
        uint32_t t = state_;
        t = (t ^ (t >> 15)) * (1u | t);
        t ^= t + (t ^ (t >> 7)) * (61u | t);
        return static_cast<float>((t ^ (t >> 14)) >> 0) / 4294967296.0f;
    }
    int range(int lo, int hiExclusive) { return lo + static_cast<int>(next() * (hiExclusive - lo)); }

private:
    uint32_t state_;
};
