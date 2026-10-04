#include "logic/Colony.h"
#include "logic/DataStore.h"
#include "logic/PowerException.h"
#include <algorithm>

const char* barName(Bar b) {
    switch (b) {
        case Bar::Oxygen: return "Oxygen";
        case Bar::Warmth: return "Warmth";
        case Bar::Water: return "Water";
        default: return "Food";
    }
}

Colony::Colony(const Balance& balance) : balance_(balance) { reset(); }

void Colony::reset() {
    bars_ = {Resource("Oxygen", balance_.get("oxygen_start")), Resource("Warmth", balance_.get("warmth_start")),
             Resource("Water", balance_.get("water_start")), Resource("Food", balance_.get("food_start"))};
    systems_.clear();
    systems_.push_back(std::make_unique<OxygenSystem>(balance_.get("oxygen_refill")));
    systems_.push_back(std::make_unique<MedicalSystem>(balance_.get("medical_heal")));
    systems_.push_back(std::make_unique<HeaterSystem>(balance_.get("heater_refill")));
    systems_.push_back(std::make_unique<CommsSystem>(balance_.get("comms_charge")));
    systems_.push_back(std::make_unique<GreenhouseSystem>(balance_.get("greenhouse_refill")));
    systems_.push_back(std::make_unique<PurifierSystem>(balance_.get("purifier_refill")));
    systems_.push_back(std::make_unique<BeaconSystem>());
    crew_.clear();
    crew_.emplace_back("maya", "Maya", "Comms", 88.0f, CrewState::Idle, IVec{43, 30}, Dir::Down);
    crew_.emplace_back("arjun", "Arjun", "Engineer", 92.0f, CrewState::Idle, IVec{40, 33}, Dir::Right);
    crew_.emplace_back("elena", "Elena", "Medic", 79.0f, CrewState::Idle, IVec{56, 30}, Dir::Down);
    crew_.emplace_back("hana", "Hana", "Pilot", balance_.get("hana_start_hp"), CrewState::Missing, IVec{80, 63}, Dir::Down);
    powerUnits_ = 0;
    transmitter_ = 0.0f;
    powered = leakSealed = kitGiven = storm = mainPowerOut = false;
    battery = 0.0f;
    greenhouseGrow = 0;
    damaged_.clear();
    zeroOxygenHours = 0;
    trust = Trust{};
}

void Colony::damage(const std::string& id) {
    if (System* s = system(id); s && s->built()) {
        damaged_.insert(id);
        s->setOn(false);
    }
}

float Colony::colonyHealth() const {
    float sum = 0;
    for (const auto& r : bars_) sum += r.value();
    return sum / 4.0f;
}

float Colony::ratePerHour(Bar b) const {
    float r = 0.0f;
    switch (b) {
        case Bar::Oxygen: r = -balance_.get("oxygen_drain") - (leakSealed ? 0.0f : balance_.get("leak_extra_drain")); break;
        case Bar::Warmth: r = -balance_.get("warmth_drain") * (storm ? balance_.get("storm_warmth_multiplier") : 1.0f); break;
        case Bar::Water: r = -balance_.get("water_drain"); break;
        case Bar::Food: r = -balance_.get("food_drain"); break;
    }
    for (const auto& s : systems_) {
        if (!s->on()) continue;
        if (s->id() == "greenhouse" && greenhouseGrow < balance_.geti("greenhouse_grow_hours")) continue;   // still growing
        r += s->barEffect(b);
    }
    return r;
}

bool Colony::anyBarCritical() const {
    float t = balance_.get("low_bar_threshold");
    for (const auto& r : bars_)
        if (r < t) return true;
    return false;
}

System* Colony::system(const std::string& id) {
    for (auto& s : systems_)
        if (s->id() == id) return s.get();
    return nullptr;
}
const System* Colony::system(const std::string& id) const {
    for (const auto& s : systems_)
        if (s->id() == id) return s.get();
    return nullptr;
}

int Colony::usedUnits() const {
    int n = 0;
    for (const auto& s : systems_)
        if (s->on()) n += s->powerCost();
    return n;
}

void Colony::setPowerUnits(int units) {
    powerUnits_ = std::clamp(units, 0, 6);
    // if units dropped, switch systems off from the end of the list until the budget fits
    for (auto it = systems_.rbegin(); it != systems_.rend() && usedUnits() > powerUnits_; ++it) (*it)->setOn(false);
}

