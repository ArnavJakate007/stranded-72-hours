// Session: setup, the main update, movement, items and collision
#include "logic/PowerException.h"
#include "logic/Session.h"
#include "logic/SessionShared.h"
#include <algorithm>
#include <cmath>

using namespace session_detail;

Session::Session(const DataStore& data)
    : data_(data), bal_(data.balance), colony_(data.balance),
      player_(data.balance.geti("carry_slots"), [&data](const std::string& n) { return data.items.isHeavy(n) ? 2 : 1; }),
      dialogue_(data.dialogue), pathfinder_(World::W, World::H) {
    newRun();
}

// ---------------------------------------------------------------- setup

void Session::resetCore() {
    world_.generate();
    colony_.reset();
    clock_.reset(0.0f, false);
    player_.reset(kStart);
    dialogue_.clear();
    weakWarned_.clear();
    puzzleFails_.clear();
    items_.clear();
    for (const Spawn& s : kSpawns) items_.push_back({s.name, {s.x, s.y}, false});
    cargo_.clear();
    tools_.clear();
    story_.clear();
    scrap_ = cables_ = ice_ = 0;
    flags_ = StoryFlags{};
    braced_.assign(world_.bracePoints().size(), false);
    lit_.assign(world_.lightMounts().size(), false);
    padCleared_.assign(world_.padDebris().size(), false);
    zone_ = nullptr;
    hold_.reset();
    pendingPuzzle_ = PuzzleKind::None;
    pendingSuccess_ = nullptr;
    events_.clear();
    ended_ = false;
    summary_ = EndSummary{};
    contactHour_ = 0;
    buildQuests();
}

void Session::newRun() {
    resetCore();
    emit(EventType::ActBanner, "ACT 1", Tone::Info, 0, "DARK");
    say("arjun_wake");
    say("arjun_cell");
    emit(EventType::SaveGame);
}

std::optional<IVec> Session::itemTile(const std::string& name) const {
    std::optional<IVec> best;
    int bestD = 1 << 30;
    for (const WorldItem& it : items_)
        if (it.name == name && !it.taken && manhattan(it.tile, player_.tile()) < bestD) {
            bestD = manhattan(it.tile, player_.tile());
            best = it.tile;
        }
    return best;
}

int Session::lightsPlaced() const { return static_cast<int>(std::count(lit_.begin(), lit_.end(), true)); }

// Hana can talk the shuttle down only if she is alive, back at the base, and has plotted the descent.
bool Session::hanaCanGuide() const {
    const CrewMember& h = hana();
    return h.alive() && h.state() != CrewState::Missing && h.state() != CrewState::Following && flags_.hanaPlotted;
}

// Where the arrow should point while a build is short of materials: the nearest scrap or cable.
std::optional<IVec> Session::materialTarget(int scrapNeed, int cableNeed, std::optional<IVec> fallback) const {
    if (scrap_ < scrapNeed) if (auto t = itemTile("Scrap")) return t;
    if (cables_ < cableNeed) if (auto t = itemTile("Cables")) return t;
    return fallback;
}

void Session::debugSkipTo(float hour) {
    if (!clock_.running()) clock_.setRunning(true);
    for (int h : clock_.skipTo(hour)) {
        tickHour(h);
        if (ended_) return;
    }
    checkActs();
}

// ---------------------------------------------------------------- events

void Session::emit(EventType type, const std::string& text, Tone tone, int value, const std::string& text2) {
    events_.push_back(SessionEvent{type, text, text2, value, tone});
}

bool Session::pollEvent(SessionEvent& out) {
    if (events_.empty()) return false;
    out = events_.front();
    events_.pop_front();
    return true;
}

// ---------------------------------------------------------------- main update

