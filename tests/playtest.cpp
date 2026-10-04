// Automated playtester. Plays STRANDED through the real rules the way a person would: it follows the
// Next Goal arrow, walks every step at the default clock speed (4x), takes human-like time to read
// dialogue and to solve puzzles (and sometimes fails them), manages power from the Power Panel and
// talks to crew to hand out jobs. Each scenario aims for one ending and prints a timeline.
#include "logic/PowerException.h"
#include "logic/Session.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <string>

namespace {

struct Policy {
    std::string name;
    bool delegate = true;        // hand jobs to the crew
    bool careSuit = true;        // head back before the suit runs dry
    bool managePower = true;     // juggle units from the Power Panel
    bool explore = true;         // pick up optional items (medical stock, thermal layer, flight recorder, generator)
    bool giveKit = true;
    bool buildBeacon = true;
    bool guide = true;           // play the guide-in puzzle if no pilot
    bool hanaPlots = true;
    bool contact = true;         // F1 test turns this off
    bool oxygen = true;          // F3 test turns this off
    bool wander = false;         // F2: wander deep outside and ignore the suit
    bool reactorBySelf = false;  // restart the reactor personally instead of sending a crew member
    float skill = 0.85f;         // chance to solve an easy puzzle on a try
    unsigned seed = 1;
    float clock = 2.0f;          // game clock multiplier (2 = default, 1 = the documented 60 s per hour)
};

class Bot {
public:
    Bot(Session& s, Policy p) : s_(s), p_(std::move(p)), rng_(p_.seed) {}

    void run(float realLimit = 3 * 3600) {
        std::printf("\n=== %s ===\n", p_.name.c_str());
        lastQuest_ = -1;
        while (!s_.ended() && real_ < realLimit) step();
        report();
    }

    std::string code() const { return s_.ended() ? s_.summary().code : "none"; }

private:
    Session& s_;
    Policy p_;
    Rng rng_;
    float real_ = 0;
    int lastQuest_ = -1, puzzles_ = 0, puzzleFails_ = 0, stuckNotes_ = 0, readLines_ = 0;
    float questSince_ = 0;
    float minSuit_ = 100, minHp_ = 100, minBar_[4] = {100, 100, 100, 100};
    std::map<std::string, int> notes_;
    std::vector<std::string> timeline_;

    float hour() const { return s_.clock().hour(); }
    void note(const std::string& what) {
        char buf[300];
        std::snprintf(buf, sizeof buf, "  H%05.2f  (%4.0f min real)  %s", hour(), real_ / 60, what.c_str());
        std::puts(buf);
        std::fflush(stdout);
    }
    void noteOnce(const std::string& what) { if (notes_[what]++ == 0) note(what); }

    // ---- one frame of play
    void frame(float dt = 0.05f, bool ui = false) {
        bool holding = s_.holdProgress() >= 0;
        s_.update(dt, std::nullopt, holding, ui, false, p_.clock);
        real_ += dt;
        if (s_.player().suit < minSuit_ && s_.outside()) minSuit_ = s_.player().suit;
        minHp_ = std::min(minHp_, s_.player().health);
        for (int i = 0; i < 4; ++i) minBar_[i] = std::min(minBar_[i], s_.colony().bar(static_cast<Bar>(i)).value());
        events();
        read();
    }
    void wait(float seconds, bool ui = false) { for (float t = 0; t < seconds && !s_.ended(); t += 0.05f) frame(0.05f, ui); }
    void waitHours(float h) { float end = hour() + h, realEnd = real_ + h * (60 / p_.clock) * 1.5f + 1; while (hour() < end && real_ < realEnd && !s_.ended()) frame(); }

    void read() {   // a person reads each line; key lines stop the clock until dismissed
        while (!s_.dialogue().empty() && !s_.ended()) {
            const DialogueLine& l = s_.dialogue().current();
            float t = 0.8f + l.text.size() * 0.03f;
            bool block = s_.dialogue().blocking();
            if (block) { real_ += t; ++readLines_; }
            if (l.text.find("lost") != std::string::npos || l.text.find("died") != std::string::npos) note("DIALOGUE: " + l.text.substr(0, 110));
            s_.dialogue().pop();
            if (!block) break;
        }
    }

