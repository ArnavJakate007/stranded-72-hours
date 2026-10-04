#pragma once
#include "logic/CrewMember.h"
#include "logic/Resource.h"
#include "logic/System.h"
#include <array>
#include <memory>
#include <set>
#include <string>
#include <vector>

class Balance;

enum class Bar { Oxygen = 0, Warmth, Water, Food };
const char* barName(Bar b);

// What happened during one game hour, so the session can turn it into dialogue and sounds.
struct HourReport {
    std::vector<std::string> barsTurnedCritical;
    std::vector<std::string> deaths;
    std::vector<std::pair<std::string, std::string>> jobsDone;   // crew id, job id
    bool transmitterFull = false;
    bool collapse = false;
    bool hanaRecovered = false;
};

// Trust Score: weighted share of delegable tasks finished by crew (PRD G8).
struct Trust {
    int crewWeight = 0;
    int totalWeight = 0;
    int percent() const { return totalWeight ? crewWeight * 100 / totalWeight : 0; }
};

// The base: bars, power budget, systems and crew. Aggregation of Systems and CrewMembers;
// power can only change through switchOn/switchOff, so it never exceeds the units (encapsulation).
class Colony {
public:
    explicit Colony(const Balance& balance);
    void reset();

    Resource& bar(Bar b) { return bars_[static_cast<int>(b)]; }
    const Resource& bar(Bar b) const { return bars_[static_cast<int>(b)]; }
    float ratePerHour(Bar b) const;
    bool anyBarCritical() const;

    System* system(const std::string& id);
    const System* system(const std::string& id) const;
    const std::vector<std::unique_ptr<System>>& systems() const { return systems_; }
    int usedUnits() const;
    int powerUnits() const { return powerUnits_; }
    void setPowerUnits(int units);
    void switchOn(const std::string& id);    // throws PowerException
    void switchOff(const std::string& id);
    void build(const std::string& id);
    bool isOn(const std::string& id) const;

    std::vector<CrewMember>& crew() { return crew_; }
    const std::vector<CrewMember>& crew() const { return crew_; }
    CrewMember* crewById(const std::string& id);
    int crewAlive() const;

    void healWeakest(float amount);
    void chargeTransmitter(float amount);
    float transmitter() const { return transmitter_; }
    void setTransmitter(float v) { transmitter_ = v; }

    HourReport tickHour();

    // Storm damage (T12-T14): a damaged system cannot run until repaired.
    void damage(const std::string& id);
    void repair(const std::string& id) { damaged_.erase(id); }
    bool isDamaged(const std::string& id) const { return damaged_.count(id) > 0; }
    const std::set<std::string>& damaged() const { return damaged_; }
    float colonyHealth() const;           // average of the four bars (status reports)

    bool powered = false;       // Power Core restored (T1)
    bool leakSealed = false;    // T2
    bool kitGiven = false;      // T4
    bool storm = false;         // Hours 48-52: warmth drains twice as fast
    bool mainPowerOut = false;  // storm knocked out main power; the reactor brings it back (T13)
    float battery = 0.0f;       // charged by unused power units (T12: above 70% the colony rides out the storm)
    int greenhouseGrow = 0;     // hours of growth; food only comes after 8 (T10)
    int zeroOxygenHours = 0;
    Trust trust;

private:
    const Balance& balance_;
    std::array<Resource, 4> bars_;
    std::vector<std::unique_ptr<System>> systems_;
    std::vector<CrewMember> crew_;
    int powerUnits_ = 0;
    float transmitter_ = 0.0f;
    std::set<std::string> damaged_;
};
