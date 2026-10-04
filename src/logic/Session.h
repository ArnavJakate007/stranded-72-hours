#pragma once
#include "logic/Colony.h"
#include "logic/DataStore.h"
#include "logic/Dialogue.h"
#include "logic/GameClock.h"
#include "logic/Objectives.h"
#include "logic/Pathfinder.h"
#include "logic/Player.h"
#include "logic/Rng.h"
#include "logic/SaveManager.h"
#include "logic/World.h"
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <set>

// Messages from the rules to the display side. The logic never draws or plays sound itself (NFR-12);
// the play screen reads these events and reacts.
enum class EventType { Sound, Toast, Message, OpenPanel, StartPuzzle, ActBanner, ZoneBanner, Shake, Flash, OpenTalk, SaveGame, Storm, Ending };
enum class Tone { Info, Good, Warn, Bad };

struct SessionEvent {
    EventType type;
    std::string text;
    std::string text2;
    int value = 0;
    Tone tone = Tone::Info;
};

// Something Kiran can do with the tile he faces.
struct Interaction {
    std::string label;
    float hold = 0.0f;                    // seconds of holding Z (0 = instant)
    PuzzleKind puzzle = PuzzleKind::None; // when set, run() is called only if the puzzle is solved
    int difficulty = 0;                   // 0 easy, 1 medium, 2 hard
    std::function<void()> run;
    bool quiet = false;                   // no floating prompt for scenery
    float timeLimit = 0;                  // 0 = from balance by difficulty
};

struct TalkOption {
    std::string label;
    std::function<void()> run;
};

struct WorldItem {
    std::string name;
    IVec tile;
    bool taken = false;
};

struct StoryFlags {
    bool oxygenOnce = false, sealantTaken = false, transmitterTaken = false, antenna = false, contact = false;
    bool generator = false, suitWarned = false, healthWarned = false;
    bool act3 = false, act4 = false;
    bool podLanded = false, podOpened = false;
    bool purifierOnce = false, hatchCut = false, coilInstalled = false;
    bool greenhouseCovered = false, firstMeal = false, reactorOn = false;
    bool stormWarned = false, stormPassed = false;
    bool report32 = false, report32Met = false, report56 = false, report56Met = false;
    bool shuttleGuided = false, hanaPlotted = false, grace = false;
    bool waterReserve = false;   // the Storage water barrels, opened once
};

struct EndSummary {
    std::string code;     // F1-F4, E1-E3
    std::string reason;   // extra line for F2 (suit or health)
    float hours = 0;
    int crewAlive = 0;
    int systemsBuilt = 0;
    int trustPercent = 0;
    bool truth = false;
    bool roughLanding = false;
    int contactHour = 0;
    int reportsMet = 0;
};

// One run of the game: the world, the colony, Kiran, the story and every rule that ties them together.
class Session {
public:
    explicit Session(const DataStore& data);

    void newRun();
    void loadFrom(const SaveData& d);
    SaveData snapshot() const;

    // Called every frame by the play screen. uiBlocking stops Kiran while a panel, menu or puzzle is open;
    // paused also stops the clock (pause menu). Puzzles and panels keep the clock running (FR-8).
    void update(float dt, std::optional<Dir> heldDir, bool acceptHeld, bool uiBlocking, bool paused, float clockMultiplier);
    void pressAccept();
    void pressBack();
    void clickTile(IVec t);

    // Panels and menus call these.
    void toggleSystem(const std::string& id);
    std::vector<std::pair<std::string, std::string>> tasksFor(const CrewMember& c) const;   // label, job id
    bool assignFirstTask(int crewIndex);
    std::vector<TalkOption> talkOptions(int crewIndex);
    void puzzleFinished(bool solved);
    void cancelPuzzle();

    // Queries for the display side.
    std::optional<Interaction> actionAt(IVec t);
    const World& world() const { return world_; }
    Colony& colony() { return colony_; }
    const Colony& colony() const { return colony_; }
    const GameClock& clock() const { return clock_; }
    const Player& player() const { return player_; }
    const QuestLog& quests() const { return quests_; }
    // What the Next Goal box, arrow and map marker show: the current objective, unless the suit is running out.
    std::string goalText() const;
    std::optional<IVec> goalTarget() const;
    bool goalWantsPowerPanel() const { return urgent().panel; }   // flash the Power Panel button
    int reportTarget() const;                                      // colony health HALCYON expects at the reports
    DialogueQueue& dialogue() { return dialogue_; }
    const std::vector<WorldItem>& items() const { return items_; }
    const std::map<std::string, int>& cargo() const { return cargo_; }
    int scrap() const { return scrap_; }
    int cables() const { return cables_; }
    int ice() const { return ice_; }
    bool hasTool(const std::string& t) const { return tools_.count(t) > 0; }
    const std::set<std::string>& storyItems() const { return story_; }
    const StoryFlags& flags() const { return flags_; }
    const std::vector<bool>& braces() const { return braced_; }
    const std::vector<bool>& lights() const { return lit_; }
    const std::vector<bool>& padCleared() const { return padCleared_; }
    int lightsPlaced() const;
    int stormWarningHour() const { return flags_.report32 && !flags_.report32Met ? 47 : 44; }
    const Zone* currentZone() const { return zone_; }
    bool outside() const { return world_.roomAt(player_.tile().x, player_.tile().y) == "out"; }
    bool heavyLoad() const;
    bool has(const std::string& item) const;
    bool transmitterReadyToBuild() const;
    bool purifierReady() const { return ice_ >= 3 && scrap_ >= 2 && cables_ >= 1; }
    bool greenhouseReady() const { return has("Seeds") && scrap_ >= 2 && cables_ >= 1; }
    bool beaconReady() const { return has("Nav computer") && scrap_ >= 3 && cables_ >= 2; }
    bool hanaCanGuide() const;
    CrewMember& hana() { return colony_.crew()[3]; }
    const CrewMember& hana() const { return colony_.crew()[3]; }
    IVec hanaFollowPos(float& outX, float& outY, Dir& outDir) const;
    const CrewMember* npcAt(IVec t) const;
    const WorldItem* itemAt(IVec t) const;
    bool canStand(IVec t) const;
    bool canMove(IVec a, IVec b) const { return canStand(b) && world_.passEdge(a, b); }
    float holdProgress() const { return hold_ ? hold_->t / hold_->duration : -1.0f; }
    std::string holdLabel() const { return hold_ ? hold_->label : ""; }
    PuzzleKind pendingPuzzle() const { return pendingPuzzle_; }
    int pendingDifficulty() const { return pendingDifficulty_; }
    float pendingTimeLimit() const { return pendingTimeLimit_; }
    bool windOn() const { return player_.gustWarning || (colony_.storm && outside()); }
    bool ended() const { return ended_; }
    const EndSummary& summary() const { return summary_; }