    void events() {
        SessionEvent e;
        while (s_.pollEvent(e)) {
            switch (e.type) {
                case EventType::StartPuzzle: puzzle(); break;
                case EventType::ActBanner: note("== " + e.text + " " + e.text2 + " =="); break;
                case EventType::Message:
                    if (e.tone == Tone::Bad || e.tone == Tone::Warn) noteOnce("msg: " + e.text);
                    break;
                case EventType::Storm: note("storm: " + e.text); break;
                case EventType::Ending: note("ENDING " + s_.summary().code); break;
                default: break;
            }
        }
    }

    void puzzle() {
        PuzzleKind k = s_.pendingPuzzle();
        int d = s_.pendingDifficulty();
        float limit = s_.pendingTimeLimit() > 0 ? s_.pendingTimeLimit() : d == 0 ? 90.0f : d == 1 ? 60.0f : 40.0f;
        static const float base[3] = {16, 24, 32};
        float need = base[std::min(d, 2)] * (0.7f + rng_.next() * 0.6f);
        bool ok = rng_.next() < p_.skill - 0.12f * d && need < limit;
        ++puzzles_;
        static const char* names[] = {"none", "Wire", "Pipe", "Tune", "Sequence", "Cut"};
        float h0 = hour();
        // the clock keeps running while the puzzle is open (FR-8)
        for (float t = 0; t < std::min(need, limit) && !s_.ended(); t += 0.05f) {
            s_.update(0.05f, std::nullopt, false, true, false, p_.clock);
            real_ += 0.05f;
        }
        if (s_.ended()) return;
        if (!ok) ++puzzleFails_;
        char buf[120];
        std::snprintf(buf, sizeof buf, "puzzle %s d%d: %s after %.0fs (%.2f game h)", names[static_cast<int>(k)], d, ok ? "solved" : "FAILED", need, hour() - h0);
        note(buf);
        s_.puzzleFinished(ok);
    }

    // ---- movement
    bool busy() const { return s_.player().moving() || !s_.player().path.empty() || s_.player().pendingTarget.has_value() || s_.holdProgress() >= 0; }
    void go(IVec t) {
        s_.clickTile(t);
        for (int i = 0; i < 20 * 120 && !s_.ended(); ++i) {
            frame();
            bool almostThere = manhattan(s_.player().tile(), t) <= 5;   // finish the pickup first
            if (p_.careSuit && s_.outside() && suitLow() && !returning_ && !almostThere) { goHome(); return; }
            if (!busy()) break;
        }
    }
    bool returning_ = false;
    bool suitLow() const {
        IVec p = s_.player().tile();
        float dist = static_cast<float>(manhattan(p, {50, 43}));
        return s_.player().suit < dist * 0.4f * (s_.heavyLoad() ? 1.5f : 1.0f) + 10;
    }
    void goHome() {
        returning_ = true;
        noteOnce("suit low: turning back to the base");
        s_.clickTile({50, 40});
        for (int i = 0; i < 20 * 120 && busy() && !s_.ended(); ++i) frame();
        wait(2);
        returning_ = false;
    }
    void talk(int crew, const std::string& word) {
        const CrewMember& m = s_.colony().crew()[crew];
        if (!m.alive()) return;
        go(m.tile());
        wait(1.5f, true);   // open the talk menu and pick the job
        for (auto& o : s_.talkOptions(crew))
            if (o.label.find(word) != std::string::npos) {
                o.run();
                note("asked " + m.name() + ": " + o.label);
                read();
                return;
            }
    }
    bool crewFree(int i) const { const CrewMember& c = s_.colony().crew()[i]; return c.alive() && c.state() == CrewState::Idle; }
    bool hasOption(int crew, const std::string& word) {
        for (auto& o : s_.talkOptions(crew)) if (o.label.find(word) != std::string::npos) return true;
        return false;
    }

