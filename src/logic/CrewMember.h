#pragma once
#include "logic/Types.h"
#include <string>

enum class CrewState { Idle, Working, Injured, Recovering, Missing, Following, Dead };

// One crew member. Data is private; the colony and session change it only through these methods.
class CrewMember {
public:
    CrewMember(std::string id, std::string name, std::string role, float hp, CrewState state, IVec tile, Dir facing);

    const std::string& id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& role() const { return role_; }
    float hp() const { return hp_; }
    CrewState state() const { return state_; }
    bool alive() const { return state_ != CrewState::Dead; }
    bool idle() const { return state_ == CrewState::Idle; }
    const std::string& job() const { return job_; }
    int hoursLeft() const { return hoursLeft_; }
    IVec tile() const { return tile_; }
    Dir facing() const { return facing_; }

    void assign(const std::string& job, int hours);
    // Advances the current job by one hour. Returns the finished job id, or "" if still working.
    std::string workOneHour();
    void changeHp(float delta);      // clamps 0..100 and marks dead at 0
    void setState(CrewState s) { state_ = s; }
    void moveTo(IVec t) { tile_ = t; }
    void face(Dir d) { facing_ = d; }
    void setHp(float hp) { hp_ = hp; }

private:
    std::string id_, name_, role_;
    float hp_;
    CrewState state_;
    std::string job_;
    int hoursLeft_ = 0;
    IVec tile_;
    Dir facing_;
};

std::string crewStateName(CrewState s);
