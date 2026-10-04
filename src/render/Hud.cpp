#include "render/Hud.h"
#include "render/WorldRenderer.h"
#include "gfx/Sprites.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
std::string hms(float hours) {
    if (hours < 0) hours = 0;
    int s = static_cast<int>(hours * 3600);
    char buf[16];
    std::snprintf(buf, sizeof buf, "%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    return buf;
}
Color stateColor(float v) { return v > 50 ? pal::Teal : v >= 25 ? pal::Amber : pal::Red; }
std::string rateText(float r) {
    char buf[16];
    float a = std::fabs(r);
    if (std::fabs(a - std::round(a)) > 0.01f) std::snprintf(buf, sizeof buf, "%c%.1f/h", r >= 0 ? '^' : '~', a);
    else std::snprintf(buf, sizeof buf, "%c%d/h", r >= 0 ? '^' : '~', static_cast<int>(std::round(a)));
    return buf;
}
}  // namespace

Color Hud::toneColor(Tone t) {
    switch (t) {
        case Tone::Good: return pal::TealHi;
        case Tone::Warn: return pal::Amber;
        case Tone::Bad: return pal::Red;
        default: return pal::Dim;
    }
}

void Hud::reset(const Session& s) {
    for (int i = 0; i < 4; ++i) {
        float v = s.colony().bar(static_cast<Bar>(i)).value();
        bars_[i] = BarAnim{v, v, 0, 0};
    }
    hintTime_ = 12;
}

void Hud::update(float dt, const Session& s) {
    for (int i = 0; i < 4; ++i) {
        BarAnim& b = bars_[i];
        float v = s.colony().bar(static_cast<Bar>(i)).value();
        if (b.shown >= 25 && v < 25) b.flash = 4;
        b.shown += (v - b.shown) * std::min(1.0f, dt * 5);   // bars slide, they never jump
        if (v < b.ghost) {
            b.hold += dt;
            if (b.hold > 0.7f) b.ghost += (v - b.ghost) * std::min(1.0f, dt * 3);
        } else {
            b.ghost = v;
            b.hold = 0;
        }
        if (b.flash > 0) b.flash -= dt;
    }
    toastTime_ -= dt;
    messageTime_ -= dt;
    zoneTime_ -= dt;
    actTime_ -= dt;
    if (s.player().tile() != IVec{50, 30}) hintTime_ -= dt;
}

void Hud::drawWaypoint(Session& s, float t) const {
    auto tg = s.goalTarget();
    if (!tg) return;
    const Player& p = s.player();
    // same camera rule as the play screen: centred on Kiran, clamped to the world edges
    Cam cam = Cam::follow(p.pixelX(), p.pixelY());   // same camera rule as the play screen
    float camX = static_cast<float>(cam.x), camY = static_cast<float>(cam.y);
    float x = tg->x * 16 + 8 - camX, y = tg->y * 16 + 8 - camY;
    if (x > 8 && y > 48 && x < 632 && y < 352) return;
    float cx = 320, cy = 180, dx = x - cx, dy = y - cy;
    float k = std::min(std::fabs(180 / (dx == 0 ? 1e-6f : dx)), std::fabs(120 / (dy == 0 ? 1e-6f : dy)));
    float ax = std::round(cx + dx * k), ay = std::round(cy + dy * k);
    float ang = std::atan2(dy, dx);
    float pulse = std::sin(t * 6) > 0 ? 1.0f : 0.0f;
    auto rot = [&](float lx, float ly) { return Vector2{ax + lx * std::cos(ang) - ly * std::sin(ang), ay + lx * std::sin(ang) + ly * std::cos(ang)}; };
    DrawTriangle(rot(10 + pulse, 0), rot(-6, -8), rot(-6, 8), pal::PanelEdge);
    DrawTriangle(rot(8 + pulse, 0), rot(-4, -6), rot(-4, 6), pal::Amber);
    int dist = static_cast<int>(std::lround(std::hypot(tg->x - p.tile().x, tg->y - p.tile().y)));
    Ui::chip(ax - 14, ay + 10, std::to_string(dist) + "m", pal::Amber, pal::rgb(0x5A4214));
}

