// Session: the hourly story beats, the storm, story actions and the endings
#include "logic/PowerException.h"
#include "logic/Session.h"
#include "logic/SessionShared.h"
#include <algorithm>
#include <cmath>

using namespace session_detail;

// ---------------------------------------------------------------- hours

void Session::tickHour(int h) {
    HourReport rep = colony_.tickHour();
    for (const std::string& b : rep.barsTurnedCritical) {
        sound("alarm");
        say("elena_critical", {b});
    }
    if (rep.transmitterFull && !flags_.contact) say("maya_tx_full");
    for (const std::string& n : rep.deaths) {
        sound("alarm");
        say("elena_lost", {n});
        if (n == "Hana") player_.trail.clear();
    }
    for (const auto& job : rep.jobsDone)
        if (CrewMember* c = colony_.crewById(job.first)) jobFinished(*c, job.second);
    if (rep.hanaRecovered) say("hana_recovered");
    // warn once when anyone gets weak, before the low bars quietly kill them
    for (const CrewMember& c : colony_.crew()) {
        if (!c.alive() || c.state() == CrewState::Missing) continue;
        if (c.hp() < 30 && !weakWarned_.count(c.id())) {
            weakWarned_.insert(c.id());
            sound("alarm");
            say("elena_weak", {c.name()});
        } else if (c.hp() >= 45) {
            weakWarned_.erase(c.id());
        }
    }
    if (rep.collapse) {
        endGame("F3");
        return;
    }
    // Kiran rests in the base
    if (!outside()) player_.health = std::min(100.0f, player_.health + (colony_.isOn("medical") ? bal_.get("kiran_heal") : 1.0f));
    if (player_.health > 40) flags_.healthWarned = false;
    if (colony_.isOn("greenhouse") && colony_.greenhouseGrow == bal_.geti("greenhouse_grow_hours") + 1 && !flags_.firstMeal) {
        flags_.firstMeal = true;
        say("elena_first_meal");
    }

    if (hana().state() == CrewState::Missing && h == 6) say("maya_hana_weak");
    if (h == GameClock::ACT2_HOUR) {
        emit(EventType::ActBanner, "ACT 2", Tone::Info, 0, "THE SIGNAL");
        say("maya_act2");
        emit(EventType::SaveGame);
    }
    if (h == 20 && contactHour_ == 8 && !flags_.podLanded) {
        flags_.podLanded = true;
        world_.setSolidName(World::POD.x, World::POD.y, "Supply Pod");
        emit(EventType::Shake);
        sound("crash");
        say("maya_pod");
    }
    if (!flags_.contact && (h == 8 || h == 16 || h == 24)) {
        toast("SIGNAL WINDOW OPEN - HOUR " + std::to_string(h));
        say(flags_.antenna ? "maya_window_up" : "maya_window_none");
    } else if ((h == 8 + GameClock::WINDOW_LENGTH || h == 16 + GameClock::WINDOW_LENGTH) && !flags_.contact) {
        toast("WINDOW CLOSED");
    } else {
        toast("HOUR " + std::to_string(h));
    }
    if (h >= GameClock::CONTACT_DEADLINE + GameClock::WINDOW_LENGTH && !flags_.contact) {
        endGame("F1");
        return;
    }
    if (h == GameClock::REPORT_HOURS[0] || h == GameClock::REPORT_HOURS[1]) statusReport(h);
    // four hours before each report, Maya says where the colony stands against HALCYON's target
    if (flags_.contact && (h == GameClock::REPORT_HOURS[0] - 4 || h == GameClock::REPORT_HOURS[1] - 4)) {
        float health = colony_.colonyHealth();
        say(health >= reportTarget() ? "maya_report_soon_ok" : "maya_report_soon_low",
            {std::to_string(h + 4), fmt0(health), std::to_string(reportTarget())});
    }
    // a nudge toward the Truth Epilogue
    if (flags_.contact && h == 30 && !story_.count("Flight recorder"))
        say(hana().alive() && hana().state() != CrewState::Missing ? "hana_recorder" : "maya_recorder");
    // autosave every 4 game hours (the menu's Continue and the end screen's Load last save use it)
    // only when it is safe: Kiran inside, Hana not trailing behind him, no storm raging
    if (h % 4 == 0) autosaveDue_ = true;
    if (autosaveDue_ && !ended_ && !colony_.storm && !outside() && hana().state() != CrewState::Following) {
        autosaveDue_ = false;
        emit(EventType::SaveGame);
        toast("AUTOSAVED");
    }
    if (flags_.contact && h == stormWarningHour() && !flags_.stormWarned) {
        flags_.stormWarned = true;
        sound("alarm");
        toast("STORM WARNING");
        say("voss_storm_warning", {std::to_string(GameClock::STORM_HOUR - h)});
        say("arjun_storm_prep");
        emit(EventType::Storm, "warning");
    }
    if (flags_.contact && h == GameClock::STORM_HOUR) stormHits();
    if (flags_.contact && h == GameClock::STORM_END) stormEnds();
    if (flags_.contact && h == GameClock::ACT4_HOUR) {
        flags_.act4 = true;
        world_.openGate();
        emit(EventType::ActBanner, "ACT 4", Tone::Info, 0, "THE LIGHT");
        say("maya_act4");
        emit(EventType::SaveGame);
    }
    if (flags_.contact && h == 70) {
        say("voss_descent");
        say(hanaCanGuide() ? "hana_guide" : "maya_no_pilot");
    }
    if (flags_.contact && h >= GameClock::SHUTTLE_HOUR) {
        if (colony_.isOn("beacon")) {
            finishRescue();
        } else if (h == GameClock::SHUTTLE_HOUR && flags_.report56Met) {
            flags_.grace = true;
            sound("alarm");
            say("voss_grace");
        } else if (h >= GameClock::SHUTTLE_HOUR + (flags_.grace ? 2 : 0)) {
            endGame("F4");
        }
    }
}