    // ---- the Power Panel, as a player would juggle it
    void power() {
        if (!p_.managePower) return;
        Colony& c = s_.colony();
        if (!c.powered || c.mainPowerOut) return;
        const StoryFlags& f = s_.flags();
        std::vector<std::string> want;
        auto add = [&](const std::string& id) { if (std::find(want.begin(), want.end(), id) == want.end() && c.system(id)->built() && !c.isDamaged(id)) want.push_back(id); };
        bool charging = !f.contact && !f.transmitterTaken && c.transmitter() < 100 && c.system("comms")->built();
        CrewState hs = s_.hana().state();
        if (charging && c.bar(Bar::Oxygen).value() > 30) {   // O2 can coast for a few hours; Hana cannot
            add("comms");
            if ((hs == CrewState::Injured || hs == CrewState::Recovering) && c.kitGiven) add("medical");
        }
        if (p_.oxygen) add("oxygen");
        if ((hs == CrewState::Injured || hs == CrewState::Recovering) && c.kitGiven) add("medical");
        if (!f.contact && !f.transmitterTaken && c.transmitter() < 100) add("comms");
        if (c.system("beacon")->built() && hour() > 66) add("beacon");
        if (c.bar(Bar::Warmth).value() < 50 || c.storm) add("heater");
        if (c.bar(Bar::Water).value() < 70) add("purifier");
        if (c.bar(Bar::Food).value() < 75) add("greenhouse");
        if (s_.player().health < 70) add("medical");
        add("heater");
        add("purifier");
        add("greenhouse");
        int units = c.powerUnits();
        // before the storm keep two units free so the batteries charge (unless the reactor is already back)
        if (hour() >= 38 && hour() < 48 && !f.reactorOn && units >= 4 && c.battery < 75) units -= 2;
        if (static_cast<int>(want.size()) > units) want.resize(units);
        for (const auto& sys : c.systems())
            if (sys->on() && std::find(want.begin(), want.end(), sys->id()) == want.end()) s_.toggleSystem(sys->id());
        for (const auto& id : want)
            if (!c.isOn(id)) s_.toggleSystem(id);
    }

    void ensureOn(const std::string& id) {   // what a player does without planning: flip it on, switch something else off if refused
        Colony& c = s_.colony();
        if (c.isOn(id)) return;
        s_.toggleSystem(id);
        for (const auto& sys : c.systems())
            if (!c.isOn(id) && sys->on() && sys->id() != "oxygen") { s_.toggleSystem(sys->id()); s_.toggleSystem(id); }
    }

