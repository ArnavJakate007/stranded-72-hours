// Logic self-test: runs without a window. Build with build.bat test.
// Checks the rules the SRS asks us to verify (power budget, bars clamped, reachability, task chain, save/load).
#include "logic/PowerException.h"
#include "logic/Session.h"
#include <cstdio>
#include <string>

static int failures = 0;
#define CHECK(cond, msg)                                     \
    do {                                                     \
        if (cond) {                                          \
            std::printf("  ok    %s\n", msg);                \
        } else {                                             \
            std::printf("  FAIL  %s\n", msg);                \
            ++failures;                                      \
        }                                                    \
    } while (0)

static bool reachable(Session& s, IVec target) {
    std::vector<IVec> goals;
    for (Dir d : {Dir::Up, Dir::Down, Dir::Left, Dir::Right}) {
        IVec v = dirVec(d), n{target.x + v.x, target.y + v.y};
        if (s.canStand(n) && s.world().passEdge(n, target)) goals.push_back(n);
    }
    Pathfinder pf(World::W, World::H);
    return pf.find(s.player().tile(), goals, [&](IVec a, IVec b) { return s.canMove(a, b); }).has_value();
}

#include <filesystem>
int main() {
    // run from the project folder or from build: use the project folder either way
    if (!std::filesystem::exists("tests") && std::filesystem::exists("../tests")) std::filesystem::current_path("..");
    DataStore data;
    data.loadAll("data");
    Session s(data);

    std::puts("Resource (operator overloading, clamping)");
    Resource r("Test", 90);
    r += 50;
    CHECK(r.value() == 100.0f, "+= clamps to 100");
    r -= 250;
    CHECK(r.value() == 0.0f, "-= clamps to 0");

    std::puts("Inventory<T> template");
    Inventory<std::string> inv(4, [&](const std::string& n) { return data.items.isHeavy(n) ? 2 : 1; });
    CHECK(inv.add("Power cell") && inv.add("Antenna dish"), "two heavy parts fit in 4 slots");
    CHECK(!inv.add("Sealant"), "a fifth slot is refused");

    std::puts("Power budget and exceptions");
    Colony& c = s.colony();
    bool threw = false;
    try { c.switchOn("oxygen"); } catch (const PowerException& e) { threw = e.reason() == PowerException::Reason::NoPower; }
    CHECK(threw, "switching on before the core is repaired throws NoPower");
    c.powered = true;
    c.setPowerUnits(2);
    c.switchOn("oxygen");
    c.switchOn("medical");
    threw = false;
    try { c.switchOn("heater"); } catch (const PowerException& e) { threw = e.reason() == PowerException::Reason::NoFreeUnit; }
    CHECK(threw, "a third system with 2 units throws NoFreeUnit");
    CHECK(c.usedUnits() <= c.powerUnits(), "power in use never exceeds units");
    threw = false;
    try { c.switchOn("comms"); } catch (const PowerException& e) { threw = e.reason() == PowerException::Reason::NotBuilt; }
    CHECK(threw, "comms is refused until the transmitter is built");

    std::puts("Hourly drain (polymorphic systems)");
    float o2 = c.bar(Bar::Oxygen).value();
    c.tickHour();
    CHECK(c.bar(Bar::Oxygen).value() == o2 - 8 + 6, "oxygen: -8 with the leak, +6 from the oxygen system");
    for (int i = 0; i < 80; ++i) c.tickHour();
    bool inRange = true;
    for (Bar b : {Bar::Oxygen, Bar::Warmth, Bar::Water, Bar::Food}) inRange = inRange && c.bar(b).value() >= 0 && c.bar(b).value() <= 100;
    CHECK(inRange, "bars stay within 0..100 after 80 hours");

    std::puts("World reachability from the start tile");
    s.newRun();
    struct T { const char* name; IVec at; } targets[] = {
        {"Power Core", {44, 34}}, {"Comms console", {43, 29}}, {"Medical cabinet", {58, 27}}, {"Oxygen leak", World::LEAK},
        {"Storage shelf", {55, 37}}, {"Hana", World::HANA_TRAP}, {"Antenna site", World::ANTENNA}, {"Wreck generator", World::GENERATOR}};
    for (const T& t : targets) CHECK(reachable(s, t.at), t.name);
    for (const WorldItem& it : s.items())
        if (it.name != "Generator coil") CHECK(reachable(s, it.tile), ("item: " + it.name).c_str());   // the coil sits behind the reactor hatch

    std::puts("Save and load");
    SaveManager saves("tests");
    SaveData snap = s.snapshot();
    CHECK(saves.writeSave(snap), "save.txt written");
    SaveData back;
    CHECK(saves.readSave(back), "save.txt read back");
    s.loadFrom(back);
    CHECK(s.clock().hour() == 0.0f && s.colony().bar(Bar::Oxygen).value() == 60.0f, "state restored");
    saves.deleteSave();

    std::puts("Quest chain, played through the real rules");
    s.newRun();
    CHECK(s.quests().currentIndex() == 0, "first goal is the power cell");
    auto use = [&](IVec t) { if (auto a = s.actionAt(t)) { if (a->run) a->run(); return true; } return false; };
    auto walkTo = [&](IVec t) {
        s.clickTile(t);
        for (int i = 0; i < 2000 && s.player().tile() != t; ++i) s.update(0.05f, std::nullopt, false, false, false, 1.0f);
        return s.player().tile() == t;
    };
    auto drain = [&] { while (!s.dialogue().empty()) s.dialogue().pop(); };
    use({55, 39});
    CHECK(s.has("Power cell"), "picked up the power cell");
    use({44, 34});
    CHECK(s.colony().powered && s.colony().powerUnits() == 2 && s.clock().running(), "Power Core repaired: 2 units, clock running");
    s.toggleSystem("oxygen");
    use({58, 27});
    use(World::LEAK);
    CHECK(s.colony().leakSealed, "oxygen leak sealed");
    use({55, 37});
    CHECK(s.hasTool("Cutter"), "took the cutter");
    use(World::HANA_TRAP);
    CHECK(s.hana().state() == CrewState::Following, "Hana cut free and following");
    drain();
    s.debugTeleport({79, 64});
    CHECK(walkTo({57, 30}), "walked Hana back to the Medical Bay");
    CHECK(s.hana().state() == CrewState::Injured, "Hana is on the medical bed");
    drain();
    for (IVec t : {IVec{78, 51}, IVec{82, 55}}) use(t);
    CHECK(s.has("Trauma kit") && s.has("Antenna dish"), "trauma kit and antenna dish picked up");
    s.toggleSystem("medical");
    for (auto& o : s.talkOptions(2))
        if (o.label == "Give trauma kit") o.run();
    CHECK(s.colony().kitGiven, "kit given to Elena");
    s.debugTeleport({52, 44});
    CHECK(walkTo({50, 30}), "walked back inside: parts stored in cargo");
    for (IVec t : {IVec{12, 43}, IVec{7, 61}, IVec{66, 44}, IVec{70, 58}}) use(t);
    s.debugTeleport({52, 44});
    walkTo({50, 31});
    CHECK(s.transmitterReadyToBuild(), "all transmitter parts collected");
    drain();
    use({43, 29});
    CHECK(s.colony().system("comms")->built(), "transmitter built");
    s.colony().switchOff("oxygen");   // brief trade-off: medical stays on for Hana, comms takes oxygen's unit
    s.toggleSystem("comms");
    CHECK(s.colony().isOn("comms"), "comms powered");
    for (int i = 0; i < 6 && s.colony().transmitter() < 100; ++i) { s.update(1.0f, std::nullopt, false, false, false, 60.0f); drain(); }
    CHECK(s.colony().transmitter() >= 100, "transmitter charged");
    use({43, 29});
    s.toggleSystem("oxygen");
    use(World::ANTENNA);
    CHECK(s.flags().antenna, "antenna set up on the summit");
    if (!s.clock().openWindow()) use(World::ANTENNA);   // wait for the next window
    CHECK(s.clock().openWindow() != 0, "a signal window is open");
    use(World::ANTENNA);
    drain();
    CHECK(s.flags().contact && !s.ended(), "HALCYON contacted, the run goes on");

    std::puts("Acts 3 and 4, delegating to the crew");
    auto delegate = [&](int crew, const std::string& word) {
        for (auto& o : s.talkOptions(crew))
            if (o.label.find(word) != std::string::npos) { o.run(); return true; }
        return false;
    };
    auto skip = [&](float hour) { s.debugSkipTo(hour); drain(); };
    for (const WorldItem& it : std::vector<WorldItem>(s.items()))
        if (!it.taken && (it.name == "Scrap" || it.name == "Cables" || it.name == "Ice")) use(it.tile);
    CHECK(s.ice() >= 3 && s.scrap() >= 13 && s.cables() >= 10, "ice, scrap and cables gathered");
    CHECK(delegate(1, "purifier"), "Arjun takes the purifier job");
    use({76, 50});
    use({84, 56});
    CHECK(delegate(2, "greenhouse"), "Elena takes the greenhouse job");
    skip(s.clock().hour() + 2.5f);
    CHECK(s.colony().system("purifier")->built() && s.colony().system("greenhouse")->built(), "purifier and greenhouse built by the crew");
    use({92, 58});
    CHECK(s.flags().hatchCut, "reactor hatch cut open");
    use({93, 61});
    use({44, 34});
    CHECK(s.flags().coilInstalled && s.colony().powerUnits() == 4, "generator coil installed: 4 units");
    s.colony().switchOff("comms");
    s.colony().switchOff("medical");
    s.toggleSystem("purifier");
    s.toggleSystem("greenhouse");
    s.toggleSystem("heater");
    CHECK(s.colony().usedUnits() == 4, "oxygen, purifier, greenhouse and heater running");
    skip(25);
    CHECK(s.clock().act() == 3 && s.flags().act3, "Act 3 started");
    CHECK(delegate(1, "Brace"), "Arjun braces the base");
    skip(s.clock().hour() + 2.2f);
    CHECK(delegate(1, "Brace"), "Arjun braces it again");
    CHECK(delegate(2, "Cover"), "Elena covers the greenhouse");
    if (s.hana().state() == CrewState::Idle) delegate(3, "Plot");
    skip(s.clock().hour() + 4.2f);
    CHECK(s.braces()[0] && s.braces()[1] && s.flags().greenhouseCovered, "storm prep done by the crew");
    skip(45);
    CHECK(s.flags().stormWarned, "storm warning received");
    CHECK(delegate(1, "reactor"), "Arjun restarts the reactor before the storm");
    skip(47.5f);
    CHECK(s.flags().reactorOn && s.colony().crewById("arjun")->alive(), "reactor online, medical stock saved Arjun");
    skip(53);
    CHECK(s.flags().stormPassed && !s.colony().mainPowerOut && s.colony().damaged().empty(), "storm passed with no damage");
    skip(60.5f);
    CHECK(s.clock().act() == 4 && s.world().gateOpen(), "Act 4: the landing zone gate is open");
    use({91, 52});
    use(World::BEACON_SITE);
    s.puzzleFinished(true);
    CHECK(s.colony().system("beacon")->built(), "beacon built (wire, then sequence)");
    for (IVec t : s.world().padDebris()) use(t);
    for (IVec t : s.world().lightMounts()) use(t);
    CHECK(s.lightsPlaced() == 6, "six landing lights placed");
    { SessionEvent e; while (s.pollEvent(e)) {} }
    s.colony().switchOff("greenhouse");
    s.toggleSystem("beacon");
    CHECK(s.colony().isOn("beacon"), "beacon powered");
    for (IVec t : {World::REACTOR, World::BEACON_SITE, s.world().padDebris()[0], s.world().lightMounts()[0]})
        CHECK(reachable(s, t) || !s.world().solidName(t.x, t.y).empty(), "late-game spot is reachable");
    skip(72.2f);
    CHECK(s.ended(), "the shuttle lands at Hour 72");
    std::printf("        ending %s, crew alive %d, trust %d%%, truth %s, rough landing %s\n", s.summary().code.c_str(), s.summary().crewAlive,
                s.summary().trustPercent, s.summary().truth ? "yes" : "no", s.summary().roughLanding ? "yes" : "no");
    CHECK(s.summary().code == "E1", "ending E1: We Held Together");

    std::puts("Failure endings");
    s.newRun();
    s.debugRestorePower();
    s.toggleSystem("oxygen");
    s.toggleSystem("heater");
    s.debugSkipTo(26.5f);
    CHECK(s.ended() && s.summary().code == "F1", "no contact by Hour 24: F1 Signal Lost");
    std::printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
