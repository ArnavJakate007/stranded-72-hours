// Session: the quest chain and what the Next Goal box shows
#include "logic/PowerException.h"
#include "logic/Session.h"
#include "logic/SessionShared.h"
#include <algorithm>
#include <cmath>

using namespace session_detail;

void Session::buildQuests() {
    quests_.clear();
    auto add = [this](int act, std::function<std::string()> text, std::function<bool()> done, std::function<std::optional<IVec>()> target) {
        quests_.add(Objective{act, std::move(text), std::move(done), std::move(target)});
    };
    auto fixed = [](IVec v) { return [v]() -> std::optional<IVec> { return v; }; };
    auto txt = [](const char* s) { return [s] { return std::string(s); }; };
    auto commsBuilt = [this] { return colony_.system("comms")->built(); };

    // ---- Act 1: Dark
    add(1, txt("Find the power cell in Storage (south-east room)."), [this] { return colony_.powered || has("Power cell"); }, [this] { return itemTile("Power cell"); });
    add(1, txt("Bring the cell to the Power Core (west room)."), [this] { return colony_.powered; }, fixed({43, 34}));
    add(1, txt("Open the Power Panel. Switch OXYGEN on."), [this] { return flags_.oxygenOnce; }, fixed({43, 35}));
    add(1, txt("Get sealant from the Medical Bay cabinet."), [this] { return flags_.sealantTaken; }, fixed({58, 27}));
    add(1,
        [this] {
            const CrewMember& a = colony_.crew()[1];
            if (a.job() == "leak") return "Arjun is sealing the leak: " + std::to_string(a.hoursLeft()) + "h left.";
            return std::string("Seal the O2 Plant leak, or give the job to Arjun.");
        },
        [this] { return colony_.leakSealed; },
        [this]() -> std::optional<IVec> { if (colony_.crew()[1].job() == "leak") return std::nullopt; return World::LEAK; });
    add(1, txt("Hana is missing. Take the cutter from Storage."), [this] { return hasTool("Cutter") || hana().state() != CrewState::Missing; }, fixed({55, 37}));
    add(1, txt("Follow Hana's beacon to the Wreck Field (SE)."), [this] { return hana().state() != CrewState::Missing; }, fixed(World::HANA_TRAP));
    add(1, txt("Bring Hana to the Medical Bay. She follows you."), [this] { return hana().state() != CrewState::Missing && hana().state() != CrewState::Following; }, fixed({57, 30}));
    add(1, txt("Find the trauma kit in the ARK-7 cargo bay."), [this] { return colony_.kitGiven || has("Trauma kit") || !hana().alive(); }, [this] { return itemTile("Trauma kit"); });
    add(1, txt("Give Elena the trauma kit. Keep MEDICAL on."), [this] { return colony_.kitGiven || !hana().alive(); }, fixed({56, 30}));

    // ---- Act 2: The Signal
    add(2, txt("Get the antenna dish from the ARK-7 cargo bay."), [this, commsBuilt] { return commsBuilt() || has("Antenna dish"); }, [this] { return itemTile("Antenna dish"); });
    add(2, txt("Find the amplifier in the Ice Cave (west)."), [this, commsBuilt] { return commsBuilt() || has("Amplifier"); }, [this] { return itemTile("Amplifier"); });
    add(2, txt("Find the large power cell, deep in the Ice Cave."), [this, commsBuilt] { return commsBuilt() || has("Large power cell"); }, [this] { return itemTile("Large power cell"); });
    add(2, [this] { return "Pick up scrap for the frame: " + std::to_string(std::min(2, scrap_)) + " of 2."; },
        [this, commsBuilt] { return commsBuilt() || scrap_ >= 2; }, [this] { return itemTile("Scrap"); });
    add(2,
        [this] {
            const CrewMember& m = colony_.crew()[0];
            if (m.job() == "tx") return "Maya is building the transmitter: " + std::to_string(m.hoursLeft()) + "h left.";
            return std::string("Build the transmitter at Comms, or ask Maya.");
        },
        commsBuilt, [this]() -> std::optional<IVec> { if (colony_.crew()[0].job() == "tx") return std::nullopt; return IVec{43, 29}; });
    add(2, [this] { return "Power COMMS to charge the transmitter: " + std::to_string(static_cast<int>(colony_.transmitter()) / 25) + " of 4 h."; },
        [this] { return colony_.transmitter() >= 100.0f || flags_.contact; },
        [this]() -> std::optional<IVec> { if (colony_.isOn("comms")) return std::nullopt; return IVec{43, 35}; });
    add(2, txt("Take the charged transmitter from Comms."), [this] { return flags_.transmitterTaken || flags_.contact; }, fixed({43, 29}));
    add(2, txt("Carry the transmitter to the Ridge summit (N)."), [this] { return flags_.antenna || flags_.contact; }, fixed(World::ANTENNA));
    add(2,
        [this] {
            if (clock_.openWindow()) return std::string("The window is open. Align the antenna now!");
            int w = clock_.nextWindow();
            if (w && w - clock_.hour() > 2) return "Next window: Hour " + std::to_string(w) + ". Wait inside, or wait at the antenna (Z).";
            return "Align the antenna at the next window: Hour " + std::to_string(w ? w : 24) + ".";
        },
        [this] { return flags_.contact; }, fixed(World::ANTENNA));

    // ---- Act 3: Holding On
    add(3, [this] { return "Bring 3 ice from the Ice Cave entrance: " + std::to_string(std::min(3, ice_)) + " of 3."; },
        [this] { return ice_ >= 3 || colony_.system("purifier")->built(); }, [this] { return itemTile("Ice"); });
    add(3,
        [this] {
            const CrewMember& a = colony_.crew()[1];
            if (a.job() == "purifier") return "Arjun is building the purifier: " + std::to_string(a.hoursLeft()) + "h left.";
            if (scrap_ < 2 || cables_ < 1) return "Purifier needs scrap " + std::to_string(scrap_) + "/2, cable " + std::to_string(cables_) + "/1. Follow the arrow.";
            return std::string("Build the purifier in Storage, or ask Arjun.");
        },
        [this] { return colony_.system("purifier")->built(); },
        [this]() -> std::optional<IVec> { if (colony_.crew()[1].job() == "purifier") return std::nullopt; return materialTarget(2, 1, IVec{57, 40}); });
    add(3, txt("Power the PURIFIER to refill the water."), [this] { return flags_.purifierOnce; }, fixed({43, 35}));
    add(3, txt("Get the seeds from the ARK-7 cargo bay."), [this] { return has("Seeds") || colony_.system("greenhouse")->built(); }, [this] { return itemTile("Seeds"); });
    add(3,
        [this] {
            const CrewMember& e = colony_.crew()[2];
            if (e.job() == "greenhouse") return "Elena is planting the greenhouse: " + std::to_string(e.hoursLeft()) + "h left.";
            if (scrap_ < 2 || cables_ < 1) return "Greenhouse needs scrap " + std::to_string(scrap_) + "/2, cable " + std::to_string(cables_) + "/1. Follow the arrow.";
            return std::string("Plant the greenhouse with the seeds, or ask Elena.");
        },
        [this] { return colony_.system("greenhouse")->built(); },
        [this]() -> std::optional<IVec> { if (colony_.crew()[2].job() == "greenhouse") return std::nullopt; return materialTarget(2, 1, IVec{56, 33}); });
    add(3, txt("Cut into the reactor room (deep Wreck Field)."), [this] { return flags_.hatchCut; }, fixed({92, 58}));
    add(3, txt("Carry the generator coil to the Power Core."), [this] { return flags_.coilInstalled; },
        [this]() -> std::optional<IVec> { if (has("Generator coil")) return IVec{43, 34}; return itemTile("Generator coil"); });
    add(3,
        [this] {
            int b = static_cast<int>(std::count(braced_.begin(), braced_.end(), true));
            return "Storm prep: brace the base (" + std::to_string(b) + "/2, 2 scrap each) and cover the greenhouse.";
        },
        [this] { return flags_.stormPassed || (std::count(braced_.begin(), braced_.end(), true) == 2 && (flags_.greenhouseCovered || !colony_.system("greenhouse")->built())); },
        [this]() -> std::optional<IVec> {
            for (size_t i = 0; i < braced_.size(); ++i)
                if (!braced_[i]) return materialTarget(2, 0, world_.bracePoints()[i]);
            return IVec{56, 33};
        });
    add(3,
        [this] {
            if (colony_.mainPowerOut && colony_.storm) return std::string("Main power is out! Send a crew member to the reactor, or go yourself after Hour 52.");
            if (colony_.mainPowerOut) return std::string("Main power is out! Restart the reactor, or send a crew member.");
            if (colony_.storm) return std::string("Ride out the storm until Hour 52. Stay inside.");
            return "Storm at Hour 48. Keep units free: batteries " + fmt0(colony_.battery) + "% (70% is safe).";
        },
        [this] { return flags_.stormPassed && !colony_.mainPowerOut; },
        [this]() -> std::optional<IVec> { if (colony_.mainPowerOut && !colony_.storm) return World::REACTOR; return std::nullopt; });
    add(3, [this] { return "Repair storm damage at the Power Core: " + std::to_string(colony_.damaged().size()) + " system(s)."; },
        [this] { return flags_.stormPassed && colony_.damaged().empty(); }, fixed({43, 34}));

    // ---- Act 4: The Light
    add(4, txt("Find the nav computer east of the ARK-7 wreck."), [this] { return has("Nav computer") || colony_.system("beacon")->built(); }, [this] { return itemTile("Nav computer"); });
    add(4,
        [this] {
            int needCables = 2 + static_cast<int>(std::count(lit_.begin(), lit_.end(), false));
            if (!world_.gateOpen()) {
                if (scrap_ < 3 || cables_ < needCables)
                    return "Gate opens at H60. Meanwhile gather scrap " + std::to_string(scrap_) + "/3, cables " + std::to_string(cables_) + "/" + std::to_string(needCables) + ".";
                return std::string("The Landing Zone gate opens at Hour 60, sooner if the Hour 56 report goes well.");
            }
            return "Build the beacon at the Landing Zone (scrap " + std::to_string(scrap_) + "/3, cables " + std::to_string(cables_) + "/2).";
        },
        [this] { return colony_.system("beacon")->built(); },
        [this]() -> std::optional<IVec> {
            int needCables = world_.gateOpen() ? 2 : 2 + static_cast<int>(std::count(lit_.begin(), lit_.end(), false));
            auto t = materialTarget(3, needCables, world_.gateOpen() ? std::optional<IVec>(World::BEACON_SITE) : std::nullopt);
            return t;
        });
    add(4,
        [this] { return "Cut the debris off the landing pad: " + std::to_string(std::count(padCleared_.begin(), padCleared_.end(), true)) + " of 3."; },
        [this] { return std::count(padCleared_.begin(), padCleared_.end(), true) == 3; },
        [this]() -> std::optional<IVec> {
            for (size_t i = 0; i < padCleared_.size(); ++i)
                if (!padCleared_[i]) return world_.padDebris()[i];
            return std::nullopt;
        });
    add(4, [this] { return "Place landing lights, 1 cable each: " + std::to_string(lightsPlaced()) + " of 6."; },
        [this] { return lightsPlaced() == 6; },
        [this]() -> std::optional<IVec> {
            for (size_t i = 0; i < lit_.size(); ++i)
                if (!lit_[i]) return materialTarget(0, 1, world_.lightMounts()[i]);
            return std::nullopt;
        });
    add(4,
        [this] {
            if (!colony_.isOn("beacon")) return std::string("Power the BEACON and keep it on for the landing at Hour 72.");
            if (!hanaCanGuide() && !flags_.shuttleGuided && hana().alive() && hana().idle() && !flags_.hanaPlotted)
                return std::string("No flight plan: ask Hana to plot the descent, or guide the shuttle yourself at H70.");
            if (!hanaCanGuide() && !flags_.shuttleGuided && hana().alive() && !flags_.hanaPlotted && hana().state() != CrewState::Working)
                return std::string("Hana is still recovering and cannot plot. Guide the shuttle yourself at the beacon at H70.");
            if (!hanaCanGuide() && !flags_.shuttleGuided) return std::string("No pilot: guide the shuttle at the beacon from Hour 70.");
            return std::string("Beacon online. Keep everyone alive until the shuttle lands at Hour 72.");
        },
        [this] { return false; },
        [this]() -> std::optional<IVec> {
            if (!colony_.isOn("beacon")) return IVec{43, 35};
            if (!hanaCanGuide() && !flags_.shuttleGuided && hana().alive() && hana().idle() && !flags_.hanaPlotted && clock_.hour() < 70) return hana().tile();
            if (!hanaCanGuide() && !flags_.shuttleGuided && clock_.hour() >= 70) return World::BEACON_SITE;
            return std::nullopt;
        });
}