    // ---- one decision
    float stallReal_ = 0, stallHour_ = -1;
    void step() {
        if (hour() != stallHour_ || !s_.clock().running()) { stallHour_ = hour(); stallReal_ = real_; }
        else if (s_.clock().running() && real_ - stallReal_ > 120) {
            int scrapLeft = 0, cablesLeft = 0;
            for (const WorldItem& it : s_.items()) if (!it.taken) { scrapLeft += it.name == "Scrap"; cablesLeft += it.name == "Cables"; }
            char buf[300];
            std::snprintf(buf, sizeof buf, "STALL: clock not moving. goal '%s', at (%d,%d), scrap %d (map %d), cables %d (map %d), dialogue '%s'", s_.goalText().c_str(),
                          s_.player().tile().x, s_.player().tile().y, s_.scrap(), scrapLeft, s_.cables(), cablesLeft,
                          s_.dialogue().empty() ? "" : s_.dialogue().current().text.substr(0, 60).c_str());
            note(buf);
            stallReal_ = real_;
            real_ += 3600;   // give up on this run
            return;
        }
        power();
        const QuestLog& q = s_.quests();
        const Objective* o = q.current();
        if (q.currentIndex() != lastQuest_) {
            if (lastQuest_ >= 0) note("goal done -> " + (o ? o->text() : std::string("(none)")));
            lastQuest_ = q.currentIndex();
            questSince_ = hour();
        }
        if (!o) { frame(); return; }
        if (hour() - questSince_ > 6 && stuckNotes_ < 12) {
            ++stuckNotes_;
            note("SLOW: same goal for 6+ game hours: " + o->text());
            questSince_ = hour();
        }
        std::string t = s_.goalText();   // what the Next Goal box shows
        auto has = [&](const char* w) { return t.find(w) != std::string::npos; };

        if (p_.wander) { wanderOutside(); return; }
        if (!p_.oxygen && !s_.colony().powered) { go({55, 39}); go({44, 34}); return; }
        if (!p_.oxygen) { waitHours(1); return; }
        if (!p_.contact && s_.colony().powered && s_.colony().leakSealed) { waitHours(1); return; }

        // optional exploration once the base is stable
        if (p_.explore && s_.flags().contact) exploreOnce();
        if (p_.explore && !s_.flags().generator && s_.colony().kitGiven && triedGenerator_ < 2) {
            ++triedGenerator_;   // +1 power unit at the ARK-7 wreck: worth the detour in Act 2
            go(World::GENERATOR);
            note(s_.flags().generator ? "repaired the wreck generator (+1 unit)" : "generator detour failed");
            return;
        }

        // crew jobs the goal suggests
        if (p_.delegate) {
            if (has("give the job to Arjun") && crewFree(1)) { talk(1, "leak"); return; }
            if (has("or ask Maya") && crewFree(0) && s_.transmitterReadyToBuild()) { talk(0, "transmitter"); return; }
            if (has("or ask Arjun") && crewFree(1) && s_.purifierReady()) { talk(1, "purifier"); return; }
            if (has("Or ask Elena") && crewFree(2) && s_.greenhouseReady()) { talk(2, "greenhouse"); return; }
        }
        if (has("Switch OXYGEN on")) { wait(2, true); s_.toggleSystem("oxygen"); return; }
        if (has(" is critical! Switch ")) {   // the emergency prompt names the system to switch on
            size_t a = t.find("Switch ") + 7, b = t.find(" on", a);
            std::string id = t.substr(a, b - a);
            for (char& ch : id) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
            wait(2, true);
            ensureOn(id);
            noteOnce("emergency prompt: " + t);
            return;
        }
        if (has("Give Elena the trauma kit")) {
            if (!p_.giveKit) { waitHours(1); return; }
            if (!s_.colony().isOn("medical")) { s_.toggleSystem("medical"); power(); }
            talk(2, "trauma kit");
            return;
        }
        if (has("Power COMMS") || has("Power the PURIFIER")) {
            wait(2, true);
            if (p_.managePower) power();
            else ensureOn(has("COMMS") ? "comms" : "purifier");
            waitHours(has("COMMS") ? 0.5f : 0.3f);
            return;
        }
        if (has("Next window: Hour") || has("Align the antenna at the next window")) {
            int w = s_.clock().nextWindow();
            go(World::ANTENNA);   // Z at the antenna waits for the window if the suit allows it
            if (!s_.clock().openWindow() && w) {
                noteOnce("cannot wait at the antenna: going inside until the window");
                home();
                waitUntil([&] { return hour() >= w - 1.2f; }, 12);
                go(World::ANTENNA);
            }
            return;
        }
        if (has("Ride out the storm") || has("gate opens at Hour 60")) { home(); waitHours(0.5f); return; }
        if (has("Storm at Hour 48")) { prepareStorm(); return; }
        if (has("Storm prep")) { stormPrep(); return; }
        if (has("Main power is out")) {
            if (!p_.reactorBySelf && p_.delegate && crewFree(1)) { talk(1, "reactor"); return; }
        }
        if (has("Power the BEACON")) {
            if (!p_.buildBeacon) { home(); waitHours(0.5f); return; }
            wait(2, true);
            if (!s_.colony().isOn("beacon")) { s_.toggleSystem("beacon"); if (!s_.colony().isOn("beacon")) { s_.colony().switchOff("greenhouse"); s_.toggleSystem("beacon"); } }
            home();
            waitHours(0.5f);
            return;
        }
        if (has("guide the shuttle")) {
            if (!p_.guide) { home(); waitHours(0.5f); return; }
            if (hour() < 70) { go({World::BEACON_SITE.x, World::BEACON_SITE.y + 2}); waitUntil([&] { return hour() >= 70; }, 3); return; }
        }
        if (has("Keep everyone alive")) { home(); waitHours(0.5f); return; }
        if (!p_.buildBeacon && (has("beacon") || has("nav computer") || has("landing lights") || has("landing pad"))) { home(); waitHours(0.5f); return; }
        if (has("Plant the greenhouse") || has("Build the purifier") || has("Build the beacon")) { if (!resourcesFor(t)) return; }
        if (p_.hanaPlots && p_.delegate && s_.flags().contact && !s_.flags().hanaPlotted && crewFree(3) && hasOption(3, "Plot")) { talk(3, "Plot"); return; }

        auto target = s_.goalTarget();
        if (!target) { frame(); waitHours(0.25f); return; }
        float h0 = hour();
        int idx = q.currentIndex();
        go(*target);
        if (!busy() && q.currentIndex() == idx && hour() - h0 < 0.01f) {
            // nothing happened: probably missing materials or a closed window
            noteOnce("no progress clicking goal target: " + t);
            if (!gatherForGoal(t)) waitHours(0.25f);
        }
    }

    template <class F> void waitUntil(F cond, float maxHours) {
        float end = hour() + maxHours;
        float realEnd = real_ + maxHours * (60 / p_.clock) * 1.5f + 1;
        while (!cond() && hour() < end && real_ < realEnd && !s_.ended()) frame();
    }
    void home() { if (s_.outside()) { s_.clickTile({50, 36}); for (int i = 0; i < 2400 && busy() && !s_.ended(); ++i) frame(); } }

