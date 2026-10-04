// Session: power systems, crew jobs and talking to the crew
#include "logic/PowerException.h"
#include "logic/Session.h"
#include "logic/SessionShared.h"
#include <algorithm>
#include <cmath>

using namespace session_detail;

// ---------------------------------------------------------------- systems and crew

void Session::recomputeUnits() {
    int units = 0;
    if (colony_.powered && !colony_.mainPowerOut)
        units = bal_.geti("units_after_t1") + (flags_.generator ? 1 : 0) + (flags_.coilInstalled ? 2 : 0) + (flags_.reactorOn ? 1 : 0);
    colony_.setPowerUnits(std::min(6, units));
}

void Session::addTrust(int weight, bool byCrew) {
    colony_.trust.totalWeight += weight;
    if (byCrew) colony_.trust.crewWeight += weight;
}

void Session::toggleSystem(const std::string& id) {
    System* s = colony_.system(id);
    if (!s) return;
    if (s->on()) {
        colony_.switchOff(id);
        sound("click");
        message(s->name() + " switched off.", Tone::Info);
        if (id == "medical" && (hana().state() == CrewState::Injured || hana().state() == CrewState::Recovering)) dialogue_.sayOnce("elena_med_dark");
        return;
    }
    try {
        colony_.switchOn(id);
    } catch (const PowerException& e) {
        message(e.what(), Tone::Bad);
        sound("buzz");
        if (e.reason() == PowerException::Reason::NoFreeUnit) emit(EventType::Shake);
        return;
    }
    sound("hum");
    message(s->name() + " online.", Tone::Good);
    emit(EventType::Flash, id);
    if (id == "oxygen" && !flags_.oxygenOnce) {
        flags_.oxygenOnce = true;
        if (!colony_.leakSealed) say("arjun_leak_hint");
    }
    if (id == "purifier") flags_.purifierOnce = true;
    if (data_.dialogue.has("on_" + id)) dialogue_.sayOnce("on_" + id);
}

std::vector<std::pair<std::string, std::string>> Session::tasksFor(const CrewMember& c) const {
    std::vector<std::pair<std::string, std::string>> out;
    if (!c.idle()) return out;
    bool reactorJob = !flags_.reactorOn && (colony_.mainPowerOut || flags_.stormWarned) && flags_.hatchCut;
    bool freeBrace = std::count(braced_.begin(), braced_.end(), false) > 0 && scrap_ >= 2 && flags_.contact;
    if (c.id() == "arjun") {
        if (has("Sealant") && !colony_.leakSealed) out.push_back({"Seal the oxygen leak (2h)", "leak"});
        if (purifierReady() && !colony_.system("purifier")->built()) out.push_back({"Build the purifier (2h)", "purifier"});
        if (freeBrace) out.push_back({"Brace the base (2h)", "brace"});
        if (!colony_.damaged().empty()) out.push_back({"Repair storm damage (2h)", "repair"});
        if (reactorJob) out.push_back({reactorLabel(2), "reactor"});
    }
    if (c.id() == "maya") {
        if (transmitterReadyToBuild() && !colony_.system("comms")->built()) out.push_back({"Build the transmitter (2h)", "tx"});
        if (freeBrace) out.push_back({"Brace the base (4h)", "brace"});
        if (reactorJob) out.push_back({reactorLabel(4), "reactor"});
    }
    if (c.id() == "elena") {
        if (greenhouseReady() && !colony_.system("greenhouse")->built()) out.push_back({"Plant the greenhouse (2h)", "greenhouse"});
        if (colony_.system("greenhouse")->built() && flags_.contact && !flags_.greenhouseCovered && !flags_.stormPassed)
            out.push_back({"Cover the greenhouse (2h)", "cover"});
        if (reactorJob) out.push_back({reactorLabel(4), "reactor"});
    }
    if (c.id() == "hana" && flags_.contact) {
        if (!flags_.hanaPlotted) out.push_back({"Plot the shuttle's descent (2h)", "plot"});
        if (colony_.system("greenhouse")->built() && !flags_.greenhouseCovered && !flags_.stormPassed) out.push_back({"Cover the greenhouse (4h)", "cover"});
    }
    return out;
}

