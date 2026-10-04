#include "ui/Panels.h"
#include "gfx/Sprites.h"
#include "render/Hud.h"
#include <cmath>

namespace {
Color stateColor(float v) { return v > 50 ? pal::Teal : v >= 25 ? pal::Amber : pal::Red; }
const char* kSystems[] = {"oxygen", "medical", "heater", "comms", "greenhouse", "purifier", "beacon"};
}  // namespace

std::unique_ptr<Panel> Panel::create(const std::string& id, Session& s, const DataStore& data, const Texture2D& minimap) {
    if (id == "power") return std::make_unique<PowerPanel>(s);
    if (id == "crew") return std::make_unique<CrewPanel>(s);
    if (id == "map") return std::make_unique<MapPanel>(s, minimap);
    if (id == "journal") return std::make_unique<JournalPanel>(s);
    if (id == "cargo") return std::make_unique<CargoPanel>(s, data);
    return nullptr;
}

void Panel::draw(float t, HitList& hits, const Hud& hud) {
    float e = 1 - std::pow(1 - slide_, 3.0f);
    float x = 138, y = 50 + std::round((1 - e) * 8), w = 364, h = 208;
    Ui::rect(130, 44, 378, 298, Color{3, 5, 10, 128});
    hits.add(Rectangle{130, 44, 378, 298}, nullptr);
    Ui::panel(x, y, w, h, pal::PanelLineHi, pal::rgb(0x0D1524));
    Ui::text(title_, x + 10, y + 8, pal::Amber);
    if (!subtitle_.empty()) Ui::textRight(subtitle_, x + w - 40, y + 8, pal::Dim);
    int cw = Ui::keycap(x + w - 30, y + 5, "X");
    hits.add(Rectangle{x + w - 30, y + 5, static_cast<float>(cw), 12}, [this] { close_ = true; });
    drawContent(x, y, w, h, t, hits, hud);
}

void Panel::footer(float x, float y, float h, const Hud& hud, const std::string& hint) const {
    if (hud.messageVisible()) Ui::text(hud.currentMessage(), x + 10, y + h - 14, Hud::toneColor(hud.messageTone()));
    else Ui::text(hint, x + 10, y + h - 14, pal::Dim2);
}

// ---------------------------------------------------------------- Power

void PowerPanel::handle(Input::Act a) {
    if (a == Input::Act::Up) row_ = (row_ + 6) % 7;
    if (a == Input::Act::Down) row_ = (row_ + 1) % 7;
    if (a == Input::Act::Accept) session_.toggleSystem(kSystems[row_]);
    if (a == Input::Act::Back || a == Input::Act::Pause) close_ = true;
}

