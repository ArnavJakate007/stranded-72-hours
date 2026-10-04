// Session: what Kiran can do with each object, puzzles and clicking
#include "logic/PowerException.h"
#include "logic/Session.h"
#include "logic/SessionShared.h"
#include <algorithm>
#include <cmath>

using namespace session_detail;

// ---------------------------------------------------------------- interaction

void Session::startPuzzle(PuzzleKind kind, int difficulty, std::function<void()> onSolved, const std::string& label, float timeLimit) {
    // after two failures on the same repair, the crew talks Kiran through it: one step easier, 50% more time
    pendingLabel_ = label;
    if (puzzleFails_[label] >= 2) {
        float base = timeLimit > 0 ? timeLimit
                     : difficulty == 0 ? bal_.get("puzzle_time_easy")
                     : difficulty == 1 ? bal_.get("puzzle_time_medium")
                                       : bal_.get("puzzle_time_hard");
        difficulty = std::max(0, difficulty - 1);
        timeLimit = base * 1.5f;
        message("The crew talks you through it: easier this time.", Tone::Info);
    }
    pendingPuzzle_ = kind;
    pendingDifficulty_ = difficulty;
    pendingTimeLimit_ = timeLimit;
    pendingSuccess_ = std::move(onSolved);
    player_.path.clear();
    emit(EventType::StartPuzzle, label, Tone::Info, static_cast<int>(kind));
}

void Session::perform(const Interaction& a, IVec target, bool automatic) {
    if (a.hold > 0.0f) {
        hold_ = Hold{target, 0.0f, a.hold, a.label, automatic, a.run};
        return;
    }
    if (a.puzzle != PuzzleKind::None) {
        startPuzzle(a.puzzle, a.difficulty, a.run, a.label, a.timeLimit);
        return;
    }
    if (a.run) a.run();
}

void Session::puzzleFinished(bool solved) {
    auto success = pendingSuccess_;
    pendingPuzzle_ = PuzzleKind::None;
    pendingSuccess_ = nullptr;
    if (solved) {
        puzzleFails_.erase(pendingLabel_);
        sound("ok");
        if (success) success();
        return;
    }
    puzzleFails_[pendingLabel_]++;
    // failing never ends the game: it costs time (PRD G10)
    float cost = bal_.get("puzzle_fail_hours");
    sound("buzz");
    emit(EventType::Shake);
    message("The repair failed and cost " + std::to_string(static_cast<int>(cost * 60)) + " minutes. Try again.", Tone::Bad);
    if (clock_.running())
        for (int h : clock_.skipTo(clock_.hour() + cost)) {
            tickHour(h);
            if (ended_) return;
        }
}

void Session::cancelPuzzle() {
    pendingPuzzle_ = PuzzleKind::None;
    pendingSuccess_ = nullptr;
    message("Repair cancelled.", Tone::Info);
}

void Session::interactFront() {
    IVec t = player_.tile(), f = player_.front();
    if (!world_.inBounds(f.x, f.y) || !world_.passEdge(t, f)) return;
    if (auto a = actionAt(f)) perform(*a, f, false);
}

void Session::pressAccept() {
    if (ended_ || player_.moving() || hold_ || pendingPuzzle_ != PuzzleKind::None) return;
    interactFront();
}

void Session::pressBack() { hold_.reset(); }