// ---------------------------------------------------------------- guidance

// Emergencies outrank the story goal: low suit air first, then a critical colony bar that the player
// can fix right now (switch a system on, or open the emergency water).
Session::Urgent Session::urgent() const {
    if (outside() && player_.suit <= bal_.get("suit_warning")) return {"Suit air is low! Get back inside the base now.", IVec{50, 42}, false};
    if (!colony_.powered || colony_.mainPowerOut || ended_) return {};
    const float crit = bal_.get("low_bar_threshold");
    struct Fix { Bar bar; const char* system; const char* name; };
    static const Fix fixes[] = {{Bar::Oxygen, "oxygen", "Oxygen"}, {Bar::Warmth, "heater", "Warmth"}, {Bar::Water, "purifier", "Water"}, {Bar::Food, "greenhouse", "Food"}};
    for (const Fix& f : fixes) {
        if (colony_.bar(f.bar).value() >= crit) continue;
        if (f.bar == Bar::Water && !flags_.waterReserve) return {"Water is critical! Open the emergency barrels in Storage.", IVec{55, 41}, false};
        const System* s = colony_.system(f.system);
        if (s->built() && !s->on() && !colony_.isDamaged(f.system)) {
            std::string sys = f.system;
            for (char& ch : sys) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
            bool full = colony_.usedUnits() >= colony_.powerUnits();
            return {std::string(f.name) + " is critical! Switch " + sys + " on (Power Panel" + (full ? ", turn another off)." : ")."), std::nullopt, true};
        }
    }
    return {};
}

std::string Session::goalText() const {
    Urgent u = urgent();
    if (!u.text.empty()) return u.text;
    const Objective* o = quests_.current();
    return o ? o->text() : "";
}

std::optional<IVec> Session::goalTarget() const {
    Urgent u = urgent();
    if (!u.text.empty()) return u.target;
    const Objective* o = quests_.current();
    return o ? o->target() : std::nullopt;
}

int Session::reportTarget() const { return contactHour_ == 8 ? 40 : contactHour_ == 16 ? 50 : 60; }