void PowerPanel::drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) {
    const Colony& c = session_.colony();
    std::string head = c.mainPowerOut ? "MAIN POWER OUT - restart the reactor"
                       : c.powered    ? std::to_string(c.usedUnits()) + " of " + std::to_string(c.powerUnits()) + " units in use"
                                      : "NO POWER";
    if (session_.flags().act3 && !c.mainPowerOut && c.powered) head += "  -  Battery " + std::to_string(static_cast<int>(c.battery)) + "%";
    Ui::textRight(head, x + w - 40, y + 8, c.powered && !c.mainPowerOut ? pal::Dim : pal::Red);
    for (int i = 0; i < 7; ++i) {
        const System* s = c.system(kSystems[i]);
        float ry = y + 24 + i * 22.0f;
        if (row_ == i) {
            Ui::rect(x + 6, ry, w - 12, 20, pal::RowHi);
            Ui::brackets(x + 6, ry, w - 12, 20, t);
        }
        if (s->on()) { Ui::rect(x + 12, ry + 7, 7, 7, pal::AmberDark); Ui::rect(x + 13, ry + 8, 5, 5, pal::Amber); }
        else { Ui::rect(x + 12, ry + 7, 7, 7, pal::rgb(0x2A3448)); Ui::rect(x + 13, ry + 8, 5, 5, s->built() ? pal::rgb(0x4A5468) : pal::rgb(0x161D2B)); }
        Ui::text(s->name(), x + 26, ry + 3, s->built() ? pal::Ink : pal::Offline);
        Ui::text(s->effect(), x + 26, ry + 12, s->built() ? pal::Dim : pal::Dim2);
        bool broken = c.isDamaged(kSystems[i]);
        std::string lab = !s->built() ? "NOT BUILT" : broken ? "DAMAGED" : s->on() ? "ONLINE" : "OFFLINE";
        float lw = Ui::width(lab) + 10.0f;
        Ui::rect(x + w - 14 - lw, ry + 5, lw, 11, broken ? pal::rgb(0x3A1416) : s->on() ? pal::rgb(0x0E3A2E) : pal::rgb(0x141C2C));
        Ui::rect(x + w - 14 - lw, ry + 5, lw, 1, broken ? pal::Red : s->on() ? pal::Teal : pal::rgb(0x2A3650));
        Ui::text(lab, x + w - 9 - lw, ry + 7, broken ? pal::Red : s->on() ? pal::TealHi : pal::Offline);
        int idx = i;
        hits.add(Rectangle{x + 6, ry, w - 12, 20}, [this, idx] { row_ = idx; session_.toggleSystem(kSystems[idx]); }, [this, idx] { row_ = idx; });
    }
    footer(x, y, h, hud, "Each running system uses 1 unit  -  Z switch  -  X close");
}

// ---------------------------------------------------------------- Crew

void CrewPanel::handle(Input::Act a) {
    if (a == Input::Act::Up) row_ = (row_ + 3) % 4;
    if (a == Input::Act::Down) row_ = (row_ + 1) % 4;
    if (a == Input::Act::Accept) session_.assignFirstTask(row_);
    if (a == Input::Act::Back || a == Input::Act::Pause) close_ = true;
}

void CrewPanel::drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) {
    const auto& crew = session_.colony().crew();
    for (int i = 0; i < 4; ++i) {
        const CrewMember& m = crew[i];
        float ry = y + 24 + i * 38.0f;
        if (row_ == i) {
            Ui::rect(x + 6, ry, w - 12, 36, pal::RowHi);
            Ui::brackets(x + 6, ry, w - 12, 36, t);
        }
        Icons::portrait(m.id(), x + 12, ry + 4);
        if (!m.alive() || m.state() == CrewState::Missing) Ui::rect(x + 12, ry + 4, 28, 28, Color{5, 8, 15, 166});
        Ui::text(m.name(), x + 48, ry + 7, m.alive() ? speakerColor(m.id()) : pal::Offline);
        Ui::text(m.role(), x + 48, ry + 18, pal::Dim);
        int filled = static_cast<int>(std::lround(m.hp() / 100 * 12));
        for (int s = 0; s < 12; ++s) Ui::rect(x + 110 + s * 5, ry + 9, 4, 6, s < filled ? stateColor(m.hp()) : pal::SegEmpty);
        Ui::text(std::to_string(static_cast<int>(std::lround(m.hp()))) + " hp", x + 110, ry + 19, pal::Dim);
        auto tasks = session_.tasksFor(m);
        std::string a, b;
        Color sc;
        switch (m.state()) {
            case CrewState::Idle: a = "Idle"; b = tasks.empty() ? "Wants a task" : "Has a job for you"; sc = pal::Amber; break;
            case CrewState::Working: a = "Working"; b = std::to_string(m.hoursLeft()) + "h left"; sc = pal::TealHi; break;
            case CrewState::Injured: a = "Injured"; b = "Medical needed"; sc = pal::Red; break;
            case CrewState::Recovering: a = "Recovering"; b = "Medical on, kit given"; sc = pal::TealHi; break;
            case CrewState::Missing: a = "Missing"; b = "Wreck Field beacon"; sc = pal::Red; break;
            case CrewState::Following: a = "With you"; b = "Take her to Medical"; sc = pal::TealHi; break;
            default: a = "Lost"; b = "In the memorial"; sc = pal::Offline; break;
        }
        Ui::text(a, x + 188, ry + 8, sc);
        Ui::text(b, x + 188, ry + 19, pal::Dim);
        bool can = m.idle() && !tasks.empty();
        float bx = x + w - 66, by = ry + 11;
        Ui::rect(bx, by, 52, 14, pal::PanelEdge);
        Ui::rect(bx + 1, by + 1, 50, 12, can ? pal::Select : pal::rgb(0x121A2A));
        Ui::rect(bx + 1, by + 1, 50, 1, can ? pal::Amber : pal::rgb(0x26324A));
        Ui::textCentered("Assign", bx + 26, by + 4, can ? pal::Ink : pal::Offline);
        int idx = i;
        hits.add(Rectangle{x + 6, ry, w - 12, 36}, [this, idx] { row_ = idx; session_.assignFirstTask(idx); }, [this, idx] { row_ = idx; });
    }
    footer(x, y, h, hud, "Giving work to your crew is how you hold together");
}

