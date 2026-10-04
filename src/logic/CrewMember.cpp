#include "logic/CrewMember.h"
#include <algorithm>

CrewMember::CrewMember(std::string id, std::string name, std::string role, float hp, CrewState state, IVec tile, Dir facing)
    : id_(std::move(id)), name_(std::move(name)), role_(std::move(role)), hp_(hp), state_(state), tile_(tile), facing_(facing) {}

void CrewMember::assign(const std::string& job, int hours) {
    job_ = job;
    hoursLeft_ = hours;
    state_ = CrewState::Working;
}

std::string CrewMember::workOneHour() {
    if (state_ != CrewState::Working) return "";
    if (--hoursLeft_ > 0) return "";
    std::string done = job_;
    job_.clear();
    state_ = CrewState::Idle;
    return done;
}

void CrewMember::changeHp(float delta) {
    if (state_ == CrewState::Dead) return;
    hp_ = std::clamp(hp_ + delta, 0.0f, 100.0f);
    if (hp_ <= 0.0f) state_ = CrewState::Dead;
}

std::string crewStateName(CrewState s) {
    switch (s) {
        case CrewState::Idle: return "idle";
        case CrewState::Working: return "work";
        case CrewState::Injured: return "injured";
        case CrewState::Recovering: return "recover";
        case CrewState::Missing: return "missing";
        case CrewState::Following: return "following";
        default: return "dead";
    }
}
