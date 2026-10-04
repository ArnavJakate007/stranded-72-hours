#pragma once
// Small value types shared by the logic side. No raylib here: logic never draws (SRS NFR-12).
#include <cstdint>
#include <string>

struct IVec {
    int x = 0, y = 0;
    bool operator==(const IVec& o) const { return x == o.x && y == o.y; }
    bool operator!=(const IVec& o) const { return !(*this == o); }
};

enum class Dir { Up, Down, Left, Right };

inline IVec dirVec(Dir d) {
    switch (d) {
        case Dir::Up: return {0, -1};
        case Dir::Down: return {0, 1};
        case Dir::Left: return {-1, 0};
        default: return {1, 0};
    }
}
inline Dir opposite(Dir d) {
    switch (d) {
        case Dir::Up: return Dir::Down;
        case Dir::Down: return Dir::Up;
        case Dir::Left: return Dir::Right;
        default: return Dir::Left;
    }
}
inline Dir dirTo(IVec a, IVec b) {
    if (b.x > a.x) return Dir::Right;
    if (b.x < a.x) return Dir::Left;
    if (b.y > a.y) return Dir::Down;
    return Dir::Up;
}
inline int manhattan(IVec a, IVec b) { return (a.x > b.x ? a.x - b.x : b.x - a.x) + (a.y > b.y ? a.y - b.y : b.y - a.y); }

struct RGB { uint8_t r = 0, g = 0, b = 0; };

enum class PuzzleKind { None, Wire, Pipe, Tune, Sequence, Cut };