// ---------------------------------------------------------------- Map

void MapPanel::handle(Input::Act a) {
    if (a == Input::Act::Back || a == Input::Act::Pause || a == Input::Act::Accept) close_ = true;
}

void MapPanel::drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) {
    (void)w; (void)h; (void)hits; (void)hud;
    float mx = x + 8, my = y + 24;
    Ui::rect(mx - 1, my - 1, 202, 142, pal::PanelEdge);
    DrawTexturePro(minimap_, Rectangle{0, 0, 100, 70}, Rectangle{mx, my, 200, 140}, Vector2{0, 0}, 0, WHITE);
    struct Label { const char* s; int x, y; };
    const Label labels[] = {{"RIDGE", 51, 12}, {"LANDING ZONE", 86, 6}, {"ICE CAVE", 14, 36}, {"WRECK FIELD", 80, 45}, {"BASE", 50, 34}};
    bool gate = session_.world().gateOpen();
    for (const Label& l : labels)
        Ui::chip(mx + l.x * 2 - std::floor((Ui::width(l.s) + 10) / 2.0f), my + l.y * 2 - 5, l.s, std::string(l.s) == "LANDING ZONE" ? (gate ? pal::TealHi : pal::Offline) : pal::Ink);
    if (session_.flags().reactorOn || session_.flags().hatchCut) Ui::rect(mx + 94 * 2 - 2, my + 64 * 2 - 2, 4, 4, pal::rgb(0x5EFF8A));
    IVec p = session_.player().tile();
    if (static_cast<int>(t * 3) % 2 == 0) { Ui::rect(mx + p.x * 2 - 2, my + p.y * 2 - 2, 5, 5, pal::PanelEdge); Ui::rect(mx + p.x * 2 - 1, my + p.y * 2 - 1, 3, 3, WHITE); }
    if (auto tg = session_.goalTarget()) { Ui::rect(mx + tg->x * 2 - 3, my + tg->y * 2 - 3, 7, 7, pal::PanelEdge); Ui::rect(mx + tg->x * 2 - 2, my + tg->y * 2 - 2, 5, 5, pal::Amber); }
    float lx = x + 218;
    Ui::text("ZONE COSTS", lx, y + 24, pal::Dim);
    const char* rows[4][2] = {{"Wreck Field", "5 / 10 / 20"}, {"Ice Cave", "5 / 10 / 20"}, {"Ridge", "5 / 10 / 20"}, {"Landing Zone", gate ? "Open - shuttle pad" : "Locked until H60"}};
    for (int i = 0; i < 4; ++i) {
        Ui::text(rows[i][0], lx, y + 38 + i * 22.0f, pal::Ink);
        Ui::text(rows[i][1], lx, y + 48 + i * 22.0f, pal::Ice);
    }
    Ui::text("Entrance / middle / deep", lx, y + 128, pal::Dim2);
    Ui::text("Each step outside: -0.25", lx, y + 140, pal::Dim2);
    Ui::text("Deep zones drain 1.5x", lx, y + 150, pal::Dim2);
    Ui::rect(lx, y + 166, 5, 5, WHITE);
    Ui::text("You", lx + 9, y + 165, pal::Dim);
    Ui::rect(lx + 40, y + 166, 5, 5, pal::Amber);
    Ui::text("Next goal", lx + 49, y + 165, pal::Dim);
}

