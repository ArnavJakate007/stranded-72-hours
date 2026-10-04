#pragma once
#include "logic/Types.h"
#include <functional>
#include <optional>
#include <vector>

// Breadth-first search on the tile grid, used for click-to-walk.
class Pathfinder {
public:
    using MoveTest = std::function<bool(IVec from, IVec to)>;
    Pathfinder(int width, int height) : w_(width), h_(height) {}

    // Shortest path from start to any goal, excluding the start tile. Empty path means already there.
    std::optional<std::vector<IVec>> find(IVec start, const std::vector<IVec>& goals, const MoveTest& canMove) const;

private:
    int w_, h_;
};