void Session::update(float dt, std::optional<Dir> heldDir, bool acceptHeld, bool uiBlocking, bool paused, float clockMultiplier) {
    if (ended_) return;
    bool storyHold = dialogue_.blocking();
    if (!paused && !storyHold) {
        float before = clock_.hour();
        for (int h : clock_.advance(dt, bal_.get("seconds_per_game_hour"), clockMultiplier)) {
            tickHour(h);
            if (ended_) return;
        }
        // standing still outside slowly uses air too (not during puzzles, panels or dialogue)
        if (outside() && clock_.running() && !player_.moving() && player_.path.empty() && !uiBlocking) {
            player_.suit = std::max(0.0f, player_.suit - (clock_.hour() - before) * bal_.get("idle_suit_per_hour"));
            if (player_.suit <= bal_.get("suit_warning") && !flags_.suitWarned) {
                flags_.suitWarned = true;
                sound("alarm");
                message("Suit oxygen at 30. Head back to the base.", Tone::Bad);
            }
            if (player_.suit <= 0.0f) {
                endGame("F2", "Your suit ran out of oxygen outside the base.");
                return;
            }
        }
    }
    if (paused) return;
    checkActs();
    if (hold_) {
        IVec f = player_.front();
        if (f != hold_->target || player_.moving() || (!hold_->automatic && !acceptHeld) || uiBlocking) {
            hold_.reset();
        } else {
            hold_->t += dt;
            if (hold_->t >= hold_->duration) {
                auto run = hold_->run;
                hold_.reset();
                run();
                if (ended_) return;
            }
        }
    }
    updatePlayer(dt, heldDir, uiBlocking || storyHold || hold_.has_value() || pendingPuzzle_ != PuzzleKind::None);
    for (CrewMember& c : colony_.crew())
        if (c.idle() && rng_.next() < dt * 0.15f) {
            const Dir dirs[4] = {Dir::Down, Dir::Left, Dir::Right, Dir::Down};
            c.face(dirs[rng_.range(0, 4)]);
        }
}

void Session::checkActs() {
    if (!flags_.act3 && flags_.contact && clock_.hour() >= GameClock::ACT3_HOUR) {
        flags_.act3 = true;
        emit(EventType::ActBanner, "ACT 3", Tone::Info, 0, "HOLDING ON");
        say("maya_act3");
        emit(EventType::SaveGame);
    }
}

void Session::updatePlayer(float dt, std::optional<Dir> heldDir, bool blocked) {
    if (player_.bumpCooldown > 0) player_.bumpCooldown -= dt;
    if (player_.moving()) {
        if (player_.advance(dt)) onArrive();
        return;
    }
    if (blocked || ended_) return;
    IVec t = player_.tile();
    bool gusty = world_.inWind(t.x, t.y) || (colony_.storm && outside());
    if (gusty) {
        player_.gustTimer -= dt;
        if (player_.gustTimer < 0.6f && !player_.gustWarning) {
            player_.gustWarning = true;
            sound("wind");
        }
        if (player_.gustTimer <= 0.0f) {
            player_.gustTimer = (colony_.storm ? 1.4f : 2.2f) + rng_.next() * 1.2f;
            player_.gustWarning = false;
            if (tryStep(Dir::Left, true)) {
                message("A gust shoves you sideways.", Tone::Info);
                return;
            }
        }
    } else {
        player_.gustWarning = false;
    }
    if (heldDir) {
        player_.path.clear();
        player_.pendingTarget.reset();
        if (*heldDir != player_.facing()) {
            player_.face(*heldDir);
            player_.turnDelay = 0.09f;   // a quick tap only turns Kiran on the spot
        } else if (player_.turnDelay > 0) {
            player_.turnDelay -= dt;
        } else {
            tryStep(*heldDir, false);
        }
        return;
    }
    player_.turnDelay = 0;
    if (!player_.path.empty()) {
        IVec n = player_.path.front();
        player_.face(dirTo(t, n));
        if (canMove(t, n)) {
            player_.path.pop_front();
            tryStep(player_.facing(), false);
        } else {
            player_.path.clear();
            player_.pendingTarget.reset();
        }
        return;
    }
    if (player_.pendingTarget) {
        IVec target = *player_.pendingTarget;
        player_.pendingTarget.reset();
        if (manhattan(target, t) == 1) {
            player_.face(dirTo(t, target));
            if (auto a = actionAt(target)) perform(*a, target, true);
        }
    }
}