// ---------------------------------------------------------------- Journal

void JournalPanel::handle(Input::Act a) {
    if (a == Input::Act::Left || a == Input::Act::Right) tab_ = 1 - tab_;
    if (a == Input::Act::Back || a == Input::Act::Pause) close_ = true;
}

void JournalPanel::drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) {
    (void)hud;
    const char* tabs[2] = {"Tasks", "Story"};
    for (int i = 0; i < 2; ++i) {
        float bx = x + 80 + i * 58.0f;
        Ui::button(bx, y + 5, 52, 13, tabs[i], tab_ == i, t);
        int idx = i;
        hits.add(Rectangle{bx, y + 5, 52, 13}, [this, idx] { tab_ = idx; });
    }
    if (tab_ == 0) {
        const QuestLog& q = session_.quests();
        const auto& steps = q.steps();
        int cur = q.currentIndex();
        int start = std::max(0, std::min(cur - 4, static_cast<int>(steps.size()) - 12));
        float ry = y + 26;
        int shownAct = 0;
        for (int i = start; i < static_cast<int>(steps.size()) && i < start + 12; ++i) {
            const Objective& o = steps[i];
            if (o.act != shownAct) {
                shownAct = o.act;
                std::string name = GameClock::actName(o.act);
                for (char& ch : name) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
                Ui::text("ACT " + std::to_string(o.act) + " - " + name, x + 12, ry, pal::Dim);
                ry += 11;
            }
            bool done = o.done(), current = i == cur;
            Ui::rect(x + 14, ry + 1, 5, 5, done ? pal::Teal : current ? pal::Amber : pal::rgb(0x26324A));
            auto lines = Ui::wrap(o.text(), static_cast<int>(w - 40));
            Ui::text(lines.empty() ? "" : lines[0], x + 24, ry, done ? pal::Dim2 : current ? pal::Ink : pal::Dim);
            ry += 12;
        }
        Ui::text("Optional: wreck generator (+1 unit), flight recorder", x + 12, y + h - 14, pal::Dim2);
    } else {
        const char* lines[] = {"HALCYON ordered the lander ARK-7 to land on Kepler-X through a storm warning. Hana asked to abort. Director Voss refused.",
                               "The storm brought the lander down. Five crew: Commander Kiran Das, Maya (comms), Arjun (engineer), Elena (medic) and Hana (pilot).",
                               "Radio only reaches HALCYON at Hours 8, 16 and 24. The rescue shuttle can only launch at Hour 24 and lands at Hour 72."};
        float ry = y + 26;
        for (const char* l : lines) {
            for (const std::string& part : Ui::wrap(l, static_cast<int>(w - 24))) {
                Ui::text(part, x + 12, ry, pal::Ink);
                ry += 11;
            }
            ry += 5;
        }
        Ui::text("MYSTERY PIECES", x + 12, ry, pal::Dim);
        ry += 12;
        const CrewMember& hana = session_.hana();
        bool hanaTold = hana.alive() && hana.state() != CrewState::Missing;
        struct Piece { const char* name; bool found; };
        Piece pieces[] = {{"Flight log (ARK-7 cargo bay)", session_.storyItems().count("Flight log") > 0},
                          {"Hana's account", hanaTold},
                          {"Flight recorder (deep Ice Cave)", session_.storyItems().count("Flight recorder") > 0}};
        for (const Piece& p : pieces) {
            Ui::text(p.found ? std::string("+ ") + p.name : "+ ???", x + 16, ry, p.found ? pal::TealHi : pal::Offline);
            ry += 11;
        }
    }
}

// ---------------------------------------------------------------- Cargo