    bool pollEvent(SessionEvent& out);

    // Test hooks used only by the automated smoke test and the self-test.
    void debugTeleport(IVec t) { player_.teleport(t); player_.wasInside = world_.roomAt(t.x, t.y) != "out"; zone_ = nullptr; }
    void debugRestorePower() { restorePower(); }
    void debugSkipTo(float hour);

private:
    struct Urgent { std::string text; std::optional<IVec> target; bool panel = false; };
    Urgent urgent() const;

    struct Hold {
        IVec target;
        float t = 0, duration = 1;
        std::string label;
        bool automatic = false;
        std::function<void()> run;
    };

    void resetCore();
    void buildQuests();
    void emit(EventType type, const std::string& text = "", Tone tone = Tone::Info, int value = 0, const std::string& text2 = "");
    void sound(const std::string& name) { emit(EventType::Sound, name); }
    void message(const std::string& text, Tone tone) { emit(EventType::Message, text, tone); }
    void toast(const std::string& text) { emit(EventType::Toast, text); }
    void say(const std::string& id, const std::vector<std::string>& args = {}) { dialogue_.say(id, args); }

    void tickHour(int hour);
    void checkActs();
    void updatePlayer(float dt, std::optional<Dir> heldDir, bool uiBlocking);
    bool tryStep(Dir d, bool forced);
    void onArrive();
    void hurtKiran(float amount, const std::string& why);
    void perform(const Interaction& a, IVec target, bool automatic);
    void startPuzzle(PuzzleKind kind, int difficulty, std::function<void()> onSolved, const std::string& label, float timeLimit = 0);
    void interactFront();
    void pickup(WorldItem& it);
    bool takeItem(const std::string& name);
    void endGame(const std::string& code, const std::string& reason = "");
    void finishRescue();
    std::optional<IVec> itemTile(const std::string& name) const;
    void recomputeUnits();
    void addTrust(int weight, bool byCrew);

    // story actions
    void restorePower();
    void sealLeak(bool byKiran);
    void buildTransmitter(bool byKiran);
    void freeHana();
    void deliverHana();
    void giveKit();
    void placeAntenna();
    void waitForWindow();
    void contactMade();
    void openPod();
    void buildPurifier(bool byKiran);
    void buildGreenhouse(bool byKiran);
    void coverGreenhouse(bool byKiran);
    void placeBrace(int index, bool byKiran);
    void installCoil();
    void restartReactor(const std::string& byWho);
    void repairNext(bool byKiran);
    void stormHits();
    void stormEnds();
    void statusReport(int hour);
    void buildBeacon();
    void openTalk(int crewIndex);
    void assignJob(CrewMember& c, const std::string& job);
    void jobFinished(CrewMember& c, const std::string& job);
    Interaction cutAction(const std::string& label, std::function<void()> onCut);
    std::optional<IVec> materialTarget(int scrapNeed, int cableNeed, std::optional<IVec> fallback) const;
    std::string reactorLabel(int hours) const {
        return "Restart the reactor (" + std::to_string(hours) + "h, " + (has("Medical stock") ? "uses the medical stock)" : "FATAL radiation without medical stock)");
    }

    const DataStore& data_;
    const Balance& bal_;
    World world_;
    Colony colony_;
    GameClock clock_;
    Player player_;
    DialogueQueue dialogue_;
    QuestLog quests_;
    Pathfinder pathfinder_;
    std::vector<WorldItem> items_;
    std::map<std::string, int> cargo_;
    std::set<std::string> tools_, story_;
    int scrap_ = 0, cables_ = 0, ice_ = 0;
    StoryFlags flags_;
    std::vector<bool> braced_, lit_, padCleared_;
    const Zone* zone_ = nullptr;
    std::optional<Hold> hold_;
    PuzzleKind pendingPuzzle_ = PuzzleKind::None;
    int pendingDifficulty_ = 0;
    float pendingTimeLimit_ = 0;
    std::function<void()> pendingSuccess_;
    std::deque<SessionEvent> events_;
    bool ended_ = false;
    EndSummary summary_;
    int contactHour_ = 0;
    Rng rng_{777};
    std::set<std::string> weakWarned_;           // crew already warned about low health
    std::map<std::string, int> puzzleFails_;     // failures per repair, for the easier retry
    std::string pendingLabel_;
    bool autosaveDue_ = false;
};