    // scavenge the nearest pickups of a kind (a player who reads "scrap 0/2" goes looking)
    bool gatherNearest(const std::string& kind, int count) {
        bool any = false;
        for (int n = 0; n < count && !s_.ended(); ++n) {
            const WorldItem* best = nullptr;
            int bd = 1 << 30;
            for (const WorldItem& it : s_.items())
                if (!it.taken && it.name == kind) {
                    int d = manhattan(it.tile, s_.player().tile());
                    if (d < bd) { bd = d; best = &it; }
                }
            if (!best) { noteOnce("no " + kind + " left on the map"); return any; }
            IVec tile = best->tile;
            go(tile);
            if (s_.player().suit < 40 && s_.outside()) goHome();
            any = true;
        }
        home();
        return any;
    }
    bool gatherForGoal(const std::string& t) {
        if (t.find("scrap") != std::string::npos || t.find("Scrap") != std::string::npos || t.find("brace") != std::string::npos) return gatherNearest("Scrap", 3);
        if (t.find("cable") != std::string::npos || t.find("lights") != std::string::npos) return gatherNearest("Cables", 3);
        return false;
    }
    bool resourcesFor(const std::string& t) {
        int needScrap = t.find("beacon") != std::string::npos ? 3 : 2, needCables = t.find("beacon") != std::string::npos ? 2 : 1;
        if (s_.scrap() < needScrap) { noteOnce("goal needs scrap the arrow does not point to: " + t); gatherNearest("Scrap", needScrap - s_.scrap()); return false; }
        if (s_.cables() < needCables) { noteOnce("goal needs cables the arrow does not point to: " + t); gatherNearest("Cables", needCables - s_.cables()); return false; }
        return true;
    }

    void stormPrep() {
        if (s_.scrap() < 2 && std::count(s_.braces().begin(), s_.braces().end(), false) > 0) { noteOnce("braces need scrap"); gatherNearest("Scrap", 4); return; }
        if (p_.delegate) {
            for (size_t i = 0; i < s_.braces().size(); ++i)
                if (!s_.braces()[i] && crewFree(1) && hasOption(1, "Brace")) { talk(1, "Brace"); return; }
            if (!s_.flags().greenhouseCovered && s_.colony().system("greenhouse")->built()) {
                if (crewFree(2) && hasOption(2, "Cover")) { talk(2, "Cover"); return; }
            }
            if (s_.colony().crew()[1].state() == CrewState::Working) {   // Arjun is busy: do the other brace yourself
                for (size_t i = 0; i < s_.braces().size(); ++i)
                    if (!s_.braces()[i] && s_.colony().crew()[1].job() != "brace") { go(s_.world().bracePoints()[i]); return; }
                waitHours(0.5f);
                return;
            }
        }
        for (size_t i = 0; i < s_.braces().size(); ++i)
            if (!s_.braces()[i]) { go(s_.world().bracePoints()[i]); return; }
        go({56, 33});
    }
    void prepareStorm() {
        // restart the reactor before Hour 48 if possible; otherwise sit tight and let the batteries charge
        if (!s_.flags().reactorOn && s_.flags().hatchCut && s_.flags().stormWarned && s_.colony().battery < 70) {
            if (!p_.reactorBySelf && p_.delegate && crewFree(1) && hasOption(1, "reactor")) { talk(1, "reactor"); return; }
            if (p_.reactorBySelf) { go(World::REACTOR); home(); return; }
        }
        home();
        waitHours(0.5f);
    }

    bool explored_ = false;
    int triedGenerator_ = 0;
    void exploreOnce() {
        if (explored_ || !s_.flags().act3) return;
        explored_ = true;
        note("exploring for optional items");
        for (const char* n : {"Medical stock", "Thermal layer", "Flight recorder"}) {
            for (const WorldItem& it : s_.items())
                if (!it.taken && it.name == n) {
                    IVec tile = it.tile;
                    go(tile);
                    if (s_.has(n) || s_.hasTool(n) || s_.storyItems().count(n)) note(std::string("found optional: ") + n);
                    else noteOnce(std::string("could not get optional: ") + n);
                    home();
                    break;
                }
        }
        if (!s_.flags().generator) { go(World::GENERATOR); home(); }
        gatherNearest("Scrap", std::max(0, 10 - s_.scrap()));
        gatherNearest("Cables", std::max(0, 9 - s_.cables()));
    }