void Session::statusReport(int hour) {
    int target = reportTarget();
    float health = colony_.colonyHealth();
    bool met = health >= target;
    if (hour == GameClock::REPORT_HOURS[0]) {
        flags_.report32 = true;
        flags_.report32Met = met;
    } else {
        flags_.report56 = true;
        flags_.report56Met = met;
    }
    toast("STATUS REPORT - HOUR " + std::to_string(hour));
    if (met) say("voss_report_ok", {fmt0(health), std::to_string(target)});
    else say(hour == GameClock::REPORT_HOURS[0] ? "voss_report_fail_32" : "voss_report_fail_56", {fmt0(health), std::to_string(target)});
    if (met && hour == GameClock::REPORT_HOURS[1] && !world_.gateOpen()) {   // a good report opens the landing zone early
        world_.openGate();
        toast("LANDING ZONE GATE OPEN");
        say("voss_gate_early");
    }
}

void Session::stormHits() {
    colony_.storm = true;
    emit(EventType::Storm, "start");
    emit(EventType::Shake);
    sound("crash");
    toast("THE STORM HITS");
    int bracesUp = static_cast<int>(std::count(braced_.begin(), braced_.end(), true));
    std::vector<std::string> broken;
    if (bracesUp < 2) {
        for (const char* id : {"heater", "purifier"})
            if (colony_.system(id)->built()) {
                colony_.damage(id);
                broken.push_back(colony_.system(id)->name());
            }
        if (outside()) hurtKiran(bal_.get("storm_debris_damage"), "Storm debris hit Kiran outside.");
    }
    if (colony_.system("greenhouse")->built() && !flags_.greenhouseCovered) {
        colony_.damage("greenhouse");
        colony_.greenhouseGrow = 0;
        broken.push_back("GREENHOUSE");
    }
    if (flags_.reactorOn) {
        say("storm_reactor");
    } else if (colony_.battery >= bal_.get("battery_safe_level")) {
        say("storm_batteries");
    } else {
        colony_.mainPowerOut = true;
        recomputeUnits();
        say("storm_dark");
    }
    if (!broken.empty()) say("storm_damage", {joinList(broken)});
}