void Session::clickTile(IVec t) {
    if (ended_ || !world_.inBounds(t.x, t.y)) return;
    auto a = actionAt(t);
    if (a && !a->quiet) {
        // big objects span several tiles: walk to whichever tile of the object is reachable
        std::vector<IVec> group{t};
        const std::string& name = world_.solidName(t.x, t.y);
        if (!name.empty() && !npcAt(t) && !itemAt(t)) {
            for (size_t i = 0; i < group.size() && group.size() < 16; ++i)
                for (Dir d : {Dir::Up, Dir::Down, Dir::Left, Dir::Right}) {
                    IVec dv = dirVec(d), n{group[i].x + dv.x, group[i].y + dv.y};
                    if (world_.inBounds(n.x, n.y) && world_.solidName(n.x, n.y) == name && manhattan(n, t) < 4 &&
                        std::find(group.begin(), group.end(), n) == group.end())
                        group.push_back(n);
                }
        }
        auto adjacentTo = [&](IVec p) -> std::optional<IVec> {
            for (IVec g : group)
                if (manhattan(g, p) == 1 && world_.passEdge(p, g)) return g;
            return std::nullopt;
        };
        if (!player_.moving())
            if (auto here = adjacentTo(player_.tile())) {
                player_.face(dirTo(player_.tile(), *here));
                if (auto act = actionAt(*here)) perform(*act, *here, true);
                return;
            }
        std::vector<IVec> goals;
        for (IVec g : group)
            for (Dir d : {Dir::Up, Dir::Down, Dir::Left, Dir::Right}) {
                IVec dv = dirVec(d), n{g.x + dv.x, g.y + dv.y};
                if (canStand(n) && world_.passEdge(n, g)) goals.push_back(n);
            }
        auto path = pathfinder_.find(player_.tile(), goals, [this](IVec a2, IVec b) { return canMove(a2, b); });
        if (!path) {
            message("Kiran can't reach that.", Tone::Info);
            return;
        }
        player_.path.assign(path->begin(), path->end());
        IVec end = path->empty() ? player_.tile() : path->back();
        auto tgt = adjacentTo(end);
        player_.pendingTarget = tgt ? *tgt : t;
        return;
    }
    if (!canStand(t)) return;
    auto path = pathfinder_.find(player_.tile(), {t}, [this](IVec a2, IVec b) { return canMove(a2, b); });
    if (!path) {
        message("Kiran can't reach that.", Tone::Info);
        return;
    }
    player_.path.assign(path->begin(), path->end());
    player_.pendingTarget.reset();
}

Interaction Session::cutAction(const std::string& label, std::function<void()> onCut) {
    if (!hasTool("Cutter")) return Interaction{label, 0, PuzzleKind::None, 0, [this] { say("ins_need_cutter"); }};
    return Interaction{label, 0, PuzzleKind::Cut, 0, std::move(onCut)};
}