bool Session::tryStep(Dir d, bool forced) {
    IVec t = player_.tile(), dv = dirVec(d), n{t.x + dv.x, t.y + dv.y};
    if (canMove(t, n)) {
        float dur = forced ? 0.1f : heavyLoad() ? bal_.get("heavy_step_seconds") : bal_.get("walk_step_seconds");
        player_.beginStep(n, dur);
        sound(world_.roomAt(n.x, n.y) == "out" ? "step_snow" : "step_metal");
        return true;
    }
    if (!forced && player_.bumpCooldown <= 0) {
        sound("bump");
        player_.bumpCooldown = 0.35f;
    }
    return false;
}

void Session::hurtKiran(float amount, const std::string& why) {
    player_.health = std::max(0.0f, player_.health - amount);
    if (player_.health <= 30 && !flags_.healthWarned) {
        flags_.healthWarned = true;
        sound("alarm");
        message("Kiran's health is low. Rest in the base with MEDICAL on.", Tone::Bad);
    }
    if (player_.health <= 0) endGame("F2", why);
}

void Session::onArrive() {
    IVec t = player_.tile();
    bool inside = world_.roomAt(t.x, t.y) != "out";
    if (!inside) {
        if (player_.wasInside) {
            player_.suit = player_.suitMax;
            toast("SUIT SEALED");
            zone_ = nullptr;
        }
        const Zone& z = world_.zoneAt(t.x, t.y);
        if (!zone_ || zone_->name != z.name || zone_->sub != z.sub) {
            // only going deeper costs extra air; heading back out to a shallower zone is free
            int previous = zone_ ? zone_->cost : 0;
            zone_ = &z;
            if (z.cost > previous) player_.suit = std::max(0.0f, player_.suit - z.cost);
            emit(EventType::ZoneBanner, z.name, Tone::Info, z.cost, z.sub);
            if (z.deep && !hasTool("Thermal layer") && z.name != "Wreck Field") {
                hurtKiran(bal_.get("cold_damage"), "The cold got to Kiran.");
                message("Deep cold bites through the suit. A thermal layer would help.", Tone::Warn);
            }
        }
        float cost = bal_.get("suit_step_cost") * (z.deep ? bal_.get("suit_deep_multiplier") : 1.0f) *
                     (heavyLoad() ? bal_.get("suit_heavy_multiplier") : 1.0f) * (colony_.storm ? 2.0f : 1.0f);
        player_.suit = std::max(0.0f, player_.suit - cost);
        if (world_.inReactorRoom(t.x, t.y)) {
            hurtKiran(bal_.get("radiation_per_step"), "Radiation from the reactor.");
            if (ended_) return;
        }
        if (player_.suit <= bal_.get("suit_warning") && !flags_.suitWarned) {
            flags_.suitWarned = true;
            sound("alarm");
            message("Suit oxygen at 30. Head back to the base.", Tone::Bad);
        }
        if (player_.suit <= 0.0f) {
            endGame("F2", "Your suit ran out of oxygen outside the base.");
            return;
        }
    } else {
        if (!player_.wasInside) {
            player_.suit = player_.suitMax;
            flags_.suitWarned = false;
            toast("SUIT REFILLED");
            zone_ = nullptr;
            auto stored = player_.carry().takeAllExcept([](const std::string& n) { return n == "Transmitter" || n == "Generator coil"; });
            for (const std::string& n : stored) cargo_[n]++;
            if (!stored.empty()) message("Stored in base cargo: " + joinList(stored) + ".", Tone::Good);
        }
        if (hana().state() == CrewState::Following && world_.roomAt(t.x, t.y) == "medical") deliverHana();
    }
    player_.wasInside = inside;
}