void Colony::switchOn(const std::string& id) {
    System* s = system(id);
    if (!s) return;
    if (!powered) throw PowerException(PowerException::Reason::NoPower, "No power. Restore the Power Core first.");
    if (mainPowerOut) throw PowerException(PowerException::Reason::NoPower, "Main power is out. Restart the reactor.");
    if (!s->built()) throw PowerException(PowerException::Reason::NotBuilt, s->name() + " is not built yet.");
    if (isDamaged(id)) throw PowerException(PowerException::Reason::Damaged, s->name() + " is damaged. Repair it at the Power Core.");
    if (s->on()) return;
    if (usedUnits() + s->powerCost() > powerUnits_)
        throw PowerException(PowerException::Reason::NoFreeUnit, "No free power unit. Switch a system off first.");
    s->setOn(true);
}

void Colony::switchOff(const std::string& id) {
    if (System* s = system(id)) s->setOn(false);
}

void Colony::build(const std::string& id) {
    if (System* s = system(id)) s->setBuilt(true);
}

bool Colony::isOn(const std::string& id) const {
    const System* s = system(id);
    return s && s->on();
}

CrewMember* Colony::crewById(const std::string& id) {
    for (auto& c : crew_)
        if (c.id() == id) return &c;
    return nullptr;
}

int Colony::crewAlive() const {
    return static_cast<int>(std::count_if(crew_.begin(), crew_.end(), [](const CrewMember& c) { return c.alive(); }));
}

void Colony::healWeakest(float amount) {
    CrewMember* weakest = nullptr;
    for (auto& c : crew_) {
        CrewState st = c.state();
        bool hanaTreated = c.id() == "hana" && (st == CrewState::Missing || st == CrewState::Following || st == CrewState::Injured || st == CrewState::Recovering);
        if (!c.alive() || hanaTreated) continue;   // Hana's first treatment is handled separately; after that Medical heals her too
        if (c.hp() < 100.0f && (!weakest || c.hp() < weakest->hp())) weakest = &c;
    }
    if (weakest) weakest->changeHp(amount);
}

void Colony::chargeTransmitter(float amount) { transmitter_ = std::min(100.0f, transmitter_ + amount); }

HourReport Colony::tickHour() {
    HourReport rep;
    float threshold = balance_.get("low_bar_threshold");
    std::array<float, 4> before{};
    for (int i = 0; i < 4; ++i) before[i] = bars_[i].value();

    // base drains, then every running system acts in its own way
    bars_[0] -= balance_.get("oxygen_drain") + (leakSealed ? 0.0f : balance_.get("leak_extra_drain"));
    bars_[1] -= balance_.get("warmth_drain") * (storm ? balance_.get("storm_warmth_multiplier") : 1.0f);
    bars_[2] -= balance_.get("water_drain");
    bars_[3] -= balance_.get("food_drain");
    bool txWasFull = transmitter_ >= 100.0f;
    for (auto& s : systems_)
        if (s->on() && s->built()) s->applyHour(*this);
    // every unused power unit trickles into the batteries
    if (powered && !mainPowerOut)
        battery = std::min(100.0f, battery + balance_.get("battery_charge_per_free_unit") * (powerUnits_ - usedUnits()));
    if (!txWasFull && transmitter_ >= 100.0f) rep.transmitterFull = true;

    for (int i = 0; i < 4; ++i)
        if (before[i] >= threshold && bars_[i].value() < threshold) rep.barsTurnedCritical.push_back(bars_[i].name());

    zeroOxygenHours = bars_[0].value() <= 0.0f ? zeroOxygenHours + 1 : 0;
    if (zeroOxygenHours >= balance_.geti("collapse_hours")) rep.collapse = true;

    bool low = anyBarCritical();
    float lowDmg = balance_.get("crew_low_bar_damage");
    for (auto& c : crew_) {
        if (!c.alive()) continue;
        if (low) c.changeHp(-lowDmg);
        if (c.id() == "hana") {
            CrewState st = c.state();
            if (st == CrewState::Missing || st == CrewState::Following) {
                c.changeHp(-balance_.get("hana_untreated_drain"));
            } else if (st == CrewState::Injured || st == CrewState::Recovering) {
                if (isOn("medical") && kitGiven) {
                    c.changeHp(balance_.get("medical_heal"));
                    if (c.alive()) c.setState(CrewState::Recovering);
                } else {
                    c.changeHp(-balance_.get("hana_untreated_drain"));
                }
                if (c.alive() && c.hp() > balance_.get("hana_recover_threshold")) {
                    c.setState(CrewState::Idle);
                    c.moveTo({58, 31});   // clear of Elena so their labels do not overlap
                    rep.hanaRecovered = true;
                }
            }
        }
        if (!c.alive()) {
            rep.deaths.push_back(c.name());
            continue;
        }
        std::string done = c.workOneHour();
        if (!done.empty()) rep.jobsDone.emplace_back(c.id(), done);
    }
    return rep;
}
