// Session: save and load (act checkpoints and autosaves)
#include "logic/PowerException.h"
#include "logic/Session.h"
#include "logic/SessionShared.h"
#include <algorithm>
#include <cmath>

using namespace session_detail;

// ---------------------------------------------------------------- save / load

SaveData Session::snapshot() const {
    SaveData d;
    d.setNum("hour", clock_.hour());
    d.set("clock_running", clock_.running() ? "1" : "0");
    d.set("powered", colony_.powered ? "1" : "0");
    d.set("leak_sealed", colony_.leakSealed ? "1" : "0");
    d.set("kit_given", colony_.kitGiven ? "1" : "0");
    d.set("main_power_out", colony_.mainPowerOut ? "1" : "0");
    d.set("storm", colony_.storm ? "1" : "0");
    d.setNum("battery", colony_.battery);
    d.setNum("grow", static_cast<float>(colony_.greenhouseGrow));
    d.setNum("zero_o2", static_cast<float>(colony_.zeroOxygenHours));
    d.setNum("trust_crew", static_cast<float>(colony_.trust.crewWeight));
    d.setNum("trust_total", static_cast<float>(colony_.trust.totalWeight));
    d.setNum("transmitter", colony_.transmitter());
    d.setNum("kiran_health", player_.health);
    d.setNum("suit_max", player_.suitMax);
    d.setNum("contact_hour", static_cast<float>(contactHour_));
    d.set("damaged", joinList(std::vector<std::string>(colony_.damaged().begin(), colony_.damaged().end())));
    for (Bar b : {Bar::Oxygen, Bar::Warmth, Bar::Water, Bar::Food}) d.setNum(std::string("bar_") + barName(b), colony_.bar(b).value());
    for (const auto& s : colony_.systems()) {
        d.set("sys_" + s->id() + "_on", s->on() ? "1" : "0");
        d.set("sys_" + s->id() + "_built", s->built() ? "1" : "0");
    }
    for (const CrewMember& c : colony_.crew()) {
        d.setNum("crew_" + c.id() + "_hp", c.hp());
        d.set("crew_" + c.id() + "_state", crewStateName(c.state()));
        d.setNum("crew_" + c.id() + "_x", static_cast<float>(c.tile().x));
        d.setNum("crew_" + c.id() + "_y", static_cast<float>(c.tile().y));
        d.set("crew_" + c.id() + "_job", c.job());
        d.setNum("crew_" + c.id() + "_left", static_cast<float>(c.hoursLeft()));
    }
    d.set("carry", joinList(std::vector<std::string>(player_.carry().items().begin(), player_.carry().items().end())));
    for (const auto& kv : cargo_) d.setNum("cargo_" + kv.first, static_cast<float>(kv.second));
    d.setNum("scrap", static_cast<float>(scrap_));
    d.setNum("cables", static_cast<float>(cables_));
    d.setNum("ice", static_cast<float>(ice_));
    d.set("tools", joinList(std::vector<std::string>(tools_.begin(), tools_.end())));
    d.set("story", joinList(std::vector<std::string>(story_.begin(), story_.end())));
    std::vector<std::string> taken;
    for (size_t i = 0; i < items_.size(); ++i)
        if (items_[i].taken) taken.push_back(std::to_string(i));
    d.set("items_taken", joinList(taken));
    d.set("braced", bools(braced_));
    d.set("lit", bools(lit_));
    d.set("pad", bools(padCleared_));
    const std::vector<bool> f = {flags_.oxygenOnce, flags_.sealantTaken, flags_.transmitterTaken, flags_.antenna, flags_.contact, flags_.generator,
                                 flags_.act3, flags_.act4, flags_.podLanded, flags_.podOpened, flags_.purifierOnce, flags_.hatchCut,
                                 flags_.coilInstalled, flags_.greenhouseCovered, flags_.firstMeal, flags_.reactorOn, flags_.stormWarned,
                                 flags_.stormPassed, flags_.report32, flags_.report32Met, flags_.report56, flags_.report56Met,
                                 flags_.shuttleGuided, flags_.hanaPlotted, flags_.waterReserve};
    d.set("flags", bools(f));
    return d;
}