// ---------------------------------------------------------------- items and inventory

bool Session::has(const std::string& item) const {
    auto it = cargo_.find(item);
    return player_.carry().contains(item) || (it != cargo_.end() && it->second > 0);
}

bool Session::takeItem(const std::string& name) {
    if (player_.carry().remove(name)) return true;
    auto it = cargo_.find(name);
    if (it != cargo_.end() && it->second > 0) {
        --it->second;
        return true;
    }
    return false;
}

bool Session::heavyLoad() const {
    for (const std::string& n : player_.carry().items())
        if (data_.items.isHeavy(n)) return true;
    return false;
}

bool Session::transmitterReadyToBuild() const {
    return has("Antenna dish") && has("Amplifier") && has("Large power cell") && scrap_ >= 2;
}

void Session::pickup(WorldItem& it) {
    if (it.name == "Scrap" || it.name == "Cables" || it.name == "Ice") {
        (it.name == "Scrap" ? scrap_ : it.name == "Cables" ? cables_ : ice_)++;
        it.taken = true;
        sound("pick");
        toast("+1 " + it.name);
        return;
    }
    if (it.name == "Thermal layer") {
        tools_.insert(it.name);
        it.taken = true;
        sound("pick");
        toast("GOT THERMAL LAYER");
        message("No more cold damage in deep zones.", Tone::Good);
        return;
    }
    if (it.name == "Flight log" || it.name == "Flight recorder") {
        story_.insert(it.name);
        it.taken = true;
        sound("pick");
        toast("FOUND: " + it.name);
        say(it.name == "Flight log" ? "ins_flight_log" : "ins_flight_recorder");
        return;
    }
    if (!player_.carry().add(it.name)) {
        int need = player_.carry().sizeOf(it.name);
        message("Hands full: " + it.name + " needs " + std::to_string(need) + " slot" + (need > 1 ? "s" : "") + ". Walk into the base to store items.", Tone::Bad);
        sound("buzz");
        return;
    }
    it.taken = true;
    sound("pick");
    toast("GOT " + it.name);
    if (data_.items.isHeavy(it.name)) message("Heavy part: you walk slower.", Tone::Warn);
}

// ---------------------------------------------------------------- collision

const CrewMember* Session::npcAt(IVec t) const {
    for (const CrewMember& c : colony_.crew()) {
        if (!c.alive() || c.state() == CrewState::Following) continue;
        if (c.id() == "hana" && (c.state() == CrewState::Injured || c.state() == CrewState::Recovering)) continue;
        if (c.tile() == t) return &c;
    }
    return nullptr;
}

const WorldItem* Session::itemAt(IVec t) const {
    for (const WorldItem& it : items_)
        if (!it.taken && it.tile == t) return &it;
    return nullptr;
}

bool Session::canStand(IVec t) const {
    return world_.inBounds(t.x, t.y) && !world_.solidTile(t.x, t.y) && world_.solidName(t.x, t.y).empty() && !npcAt(t) && !itemAt(t);
}

IVec Session::hanaFollowPos(float& outX, float& outY, Dir& outDir) const {
    const auto& trail = player_.trail;
    if (trail.empty()) {
        outX = player_.pixelX();
        outY = player_.pixelY();
        outDir = player_.facing();
        return player_.tile();
    }
    IVec t = trail[0], prev = trail.size() > 1 ? trail[1] : trail[0];
    float k = player_.moving() ? player_.progress() : 1.0f;
    outX = (prev.x + (t.x - prev.x) * k) * 16.0f + 8.0f;
    outY = (prev.y + (t.y - prev.y) * k) * 16.0f + 13.0f;
    outDir = prev == t ? player_.facing() : dirTo(prev, t);
    return t;
}