void Session::assignJob(CrewMember& c, const std::string& job) {
    // right skill: 2 hours; wrong skill: 4 hours with a 25% mistake chance (PRD G7)
    int hours = bal_.geti("crew_task_hours") * (rightSkill(c.id(), job) ? 1 : 2);
    c.assign(job, hours);
    if (job == "leak") takeItem("Sealant");
    if (job == "brace") scrap_ -= 2;
    sound("click");
    message(c.name() + " is on it: " + std::to_string(hours) + " hours.", Tone::Good);
    say("assign_" + c.id());
}

void Session::jobFinished(CrewMember& c, const std::string& job) {
    if (!rightSkill(c.id(), job) && rng_.next() < 0.25f) {
        c.assign(job, bal_.geti("crew_task_hours") * 2);
        message(c.name() + " made a mistake and has to start over.", Tone::Warn);
        say("crew_mistake", {c.name()});
        return;
    }
    if (job == "leak") {
        sealLeak(false);
        say("arjun_leak_done");
    } else if (job == "tx") {
        if (transmitterReadyToBuild()) buildTransmitter(false);
        else message("Maya could not finish: some parts are missing.", Tone::Warn);
    } else if (job == "purifier") {
        if (purifierReady()) buildPurifier(false);
        else message("Arjun could not finish the purifier: materials are missing.", Tone::Warn);
    } else if (job == "greenhouse") {
        if (greenhouseReady()) buildGreenhouse(false);
        else message("Elena could not plant: materials are missing.", Tone::Warn);
    } else if (job == "cover") {
        coverGreenhouse(false);
    } else if (job == "brace") {
        for (size_t i = 0; i < braced_.size(); ++i)
            if (!braced_[i]) {
                placeBrace(static_cast<int>(i), false);
                break;
            }
    } else if (job == "repair") {
        repairNext(false);
    } else if (job == "plot") {
        flags_.hanaPlotted = true;
        addTrust(2, true);
        say("hana_plotted");
    } else if (job == "reactor") {
        c.changeHp(-bal_.get("reactor_damage"));
        if (c.alive() && has("Medical stock")) {
            takeItem("Medical stock");
            say("reactor_crew_ok", {c.name()});
        } else if (c.alive()) {
            c.changeHp(-c.hp());   // untreated radiation is fatal
            say("reactor_crew_lost", {c.name()});
        } else {
            say("reactor_crew_lost", {c.name()});
        }
        addTrust(3, true);
        restartReactor(c.id());
    }
}

bool Session::assignFirstTask(int crewIndex) {
    CrewMember& c = colony_.crew()[crewIndex];
    if (!c.alive()) {
        message(c.name() + " is gone.", Tone::Bad);
        return false;
    }
    if (!c.idle()) {
        message(c.state() == CrewState::Working ? c.name() + " is busy: " + std::to_string(c.hoursLeft()) + "h left." : c.name() + " can't work right now.", Tone::Warn);
        sound("buzz");
        return false;
    }
    auto tasks = tasksFor(c);
    if (tasks.empty()) {
        message("No job for " + c.name() + " right now. Bring the materials first.", Tone::Info);
        sound("buzz");
        return false;
    }
    assignJob(c, tasks[0].second);
    return true;
}

void Session::openTalk(int crewIndex) {
    colony_.crew()[crewIndex].face(opposite(player_.facing()));
    sound("click");
    emit(EventType::OpenTalk, "", Tone::Info, crewIndex);
}

std::vector<TalkOption> Session::talkOptions(int crewIndex) {
    std::vector<TalkOption> out;
    CrewMember* c = &colony_.crew()[crewIndex];
    for (const auto& t : tasksFor(*c)) {
        std::string job = t.second;
        out.push_back({t.first, [this, c, job] { assignJob(*c, job); }});
    }
    if (c->id() == "elena" && has("Trauma kit") && (hana().state() == CrewState::Injured || hana().state() == CrewState::Recovering))
        out.push_back({"Give trauma kit", [this] { giveKit(); }});
    out.push_back({"Talk", [this, c] { say("talk_" + c->id() + (c->state() == CrewState::Working ? "_work" : "_idle")); }});
    out.push_back({"Leave", [this] { sound("click"); }});
    return out;
}

