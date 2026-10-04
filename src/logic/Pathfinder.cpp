#include "logic/Pathfinder.h"
#include <algorithm>

std::optional<std::vector<IVec>> Pathfinder::find(IVec start, const std::vector<IVec>& goals, const MoveTest& canMove) const {
    auto index = [this](IVec p) { return p.y * w_ + p.x; };
    std::vector<char> isGoal(w_ * h_, 0);
    for (IVec g : goals)
        if (g.x >= 0 && g.y >= 0 && g.x < w_ && g.y < h_) isGoal[index(g)] = 1;
    if (isGoal[index(start)]) return std::vector<IVec>{};

    std::vector<int> prev(w_ * h_, -1);
    std::vector<IVec> queue{start};
    prev[index(start)] = index(start);
    const IVec steps[4] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    for (size_t head = 0; head < queue.size(); ++head) {
        IVec cur = queue[head];
        for (IVec s : steps) {
            IVec n{cur.x + s.x, cur.y + s.y};
            if (n.x < 0 || n.y < 0 || n.x >= w_ || n.y >= h_) continue;
            int ni = index(n);
            if (prev[ni] != -1 || !canMove(cur, n)) continue;
            prev[ni] = index(cur);
            if (isGoal[ni]) {
                std::vector<IVec> path;
                for (int k = ni; k != index(start); k = prev[k]) path.push_back({k % w_, k / w_});
                std::reverse(path.begin(), path.end());
                return path;
            }
            queue.push_back(n);
        }
    }
    return std::nullopt;
}