void Session::loadFrom(const SaveData& d) {
    resetCore();
    clock_.reset(d.num("hour"), d.get("clock_running") == "1");
    colony_.powered = d.get("powered") == "1";
    colony_.leakSealed = d.get("leak_sealed") == "1";
    colony_.kitGiven = d.get("kit_given") == "1";
    colony_.mainPowerOut = d.get("main_power_out") == "1";
    colony_.storm = d.get("storm") == "1";
    colony_.battery = d.num("battery");
    colony_.greenhouseGrow = static_cast<int>(d.num("grow"));
    colony_.zeroOxygenHours = static_cast<int>(d.num("zero_o2"));
    colony_.trust.crewWeight = static_cast<int>(d.num("trust_crew"));
    colony_.trust.totalWeight = static_cast<int>(d.num("trust_total"));
    colony_.setTransmitter(d.num("transmitter"));
    player_.health = d.num("kiran_health", 100);
    player_.suitMax = player_.suit = d.num("suit_max", 100);
    contactHour_ = static_cast<int>(d.num("contact_hour"));
    std::vector<bool> f(25, false);
    readBools(d.get("flags"), f);
    flags_.oxygenOnce = f[0]; flags_.sealantTaken = f[1]; flags_.transmitterTaken = f[2]; flags_.antenna = f[3]; flags_.contact = f[4];
    flags_.generator = f[5]; flags_.act3 = f[6]; flags_.act4 = f[7]; flags_.podLanded = f[8]; flags_.podOpened = f[9];
    flags_.purifierOnce = f[10]; flags_.hatchCut = f[11]; flags_.coilInstalled = f[12]; flags_.greenhouseCovered = f[13];
    flags_.firstMeal = f[14]; flags_.reactorOn = f[15]; flags_.stormWarned = f[16]; flags_.stormPassed = f[17];
    flags_.report32 = f[18]; flags_.report32Met = f[19]; flags_.report56 = f[20]; flags_.report56Met = f[21];
    flags_.shuttleGuided = f[22]; flags_.hanaPlotted = f[23]; flags_.waterReserve = f[24];
    readBools(d.get("braced"), braced_);
    readBools(d.get("lit"), lit_);
    readBools(d.get("pad"), padCleared_);
    for (Bar b : {Bar::Oxygen, Bar::Warmth, Bar::Water, Bar::Food}) colony_.bar(b).set(d.num(std::string("bar_") + barName(b), colony_.bar(b).value()));
    for (const auto& s : colony_.systems())
        if (d.get("sys_" + s->id() + "_built") == "1") colony_.build(s->id());
    recomputeUnits();
    for (const auto& s : colony_.systems())
        if (d.get("sys_" + s->id() + "_on") == "1") {
            try {
                colony_.switchOn(s->id());
            } catch (const PowerException&) {
            }
        }
    for (const std::string& id : splitList(d.get("damaged"))) colony_.damage(id);
    static const std::map<std::string, CrewState> states = {
        {"idle", CrewState::Idle}, {"work", CrewState::Working}, {"injured", CrewState::Injured}, {"recover", CrewState::Recovering},
        {"missing", CrewState::Missing}, {"following", CrewState::Following}, {"dead", CrewState::Dead}};
    for (CrewMember& c : colony_.crew()) {
        c.setHp(d.num("crew_" + c.id() + "_hp", c.hp()));
        auto it = states.find(d.get("crew_" + c.id() + "_state"));
        CrewState st = it == states.end() ? c.state() : it->second;
        if (st == CrewState::Following) st = CrewState::Missing;   // the act save puts Kiran back at the base
        c.moveTo({static_cast<int>(d.num("crew_" + c.id() + "_x", float(c.tile().x))), static_cast<int>(d.num("crew_" + c.id() + "_y", float(c.tile().y)))});
        if (st == CrewState::Missing && c.id() == "hana") c.moveTo(World::HANA_TRAP);
        if (st == CrewState::Working && !d.get("crew_" + c.id() + "_job").empty())
            c.assign(d.get("crew_" + c.id() + "_job"), static_cast<int>(d.num("crew_" + c.id() + "_left", 1)));
        else
            c.setState(st);
    }
    for (const std::string& n : splitList(d.get("carry"))) cargo_[n]++;
    for (const auto& kv : d.all())
        if (kv.first.rfind("cargo_", 0) == 0) cargo_[kv.first.substr(6)] += static_cast<int>(d.num(kv.first));
    scrap_ = static_cast<int>(d.num("scrap"));
    cables_ = static_cast<int>(d.num("cables"));
    ice_ = static_cast<int>(d.num("ice"));
    for (const std::string& t : splitList(d.get("tools"))) tools_.insert(t);
    for (const std::string& s : splitList(d.get("story"))) story_.insert(s);
    for (const std::string& i : splitList(d.get("items_taken"))) {
        size_t k = static_cast<size_t>(std::stoi(i));
        if (k < items_.size()) items_[k].taken = true;
    }
    if (hana().state() != CrewState::Missing)
        for (const IVec& t : world_.debris()) world_.clearSolid(t.x, t.y);
    if (flags_.hatchCut)
        for (const IVec& t : world_.reactorHatch()) world_.clearSolid(t.x, t.y);
    for (size_t i = 0; i < padCleared_.size(); ++i)
        if (padCleared_[i]) world_.clearSolid(world_.padDebris()[i].x, world_.padDebris()[i].y);
    if (flags_.podLanded) world_.setSolidName(World::POD.x, World::POD.y, "Supply Pod");
    if (clock_.hour() >= GameClock::ACT4_HOUR) world_.openGate();
    int act = clock_.act();
    std::string up = GameClock::actName(act);
    for (char& ch : up) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
    emit(EventType::ActBanner, "ACT " + std::to_string(act), Tone::Info, 0, up);
    toast("SAVE LOADED");
}