    int wanderLeg_ = 0;
    void wanderOutside() {
        if (!s_.colony().powered) { go({55, 39}); go({44, 34}); s_.toggleSystem("oxygen"); return; }
        const IVec spots[] = {{14, 46}, {7, 61}, {80, 63}, {51, 6}};
        go(spots[wanderLeg_++ % 4]);
    }

    void report() {

        std::printf("  -- result: %s", s_.ended() ? s_.summary().code.c_str() : "DID NOT END");
        if (s_.ended()) {
            const EndSummary& e = s_.summary();
            std::printf(" (%s) hours %.1f, crew alive %d/4, systems %d, trust %d%%, truth %s, rough %s, contact H%d, reports %d/2",
                        e.reason.c_str(), e.hours, e.crewAlive, e.systemsBuilt, e.trustPercent, e.truth ? "yes" : "no", e.roughLanding ? "yes" : "no",
                        e.contactHour, e.reportsMet);
        }
        std::printf("\n  -- play time %.0f min real, puzzles %d (%d failed), key lines read %d, lowest suit %.0f, lowest Kiran hp %.0f\n", real_ / 60, puzzles_,
                    puzzleFails_, readLines_, minSuit_, minHp_);
        std::printf("  -- lowest bars: O2 %.0f  Warmth %.0f  Water %.0f  Food %.0f\n", minBar_[0], minBar_[1], minBar_[2], minBar_[3]);
    }
};

}  // namespace

#include <filesystem>
int main(int argc, char** argv) {
    // run from the project folder or from build: use the project folder either way
    if (!std::filesystem::exists("tests") && std::filesystem::exists("../tests")) std::filesystem::current_path("..");
    DataStore data;
    data.loadAll("data");
    std::string only = argc > 1 ? argv[1] : "";
    std::vector<Policy> runs;
    Policy best; best.name = "E1 attempt: careful, delegates, explores"; runs.push_back(best);
    Policy solo = best; solo.name = "E3 attempt: does everything alone"; solo.delegate = false; solo.seed = 2; runs.push_back(solo);
    Policy e2 = best; e2.name = "E2 attempt: no trauma kit for Hana, no medical stock"; e2.giveKit = false; e2.explore = false; e2.seed = 3; runs.push_back(e2);
    Policy rough = best; rough.name = "Rough landing: no plot, no guide"; rough.hanaPlots = false; rough.guide = false; rough.seed = 4; runs.push_back(rough);
    Policy f4 = best; f4.name = "F4 attempt: never builds the beacon"; f4.buildBeacon = false; f4.seed = 5; runs.push_back(f4);
    Policy f1; f1.name = "F1 attempt: never contacts HALCYON"; f1.contact = false; f1.seed = 6; runs.push_back(f1);
    Policy f2; f2.name = "F2 attempt: wanders the wilds, ignores the suit"; f2.wander = true; f2.careSuit = false; f2.seed = 7; runs.push_back(f2);
    Policy f3; f3.name = "F3 attempt: never turns oxygen on"; f3.oxygen = false; f3.managePower = false; f3.seed = 8; runs.push_back(f3);
    Policy casual; casual.name = "Casual player: follows the arrow, no power juggling, no suit care, clumsy at puzzles";
    casual.managePower = false; casual.careSuit = false; casual.explore = false; casual.skill = 0.6f; casual.seed = 9; runs.push_back(casual);
    Policy self = best; self.name = "Self reactor: restarts the reactor personally"; self.reactorBySelf = true; self.seed = 10; runs.push_back(self);
    Policy slow = best; slow.name = "E1 attempt at the documented 1x clock (60 s per hour)"; slow.clock = 1.0f; slow.seed = 11; runs.push_back(slow);
    Policy fast = best; fast.name = "E1 attempt at 4x clock (old default)"; fast.clock = 4.0f; fast.seed = 12; runs.push_back(fast);

    std::map<std::string, int> got;
    for (size_t i = 0; i < runs.size(); ++i) {
        if (!only.empty() && std::to_string(i) != only) continue;
        Session s(data);
        Bot b(s, runs[i]);
        b.run();
        got[b.code()]++;
    }
    std::printf("\nEndings reached:");
    for (const auto& kv : got) std::printf("  %s x%d", kv.first.c_str(), kv.second);
    std::printf("\n");
}
