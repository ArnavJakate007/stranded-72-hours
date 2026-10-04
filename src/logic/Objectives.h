#pragma once
#include "logic/Types.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

// One step of the task chain. Text, completion and target are evaluated live from the session.
struct Objective {
    int act;
    std::function<std::string()> text;
    std::function<bool()> done;
    std::function<std::optional<IVec>()> target;   // tile the waypoint arrow points at
};

// Ordered list of objectives; the current one is the first not yet done (PRD P-6).
class QuestLog {
public:
    void clear() { steps_.clear(); }
    void add(Objective o) { steps_.push_back(std::move(o)); }
    const Objective* current() const;
    int currentIndex() const;
    const std::vector<Objective>& steps() const { return steps_; }

private:
    std::vector<Objective> steps_;
};