std::vector<std::pair<std::string, int>> CargoPanel::list() const {
    std::vector<std::pair<std::string, int>> out;
    const auto& cargo = session_.cargo();
    auto count = [&](const std::string& n) { auto it = cargo.find(n); return it == cargo.end() ? 0 : it->second; };
    if (tab_ == 0) {
        for (const char* n : {"Power cell", "Antenna dish", "Amplifier", "Large power cell", "Transmitter"})
            if (count(n)) out.push_back({n, count(n)});
        out.push_back({"Scrap", session_.scrap()});
        out.push_back({"Cables", session_.cables()});
    } else if (tab_ == 1) {
        for (const char* n : {"Sealant", "Trauma kit"})
            if (count(n)) out.push_back({n, count(n)});
        if (session_.hasTool("Cutter")) out.push_back({"Cutter", 1});
    } else {
        for (const std::string& s : session_.storyItems()) out.push_back({s, 1});
    }
    return out;
}

void CargoPanel::handle(Input::Act a) {
    int n = std::max(1, static_cast<int>(list().size()));
    if (a == Input::Act::Left) { tab_ = (tab_ + 2) % 3; row_ = 0; }
    if (a == Input::Act::Right) { tab_ = (tab_ + 1) % 3; row_ = 0; }
    if (a == Input::Act::Up) row_ = (row_ + n - 1) % n;
    if (a == Input::Act::Down) row_ = (row_ + 1) % n;
    if (a == Input::Act::Back || a == Input::Act::Pause) close_ = true;
}

void CargoPanel::drawContent(float x, float y, float w, float h, float t, HitList& hits, const Hud& hud) {
    (void)hud;
    const char* tabs[3] = {"Parts", "Supplies", "Story"};
    for (int i = 0; i < 3; ++i) {
        float ty = y + 24 + i * 20.0f;
        bool f = tab_ == i;
        Ui::rect(x + 8, ty, 82, 16, pal::PanelEdge);
        Ui::rect(x + 9, ty + 1, 80, 14, f ? pal::Select : pal::rgb(0x111A2B));
        if (f) Ui::rect(x + 9, ty + 1, 2, 14, pal::Amber);
        Ui::text(tabs[i], x + 16, ty + 5, f ? pal::Ink : pal::Dim);
        int idx = i;
        hits.add(Rectangle{x + 8, ty, 82, 16}, [this, idx] { tab_ = idx; row_ = 0; });
    }
    auto items = list();
    if (items.empty()) Ui::text("Nothing here yet.", x + 104, y + 30, pal::Dim);
    for (size_t i = 0; i < items.size(); ++i) {
        float ry = y + 24 + i * 18.0f, ix = x + 98;
        if (row_ == static_cast<int>(i)) {
            Ui::rect(ix, ry, w - 106, 16, pal::RowHi);
            Ui::brackets(ix, ry, w - 106, 16, t);
        }
        Icons::item(items[i].first, ix + 4, ry + 2);
        Ui::text(items[i].first, ix + 22, ry + 5, items[i].second ? pal::Ink : pal::Offline);
        Ui::textRight(std::string("\xC3\x97") + std::to_string(items[i].second), ix + w - 112, ry + 5, pal::Amber);
        int idx = static_cast<int>(i);
        hits.add(Rectangle{ix, ry, w - 106, 16}, [this, idx] { row_ = idx; }, [this, idx] { row_ = idx; });
    }
    if (!items.empty()) {
        const auto& sel = items[std::min<size_t>(row_, items.size() - 1)];
        Ui::rect(x + 98, y + h - 46, w - 106, 38, pal::rgb(0x0A111D));
        std::string desc = data_.items.has(sel.first) ? data_.items.get(sel.first).description : "";
        auto lines = Ui::wrap(desc, static_cast<int>(w - 118));
        for (size_t i = 0; i < lines.size() && i < 3; ++i) Ui::text(lines[i], x + 104, y + h - 41 + i * 11.0f, pal::Ink);
    }
    Ui::text("< > tabs", x + 12, y + h - 14, pal::Dim2);
}