void Hud::draw(Session& s, float t, HitList& hits, const std::function<void(const std::string&)>& openPanel, bool panelOpen) {
    const Colony& col = s.colony();
    const GameClock& clk = s.clock();
    if (!panelOpen) drawWaypoint(s, t);

    // ---- status card (top left): act, hour, the clock that matters now, and what comes next
    const int hourNow = static_cast<int>(clk.hour());
    Ui::card(6, 6, 150, 36, pal::Amber);
    std::string actName = GameClock::actName(clk.act());
    for (char& ch : actName) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
    Ui::text("ACT " + std::to_string(clk.act()), 11, 10, pal::Amber, 1, false);
    Ui::text(actName, 44, 10, pal::Soft, 1, false);
    Ui::textRight("H" + std::to_string(hourNow), 151, 10, pal::Dim);
    if (!s.flags().contact) {
        Color cc = !clk.running() ? pal::rgb(0x7A3A3A) : pal::rgb(0xFF6A5E);
        if (clk.running()) Ui::glow(110, 23, 30, pal::Red, 0.10f);
        Ui::text("CONTACT", 11, 20, pal::Dim2, 1, false);
        Ui::textRight(clk.running() ? hms(clk.hoursToContactDeadline()) : "24:00:00", 151, 20, cc);
    } else {
        Ui::text("SHUTTLE", 11, 20, pal::Dim2, 1, false);
        Ui::textRight(hms(clk.hoursToShuttle()), 151, 20, pal::TealHi);
    }
    {
        std::string line;
        Color lc = pal::Dim;
        if (!s.flags().contact) {
            int open = clk.openWindow(), next = clk.nextWindow();
            line = open ? "Radio window OPEN" : next ? "Next radio window H" + std::to_string(next) : "No windows left";
            lc = open ? pal::TealHi : pal::Dim;
        } else if (col.storm) {
            line = "Storm until H" + std::to_string(GameClock::STORM_END);
            lc = static_cast<int>(t * 3) % 2 ? pal::Red : pal::Amber;
        } else if (!s.flags().stormPassed && hourNow >= s.stormWarningHour() && hourNow < GameClock::STORM_HOUR) {
            line = "Storm in " + std::to_string(GameClock::STORM_HOUR - hourNow) + "h";
            lc = pal::Amber;
        } else if (s.flags().grace) {
            line = "Shuttle circling!";
            lc = static_cast<int>(t * 3) % 2 ? pal::Red : pal::Amber;
        } else {
            int report = 0;
            for (int h : GameClock::REPORT_HOURS)
                if (hourNow < h) { report = h; break; }
            if (report) {
                int health = static_cast<int>(std::lround(col.colonyHealth())), target = s.reportTarget();
                line = "Report H" + std::to_string(report) + "  " + std::to_string(health) + "/" + std::to_string(target) + "%";
                lc = health >= target ? pal::TealHi : pal::RedSoft;
            } else {
                line = "Shuttle lands H72";
            }
        }
        Ui::text(line, 11, 30, lc, 1, false);
    }

    // ---- vitals card (left): power pips, then four slim bars
    Ui::card(6, 46, 104, 80);
    int used = col.usedUnits(), units = col.powerUnits();
    Ui::text("PWR", 11, 50, pal::Dim2, 1, false);
    for (int i = 0; i < 6; ++i) {
        float px = 32 + i * 9.0f;
        if (i < used) { Ui::rect(px, 51, 7, 5, pal::Amber); Ui::rect(px, 51, 7, 1, pal::AmberLight); }
        else if (i < units) { Ui::rect(px, 51, 7, 5, pal::AmberDark); Ui::rect(px + 1, 52, 5, 3, pal::rgb(0x0C1320)); }
        else Ui::rect(px, 51, 7, 5, pal::rgb(0x1A2336));
    }
    if (col.mainPowerOut) Ui::textRight("OUT", 105, 50, static_cast<int>(t * 3) % 2 ? pal::Red : pal::AmberDark);
    else Ui::textRight(std::to_string(used) + "/" + std::to_string(units), 105, 50, pal::Amber);
    if (s.flags().act3) {   // battery charge from unused units (T12)
        Ui::rect(32, 58, 53, 1, pal::rgb(0x1A2336));
        Ui::rect(32, 58, std::round(53 * col.battery / 100.0f), 1, col.battery >= 70 ? pal::TealHi : pal::Blue);
    }
    for (int i = 0; i < 4; ++i) {
        float y = 63 + i * 15.0f;
        const BarAnim& b = bars_[i];
        Icons::bar(i, 10, y - 1);
        std::string vs = std::to_string(static_cast<int>(std::lround(b.shown)));
        Ui::textRight(vs, 105, y, b.shown < 25 ? pal::Red : pal::Ink);
        float r = col.ratePerHour(static_cast<Bar>(i));
        Ui::text(rateText(r), 23, y, r >= 0 ? pal::rgb(0x4FB892) : pal::rgb(0xC97A70), 1, false);
        bool flash = b.flash > 0 && static_cast<int>(t * 2) % 2 == 0;
        Ui::thinBar(23, y + 9, 82, b.shown, b.ghost, stateColor(b.shown), flash);
    }

    // ---- suit and health: small cards that only appear when they matter
    float lowY = 130;
    if (s.outside()) {
        float suit = s.player().suit, pct = suit / s.player().suitMax * 100;
        bool low = suit <= 30;
        Ui::card(6, lowY, 104, s.currentZone() ? 30 : 20, low ? pal::Red : pal::Blue);
        Ui::text("SUIT AIR", 11, lowY + 4, low ? pal::Red : pal::Ice, 1, false);
        Ui::textRight(std::to_string(static_cast<int>(std::ceil(suit))), 105, lowY + 4, pal::Ink);
        Ui::thinBar(11, lowY + 13, 94, pct, pct, low ? pal::Red : pal::Blue, low && static_cast<int>(t * 2) % 2 == 0);
        if (const Zone* z = s.currentZone()) Ui::text(z->name, 11, lowY + 19, pal::Dim, 1, false);
        lowY += s.currentZone() ? 34 : 24;
    }
    float hp = s.player().health;
    if (hp < 100) {
        bool low = hp <= 30;
        Ui::card(6, lowY, 104, 20, low ? pal::Red : pal::Elena);
        Ui::text("KIRAN", 11, lowY + 4, low ? pal::Red : pal::Soft, 1, false);
        Ui::textRight(std::to_string(static_cast<int>(std::ceil(hp))) + " hp", 105, lowY + 4, pal::Ink);
        Ui::thinBar(11, lowY + 13, 94, hp, hp, low ? pal::Red : pal::Elena, false);
        if (s.world().inReactorRoom(s.player().tile().x, s.player().tile().y))
            Ui::chip(114, lowY + 4, "RADIATION", static_cast<int>(t * 4) % 2 ? pal::rgb(0x5EFF8A) : pal::Amber, pal::rgb(0x1E4A26));
    }

    // ---- goal card (top right)
    {
        bool urgent = !s.goalText().empty() && (s.goalWantsPowerPanel() || (s.outside() && s.player().suit <= 30) || s.goalText().find("critical") != std::string::npos);
        auto lines = Ui::wrap(s.goalText(), 156);
        int n = static_cast<int>(std::min<size_t>(3, lines.size()));
        float h = 16 + n * 9.0f;
        Ui::card(468, 6, 166, h, urgent ? pal::Red : pal::Amber);
        Ui::rect(473, 10, 3, 7, urgent ? pal::Red : pal::Amber);
        Ui::text(urgent ? "URGENT" : "NEXT GOAL", 480, 10, urgent ? pal::Red : pal::Amber, 1, false);
        if (auto tg = s.goalTarget())
            Ui::textRight(std::to_string(static_cast<int>(std::lround(std::hypot(tg->x - s.player().tile().x, tg->y - s.player().tile().y)))) + "m", 629, 10, pal::Dim);
        for (int i = 0; i < n; ++i) Ui::text(lines[i], 473, 20 + i * 9.0f, urgent && static_cast<int>(t * 3) % 2 ? pal::RedSoft : pal::Ink, 1, false);
        goalBottom_ = 6 + h;
    }

    // ---- icon toolbar (mouse shortcuts; the keyboard reaches them through the Enter menu)
    static const char* icons[4] = {"bolt", "crew", "pin", "book"};
    static const char* names[4] = {"Power Panel", "Crew Panel", "Map", "Journal"};
    static const char* panels[4] = {"power", "crew", "map", "journal"};
    int hovered = hover_;
    hover_ = -1;
    bool urgentPower = s.goalWantsPowerPanel() && static_cast<int>(t * 3) % 2 == 0;
    float ty = goalBottom_ + 4;
    for (int i = 0; i < 4; ++i) {
        float x = 634 - (4 - i) * 22.0f + 2;
        bool f = (hovered == i || (i == 0 && urgentPower)) && !panelOpen;
        Ui::card(x, ty, 20, 18, f ? pal::Amber : pal::PanelLine);
        Icons::menu(icons[i], x + 6, ty + 5, f ? pal::Amber : pal::rgb(0x8FA0BE));
        if (f && hovered == i) {
            int w = Ui::width(names[i]) + 10;
            Ui::card(x + 10 - w, ty + 21, static_cast<float>(w), 13, pal::Amber);
            Ui::text(names[i], x + 15 - w, ty + 24, pal::Ink, 1, false);
        }
        std::string p = panels[i];
        hits.add(Rectangle{x, ty, 20, 18}, [openPanel, p] { openPanel(p); }, [this, i] { hover_ = i; });
    }

    // ---- crew card (right): one quiet line each
    float cy = ty + 40;
    Ui::card(530, cy, 104, 14 + 4 * 10.0f);
    Ui::text("CREW", 535, cy + 4, pal::Dim2, 1, false);
    Ui::textRight(std::to_string(col.crewAlive()) + "/4", 629, cy + 4, pal::Dim2);
    for (size_t i = 0; i < col.crew().size(); ++i) {
        const CrewMember& m = col.crew()[i];
        float y = cy + 15 + i * 10.0f;
        Ui::rect(535, y + 2, 3, 3, m.alive() ? speakerColor(m.id()) : pal::rgb(0x3A4252));
        Ui::text(m.name(), 542, y, m.alive() ? pal::Soft : pal::Offline, 1, false);
        std::string st;
        Color sc;
        switch (m.state()) {
            case CrewState::Idle: st = "idle"; sc = pal::Amber; break;
            case CrewState::Working: st = "work " + std::to_string(m.hoursLeft()) + "h"; sc = pal::TealHi; break;
            case CrewState::Injured: st = "hurt"; sc = pal::Red; break;
            case CrewState::Recovering: st = "resting"; sc = pal::TealHi; break;
            case CrewState::Missing: st = "missing"; sc = pal::Red; break;
            case CrewState::Following: st = "with you"; sc = pal::TealHi; break;
            default: st = "lost"; sc = pal::Offline; break;
        }
        Ui::textRight(st, 629, y, sc);
    }

    // ---- carried slots, scrap and cables (bottom centre, compact)
    const auto& carried = s.player().carry().items();
    float sx = 262;
    Ui::card(240, 336, 160, 20);
    if (s.hasTool("Cutter")) Icons::item("Cutter", 245, 340);
    for (int slot = 0; slot < 4;) {
        std::string n;
        int acc = 0;
        for (const std::string& q : carried) {
            if (acc == slot) { n = q; break; }
            acc += s.player().carry().sizeOf(q);
        }
        int w = n.empty() ? 1 : s.player().carry().sizeOf(n);
        float ww = w * 16.0f + (w - 1) * 2.0f;
        Ui::rect(sx, 338, ww, 16, pal::rgb(0x0E1626));
        Ui::rect(sx, 338, ww, 1, pal::rgb(0x24324E));
        if (!n.empty()) Icons::item(n, sx + std::floor(ww / 2) - 6, 340);
        sx += ww + 2;
        slot += w;
    }
    Icons::item("Scrap", 340, 336);
    Ui::text(std::to_string(s.scrap()), 354, 339, pal::Soft, 1, false);
    Icons::item("Cables", 368, 341);
    Ui::text(std::to_string(s.cables()), 382, 344, pal::Soft, 1, false);

    // banners, toasts and messages
    if (zoneTime_ > 0) {
        float k = std::max(0.0f, std::min({1.0f, zoneTime_ / 0.3f, (2.8f - zoneTime_ + 0.3f) / 0.3f}));
        float y = std::round(8 - (1 - k) * 30);
        std::string sub = zoneSub_ + (zoneCost_ ? "  Suit -" + std::to_string(zoneCost_) : "");
        std::string up = zoneName_;
        for (char& c : up) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        float w = std::max(Ui::width(up), Ui::width(sub)) + 16.0f, zx = std::round(320 - w / 2);
        Ui::card(zx, y, w, 24, zoneCost_ ? pal::Blue : pal::PanelLine);
        Ui::text(up, zx + 8, y + 4, pal::Ink, 1, false);
        Ui::text(zoneSub_, zx + 8, y + 14, pal::Dim, 1, false);
        if (zoneCost_) Ui::text("Suit -" + std::to_string(zoneCost_), zx + 8 + Ui::width(zoneSub_) + 8.0f, y + 14, pal::Ice, 1, false);
    }
    if (toastTime_ > 0 && !panelOpen) {
        // drops in from under the top bar, then lifts away
        float k = std::max(0.0f, std::min({1.0f, (2.6f - toastTime_) / 0.18f, toastTime_ / 0.25f}));
        float w = Ui::width(toast_) + 26.0f, x = std::round(320 - w / 2), y = std::round(64 + 14 * k);
        Ui::glow(320, y + 7, w * 0.6f, pal::Amber, 0.10f * k);
        Ui::rect(x + 1, y + 14, w - 1, 1, Color{0, 0, 0, 90});
        Ui::rect(x, y, w, 14, pal::PanelEdge);
        DrawRectangleGradientV(static_cast<int>(x + 1), static_cast<int>(y + 1), static_cast<int>(w - 2), 12, pal::rgb(0x223352), pal::rgb(0x111B2E));
        Ui::rect(x + 1, y + 1, w - 2, 1, pal::Amber);
        // small diamond icon
        Ui::rect(x + 7, y + 5, 3, 3, pal::Amber); Ui::rect(x + 8, y + 4, 1, 5, pal::Amber); Ui::rect(x + 6, y + 6, 5, 1, pal::Amber);
        Ui::text(toast_, x + 16, y + 4, pal::AmberLight);
    }
    if (messageTime_ > 0 && !panelOpen) {
        float k = std::max(0.0f, std::min({1.0f, (3.4f - messageTime_) / 0.18f, messageTime_ / 0.3f}));
        Color tc = toneColor(messageTone_);
        float w = Ui::width(message_) + 18.0f, x = std::round(320 - w / 2), y = std::round(244 - 8 * k);
        Ui::rect(x + 1, y + 14, w - 1, 1, Color{0, 0, 0, 90});
        Ui::rect(x, y, w, 14, pal::PanelEdge);
        DrawRectangleGradientV(static_cast<int>(x + 1), static_cast<int>(y + 1), static_cast<int>(w - 2), 12, pal::rgb(0x142036), pal::rgb(0x0A111F));
        Ui::rect(x + 1, y + 1, 3, 12, tc);   // tone bar
        Ui::text(message_, x + 10, y + 4, tc);
    }
    if (hintTime_ > 0 && s.dialogue().empty()) {
        std::string h = "Arrows walk  -  Z interact  -  Enter menu  -  click to walk";
        float w = Ui::width(h) + 12.0f;
        Ui::rect(std::round(320 - w / 2), 312, w, 13, Color{4, 7, 13, 216});
        Ui::text(h, std::round(320 - w / 2) + 6, 315, pal::Dim);
    }
    if (actTime_ > 0) {
        float k = std::max(0.0f, std::min({1.0f, actTime_ / 0.4f, (3.2f - actTime_) / 0.3f}));
        unsigned char a = static_cast<unsigned char>(k * 210);
        // the band opens from the centre, the amber rules sweep outward
        float open = std::min(1.0f, (3.2f - actTime_) / 0.35f);
        float half = 31 * (1 - std::pow(1 - open, 3.0f));
        Ui::rect(0, 171 - half, 640, half * 2, Color{4, 7, 13, a});
        float sweep = 320 * std::min(1.0f, (3.2f - actTime_) / 0.5f);
        Ui::rect(320 - sweep, 171 - half, sweep * 2, 1, withAlpha(pal::Amber, k));
        Ui::rect(320 - sweep, 170 + half, sweep * 2, 1, withAlpha(pal::Amber, k));
        Ui::glow(320, 171, 160, pal::Amber, 0.08f * k);
        if (open >= 1) {
            Ui::textCentered(actA_, 320, 148, withAlpha(pal::Amber, k), 2);
            Ui::textCentered(actB_, 320, 168, withAlpha(pal::Ink, k), 3);
        }
    }
}
