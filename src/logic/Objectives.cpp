#include "logic/Objectives.h"

const Objective* QuestLog::current() const {
    for (const Objective& o : steps_)
        if (!o.done()) return &o;
    return nullptr;
}

int QuestLog::currentIndex() const {
    for (size_t i = 0; i < steps_.size(); ++i)
        if (!steps_[i].done()) return static_cast<int>(i);
    return static_cast<int>(steps_.size());
}