void Session::stormEnds() {
    colony_.storm = false;
    flags_.stormPassed = true;
    emit(EventType::Storm, "end");
    toast("THE STORM HAS PASSED");
    say("storm_ends");
}

// ---------------------------------------------------------------- story actions

void Session::restorePower() {
    takeItem("Power cell");
    colony_.powered = true;
    recomputeUnits();
    clock_.setRunning(true);
    sound("hum");
    emit(EventType::Flash, "core");
    toast("POWER RESTORED - CLOCK STARTED");
    say("maya_lights");
    say("elena_medical_too");
    say("maya_beacon");
}

void Session::sealLeak(bool byKiran) {
    colony_.leakSealed = true;
    addTrust(1, !byKiran);
    sound("ok");
    toast("LEAK SEALED");
    if (byKiran) {
        takeItem("Sealant");
        if (colony_.crew()[1].idle()) dialogue_.sayOnce("hint_arjun");
    }
}

void Session::buildTransmitter(bool byKiran) {
    for (const char* n : {"Antenna dish", "Amplifier", "Large power cell"}) takeItem(n);
    scrap_ -= 2;
    colony_.build("comms");
    colony_.setTransmitter(0);
    addTrust(2, !byKiran);
    if (byKiran) dialogue_.sayOnce("hint_maya");
    sound("ok");
    toast("TRANSMITTER BUILT");
    say("maya_tx_built");
}

void Session::freeHana() {
    hana().setState(CrewState::Following);
    player_.trail.clear();
    for (const IVec& d : world_.debris()) world_.clearSolid(d.x, d.y);
    sound("ok");
    toast("HANA FOUND");
    say("hana_found1");
    say("hana_found2");
}

void Session::deliverHana() {
    hana().setState(CrewState::Injured);
    hana().moveTo({58, 29});
    sound("ok");
    toast("HANA IN MEDICAL");
    say("elena_bed");
}

void Session::giveKit() {
    takeItem("Trauma kit");
    colony_.kitGiven = true;
    sound("ok");
    toast("HANA STABILIZING");
    say(colony_.isOn("medical") ? "elena_kit_ok" : "elena_kit_dark");
}

void Session::placeAntenna() {
    player_.carry().remove("Transmitter");
    flags_.antenna = true;
    sound("ok");
    toast("ANTENNA SET UP");
    int w = clock_.nextWindow();
    say("maya_antenna_up", {std::to_string(w ? w : 24)});
}

void Session::waitForWindow() {
    int w = clock_.nextWindow();
    if (!w) {
        message("No windows left.", Tone::Bad);
        return;
    }
    int hours = std::max(1, static_cast<int>(std::ceil(w - clock_.hour())));
    float cost = hours * bal_.get("wait_suit_per_hour");
    // keep enough air to walk home afterwards: estimate the trip from the real path to the base door
    float reserve = 0;
    if (outside()) {
        auto home = pathfinder_.find(player_.tile(), {IVec{49, 42}, IVec{50, 42}}, [this](IVec a2, IVec b) { return canMove(a2, b); });
        reserve = (home ? home->size() : 120) * bal_.get("suit_step_cost") * 2.0f + 5.0f;   // steps plus zone entries on the way back
    }
    if (outside() && player_.suit - cost < reserve) {
        message("Waiting " + std::to_string(hours) + "h costs " + fmt0(cost) + " air and you need " + fmt0(reserve) + " to get home. Wait inside.", Tone::Bad);
        sound("buzz");
        return;
    }
    if (outside()) player_.suit -= cost;
    for (int h : clock_.skipTo(w + 0.02f)) {
        tickHour(h);
        if (ended_) return;
    }
    toast("WAITED " + std::to_string(hours) + "H");
}

void Session::contactMade() {
    int w = clock_.openWindow();
    if (!w) w = 24;
    flags_.contact = true;
    contactHour_ = w;
    toast("HALCYON ANSWERS");
    say("maya_hear");
    say("voss_" + std::to_string(w));
    if (w == 24) say("maya_memorial");
    say("voss_final");
    if (w == 8) say("voss_pod_promise");
    checkActs();
}