std::optional<Interaction> Session::actionAt(IVec t) {
    if (!world_.inBounds(t.x, t.y)) return std::nullopt;
    if (const CrewMember* m = npcAt(t)) {
        if (m->id() == "hana" && m->state() == CrewState::Missing) return cutAction("Cut Hana free", [this] { freeHana(); });
        int index = static_cast<int>(m - &colony_.crew()[0]);
        return Interaction{m->name(), 0, PuzzleKind::None, 0, [this, index] { openTalk(index); }};
    }
    for (WorldItem& it : items_)
        if (!it.taken && it.tile == t) {
            WorldItem* p = &it;
            return Interaction{"Pick up " + it.name, 0, PuzzleKind::None, 0, [this, p] { pickup(*p); }};
        }
    if (world_.tile(t.x, t.y) == Tile::Gate) return Interaction{"Gate", 0, PuzzleKind::None, 0, [this] { say("ins_gate"); }};
    const std::string& n = world_.solidName(t.x, t.y);
    if (n.empty()) return std::nullopt;
    auto inspect = [this](const std::string& label, const std::string& id, std::vector<std::string> args = {}, bool quiet = false) {
        return Interaction{label, 0, PuzzleKind::None, 0, [this, id, args] { say(id, args); }, quiet};
    };
    auto openPanel = [this](const std::string& label, const std::string& panel) {
        return Interaction{label, 0, PuzzleKind::None, 0, [this, panel] { emit(EventType::OpenPanel, panel); }};
    };

    if (n == "Power Core") {
        if (!colony_.powered) {
            if (has("Power cell")) return Interaction{"Repair Power Core", 0, PuzzleKind::Wire, 0, [this] { restorePower(); }};
            return inspect("Power Core", "ins_core_dead");
        }
        if (has("Generator coil") && !flags_.coilInstalled) return Interaction{"Install the coil", 0, PuzzleKind::Wire, 1, [this] { installCoil(); }};
        if (!colony_.damaged().empty()) {
            std::string id = *colony_.damaged().begin();
            PuzzleKind k = id == "purifier" ? PuzzleKind::Pipe : PuzzleKind::Wire;
            return Interaction{"Repair " + colony_.system(id)->name(), 0, k, 1, [this] { repairNext(true); }};
        }
        return openPanel("Power Panel", "power");
    }
    if (n == "Comms Console" || n == "Comms Dish" || n == "Server Rack") {
        if (!colony_.system("comms")->built()) {
            if (!transmitterReadyToBuild())
                return inspect("Comms Console", "ins_comms_parts",
                               {has("Antenna dish") ? "have" : "missing", has("Amplifier") ? "have" : "missing",
                                has("Large power cell") ? "have" : "missing", std::to_string(scrap_)});
            if (colony_.crew()[0].job() == "tx") return inspect("Comms Console", "ins_maya_building");
            return Interaction{"Build transmitter", 0, PuzzleKind::Wire, 1, [this] { buildTransmitter(true); }};
        }
        if (!flags_.transmitterTaken && !flags_.contact) {
            if (colony_.transmitter() >= 100.0f)
                return Interaction{"Take transmitter", 0, PuzzleKind::None, 0, [this] {
                                       if (!player_.carry().add("Transmitter")) {
                                           message("Hands full. The transmitter needs 2 free slots.", Tone::Bad);
                                           sound("buzz");
                                           return;
                                       }
                                       flags_.transmitterTaken = true;
                                       colony_.switchOff("comms");
                                       sound("pick");
                                       toast("TRANSMITTER TAKEN");
                                   }};
            return inspect("Transmitter", "ins_comms_charge",
                           {fmt0(colony_.transmitter()), colony_.isOn("comms") ? "Charging." : "Comms needs power to charge it."});
        }
        return inspect("Comms Console", "ins_comms_out");
    }
    if (n == "Oxygen Leak") {
        if (colony_.leakSealed) return inspect("Pipe", "ins_pipe_ok");
        if (has("Sealant") && colony_.crew()[1].job() != "leak") return Interaction{"Seal the leak", 0, PuzzleKind::Pipe, 0, [this] { sealLeak(true); }};
        return inspect("Oxygen Leak", "ins_leak");
    }
    if (n == "Medical Cabinet") {
        if (flags_.sealantTaken) return inspect("Cabinet", "ins_cabinet");
        return Interaction{"Take sealant", 0, PuzzleKind::None, 0, [this] {
                               if (!player_.carry().add("Sealant")) {
                                   message("Hands full.", Tone::Bad);
                                   return;
                               }
                               flags_.sealantTaken = true;
                               sound("pick");
                               toast("GOT SEALANT");
                           }};
    }
    if (n == "Shelf") {
        if (hasTool("Cutter")) return openPanel("Cargo", "cargo");
        return Interaction{"Take cutter", 0, PuzzleKind::None, 0, [this] {
                               tools_.insert("Cutter");
                               sound("pick");
                               toast("GOT CUTTER");
                           }};
    }
    if (n == "Crates") return openPanel("Cargo", "cargo");
    if (n == "Map Table") return openPanel("Map", "map");
    if (n == "Command Desk") return openPanel("Journal", "journal");
    if (n == "Hana Bed") {
        const CrewMember& h = hana();
        if (h.state() == CrewState::Injured || h.state() == CrewState::Recovering)
            return inspect("Hana", "ins_hana_bed",
                           {fmt0(h.hp()), colony_.kitGiven ? (colony_.isOn("medical") ? "Stabilizing." : "Needs Medical powered.") : "Needs the trauma kit and Medical power."});
        return inspect("Bed", "ins_bed");
    }
    if (n == "Debris") return cutAction("Cut Hana free", [this] { freeHana(); });
    if (n == "Wreck Generator") {
        if (flags_.generator) return inspect("Generator", "ins_gen_ok");
        return Interaction{"Repair generator", 0, PuzzleKind::Sequence, 0, [this] {
                               flags_.generator = true;
                               recomputeUnits();
                               sound("hum");
                               toast("+1 POWER UNIT");
                               say("arjun_generator");
                           }};
    }
    if (n == "Antenna Site") {
        if (flags_.contact) return inspect("Antenna", "ins_antenna_done");
        if (!flags_.antenna) {
            if (player_.carry().contains("Transmitter")) return Interaction{"Set up antenna", 2.0f, PuzzleKind::None, 0, [this] { placeAntenna(); }};
            return inspect("Antenna Site", "ins_antenna_site");
        }
        if (clock_.openWindow()) return Interaction{"Align antenna", 0, PuzzleKind::Tune, 1, [this] { contactMade(); }};
        int w = clock_.nextWindow();
        return Interaction{"Wait for Hour " + std::to_string(w ? w : 24), 0, PuzzleKind::None, 0, [this] { waitForWindow(); }};
    }
    if (n == "Supply Pod") {
        if (flags_.podOpened) return inspect("Supply Pod", "ins_pod_done");
        return Interaction{"Open the supply pod", 1.5f, PuzzleKind::None, 0, [this] { openPod(); }};
    }
    if (n == "Purifier Plot") {
        if (colony_.system("purifier")->built()) return inspect("Purifier", "ins_purifier", {fmt0(colony_.bar(Bar::Water).value())});
        if (purifierReady() && colony_.crew()[1].job() != "purifier")
            return Interaction{"Build the purifier", 0, PuzzleKind::Pipe, 0, [this] { buildPurifier(true); }};
        return inspect("Purifier Plot", "ins_plot_needs", {std::to_string(ice_), std::to_string(scrap_), std::to_string(cables_)});
    }
    if (n == "Planter") {
        if (!colony_.system("greenhouse")->built()) {
            if (greenhouseReady() && colony_.crew()[2].job() != "greenhouse")
                return Interaction{"Plant the greenhouse", 0, PuzzleKind::Wire, 0, [this] { buildGreenhouse(true); }};
            return inspect("Planters", "ins_planter_needs", {has("Seeds") ? "have" : "missing", std::to_string(scrap_), std::to_string(cables_)});
        }
        if (flags_.contact && !flags_.greenhouseCovered && !flags_.stormPassed && colony_.crew()[2].job() != "cover")
            return Interaction{"Cover the greenhouse", 2.0f, PuzzleKind::None, 0, [this] { coverGreenhouse(true); }};
        int grow = colony_.greenhouseGrow, need = bal_.geti("greenhouse_grow_hours");
        return inspect("Greenhouse", "ins_greenhouse", {grow >= need ? "Ready to eat." : std::to_string(need - grow) + " hours until the first harvest."});
    }
    if (n == "Brace Point") {
        int index = -1;
        for (size_t i = 0; i < world_.bracePoints().size(); ++i)
            if (world_.bracePoints()[i] == t) index = static_cast<int>(i);
        if (index < 0) return std::nullopt;
        if (braced_[index]) return inspect("Brace", "ins_brace_done");
        if (scrap_ >= 2) return Interaction{"Place a brace", 2.0f, PuzzleKind::None, 0, [this, index] { placeBrace(index, true); }};
        return inspect("Brace Point", "ins_brace_need", {std::to_string(scrap_)});
    }
    if (n == "Reactor Hatch") return cutAction("Cut the hatch open", [this] {
        flags_.hatchCut = true;
        for (const IVec& h : world_.reactorHatch()) world_.clearSolid(h.x, h.y);
        toast("REACTOR ROOM OPEN");
        message("Radiation inside. Move fast.", Tone::Warn);
    });
    if (n == "Reactor") {
        if (flags_.reactorOn) return inspect("Reactor", "ins_reactor_on");
        if (colony_.mainPowerOut || flags_.stormWarned)
            return Interaction{"Restart the reactor", 0, PuzzleKind::Sequence, 2, [this] { restartReactor("kiran"); }, false, 30};
        return inspect("Reactor", "ins_reactor_cold");
    }
    if (n == "Pad Debris") {
        if (!world_.gateOpen()) return inspect("Debris", "ins_gate");
        int index = -1;
        for (size_t i = 0; i < world_.padDebris().size(); ++i)
            if (world_.padDebris()[i] == t) index = static_cast<int>(i);
        return cutAction("Cut the debris", [this, index, t] {
            if (index >= 0) padCleared_[index] = true;
            world_.clearSolid(t.x, t.y);
            toast("PAD CLEARED " + std::to_string(std::count(padCleared_.begin(), padCleared_.end(), true)) + "/3");
        });
    }
    if (n == "Light Mount") {
        int index = -1;
        for (size_t i = 0; i < world_.lightMounts().size(); ++i)
            if (world_.lightMounts()[i] == t) index = static_cast<int>(i);
        if (index < 0) return std::nullopt;
        if (lit_[index]) return inspect("Landing light", "ins_mount_done");
        if (cables_ >= 1) return Interaction{"Place a landing light", 1.5f, PuzzleKind::None, 0, [this, index] {
                                                 --cables_;
                                                 lit_[index] = true;
                                                 sound("hum");
                                                 toast("LANDING LIGHTS " + std::to_string(lightsPlaced()) + "/6");
                                             }};
        return inspect("Light Mount", "ins_mount_need");
    }
    if (n == "Beacon Site") {
        if (!colony_.system("beacon")->built()) {
            if (beaconReady()) return Interaction{"Build the beacon", 0, PuzzleKind::Wire, 1, [this] {
                                                      startPuzzle(PuzzleKind::Sequence, 1, [this] { buildBeacon(); }, "Calibrate the beacon");
                                                  }};
            return inspect("Beacon Site", "ins_beacon_needs", {has("Nav computer") ? "have" : "missing", std::to_string(scrap_), std::to_string(cables_)});
        }
        if (clock_.hour() >= 70 && !flags_.shuttleGuided && !hanaCanGuide() && colony_.isOn("beacon"))
            return Interaction{"Guide the shuttle", 0, PuzzleKind::Tune, 2, [this] {
                                   flags_.shuttleGuided = true;
                                   toast("SHUTTLE ON THE GLIDE PATH");
                                   say("voss_glide_path");
                               }};
        return inspect("Beacon", "ins_beacon", {colony_.isOn("beacon") ? "online. The shuttle can find you." : "offline. Power it from the Power Panel."});
    }
    if (n == "Wreckage") return inspect("Wreckage", "ins_wreckage", {}, true);
    if (n == "O2 Tank" || n == "O2 Fan")
        return inspect("O2 Plant", "ins_o2", {fmt0(colony_.bar(Bar::Oxygen).value()), colony_.leakSealed ? "" : "A leak is hissing at the west end of the room."});
    if (n == "Empty Bed") return inspect("Bed", "ins_bed");
    if (n == "Water Barrels") {
        if (!flags_.waterReserve)
            return Interaction{"Open the emergency water", 1.0f, PuzzleKind::None, 0, [this] {
                                   flags_.waterReserve = true;
                                   colony_.bar(Bar::Water) += bal_.get("water_reserve");
                                   toast("WATER +" + fmt0(bal_.get("water_reserve")));
                                   say("ins_water_reserve");
                               }};
        return inspect("Water", "ins_water", {fmt0(colony_.bar(Bar::Water).value())});
    }
    if (n == "Bunk") return inspect("Bunk", "ins_bunk", {}, true);
    if (n == "Lockers") return inspect("Lockers", "ins_lockers", {}, true);
    if (n == "Plant") return inspect("Plant", "ins_plant", {}, true);
    if (n == "Lamp") return inspect("Floodlight", "ins_lamp", {}, true);
    if (n == "Tree") return inspect("Pine", "ins_tree", {}, true);
    if (n == "Boulder") return inspect("Rock", "ins_rock", {}, true);
    if (n == "Crate") return inspect("Crate", "ins_crate", {}, true);
    return std::nullopt;
}