void Session::openPod() {
    flags_.podOpened = true;
    cargo_["Medical stock"]++;
    player_.suitMax = 125;
    player_.suit = std::min(player_.suitMax, player_.suit + 25);
    colony_.battery = std::min(100.0f, colony_.battery + 40);
    sound("pick");
    toast("SUPPLY POD OPENED");
    say("ins_pod_open");
}

void Session::buildPurifier(bool byKiran) {
    ice_ -= 3;
    scrap_ -= 2;
    cables_ -= 1;
    colony_.build("purifier");
    addTrust(1, !byKiran);
    sound("ok");
    toast("PURIFIER BUILT");
    say("arjun_purifier_done");
}

void Session::buildGreenhouse(bool byKiran) {
    takeItem("Seeds");
    scrap_ -= 2;
    cables_ -= 1;
    colony_.build("greenhouse");
    colony_.greenhouseGrow = 0;
    addTrust(1, !byKiran);
    sound("ok");
    toast("GREENHOUSE PLANTED");
    say("elena_greenhouse_done");
}

void Session::coverGreenhouse(bool byKiran) {
    flags_.greenhouseCovered = true;
    addTrust(2, !byKiran);
    sound("ok");
    toast("GREENHOUSE COVERED");
}

void Session::placeBrace(int index, bool byKiran) {
    if (byKiran) scrap_ -= 2;
    braced_[index] = true;
    addTrust(2, !byKiran);
    sound("ok");
    toast("BRACES " + std::to_string(std::count(braced_.begin(), braced_.end(), true)) + "/2");
}

void Session::installCoil() {
    takeItem("Generator coil");
    flags_.coilInstalled = true;
    recomputeUnits();
    sound("hum");
    emit(EventType::Flash, "core");
    toast("+2 POWER UNITS");
    say("arjun_coil");
}

void Session::restartReactor(const std::string& byWho) {
    if (byWho == "kiran") {
        hurtKiran(bal_.get("reactor_damage"), "Radiation from the reactor.");
        addTrust(3, false);
        if (ended_) return;
    }
    flags_.reactorOn = true;
    colony_.mainPowerOut = false;
    recomputeUnits();
    sound("hum");
    emit(EventType::Flash, "core");
    toast("REACTOR ONLINE - POWER RESTORED");
    say("reactor_back");
}

void Session::repairNext(bool byKiran) {
    if (colony_.damaged().empty()) return;
    std::string id = *colony_.damaged().begin();
    colony_.repair(id);
    addTrust(1, !byKiran);
    sound("ok");
    toast(colony_.system(id)->name() + " REPAIRED");
}

void Session::buildBeacon() {
    takeItem("Nav computer");
    scrap_ -= 3;
    cables_ -= 2;
    colony_.build("beacon");
    sound("hum");
    toast("BEACON BUILT");
    say("beacon_built");
}

void Session::finishRescue() {
    bool guided = hanaCanGuide() || flags_.shuttleGuided;
    summary_.roughLanding = !guided || lightsPlaced() < 3;
    int trust = colony_.trust.percent();
    int alive = colony_.crewAlive();
    std::string code = trust < 50 ? "E3" : alive <= 2 ? "E2" : "E1";
    endGame(code);
}

void Session::endGame(const std::string& code, const std::string& reason) {
    if (ended_) return;
    ended_ = true;
    summary_.code = code;
    summary_.reason = reason;
    summary_.hours = clock_.hour();
    summary_.crewAlive = colony_.crewAlive();
    summary_.systemsBuilt = 0;
    for (const auto& s : colony_.systems())
        if (s->built()) ++summary_.systemsBuilt;
    summary_.trustPercent = colony_.trust.percent();
    // the recorder is the proof; Hana's account (or the flight log) backs it up at the inquiry
    summary_.truth = story_.count("Flight recorder") > 0 &&
                     ((hana().alive() && hana().state() != CrewState::Missing) || story_.count("Flight log") > 0);
    summary_.contactHour = contactHour_;
    summary_.reportsMet = (flags_.report32Met ? 1 : 0) + (flags_.report56Met ? 1 : 0);
    emit(EventType::Ending, code);
}

