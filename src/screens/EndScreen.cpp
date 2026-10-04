#include "screens/EndScreen.h"
#include "core/Game.h"
#include "screens/Backdrop.h"
#include "screens/IntroScreen.h"
#include "screens/PlayScreen.h"
#include "screens/TitleScreen.h"

namespace {
struct EndingInfo { const char* title; const char* line; Color c; };
EndingInfo infoFor(const std::string& code) {
    if (code == "F1") return {"F1 - SIGNAL LOST", "All three radio windows passed without contact.", pal::Red};
    if (code == "F2") return {"F2 - LOST ON THE ICE", "Kiran never made it back to the base.", pal::Red};
    if (code == "F3") return {"F3 - COLONY COLLAPSE", "Colony oxygen stayed at zero for two hours.", pal::Red};
    if (code == "F4") return {"F4 - THE SHUTTLE TURNS BACK", "No beacon guided it down. The shuttle could not find the pad.", pal::Red};
    if (code == "E1") return {"E1 - TOGETHER", "Everyone walked onto the shuttle. Nobody was carried.", pal::TealHi};
    if (code == "E2") return {"E2 - THE EMPTY SEATS", "Rescued, but not everyone made it.", pal::Amber};
    if (code == "E3") return {"E3 - THE LONE COMMANDER", "You got them out alone, and the colony went dark.", pal::Amber};
    return {"THE END", "", pal::Ink};
}
}  // namespace

EndScreen::EndScreen(Game& game, EndSummary summary) : Screen(game), summary_(std::move(summary)) {
    game_.saves().unlockEnding(summary_.code);
    if (summary_.code[0] == 'F' && game_.saves().hasSave()) buttons_.push_back("Load last save");
    buttons_.push_back("New run");
    buttons_.push_back("Title");
    game_.audio().playMusic(summary_.code[0] == 'E' ? "music_finale" : "music_intro", 0.4f);
}

void EndScreen::activate(int index) {
    game_.audio().play("click");
    const std::string& b = buttons_[index];
    if (b == "Load last save") {
        SaveData d;
        if (game_.saves().readSave(d)) game_.switchTo(std::make_unique<PlayScreen>(game_, &d));
    } else if (b == "New run") {
        game_.switchTo(std::make_unique<IntroScreen>(game_, 4));
    } else {
        game_.switchTo(std::make_unique<TitleScreen>(game_));
    }
}

void EndScreen::update(float) {
    int n = static_cast<int>(buttons_.size());
    for (Input::Act a : game_.input().presses()) {
        if (a == Input::Act::Left || a == Input::Act::Up) selected_ = (selected_ + n - 1) % n;
        if (a == Input::Act::Right || a == Input::Act::Down) selected_ = (selected_ + 1) % n;
        if (a == Input::Act::Accept || a == Input::Act::Pause) { activate(selected_); return; }
    }
}

void EndScreen::draw() {
    float t = game_.time();
    bool won = summary_.code[0] == 'E';
    ClearBackground(pal::Void);
    Backdrop::stars(t);
    if (won) Backdrop::gasGiant(560, 300, 110, t);
    EndingInfo info = infoFor(summary_.code);
    if (summary_.code == "E2" && summary_.crewAlive == 0) info.line = "Rescued, but Kiran boards the shuttle alone.";
    Ui::panel(90, 22, 460, 316, info.c, pal::PanelFillDark);
    Ui::textCentered(info.title, 320, 36, info.c, 2);
    Ui::textCentered(info.line, 320, 58, pal::Ink);
    float extra = 72;
    if (!summary_.reason.empty()) { Ui::textCentered(summary_.reason, 320, extra, pal::Dim); extra += 12; }
    if (won) {
        const DialogueBook& book = game_.data().dialogue;
        std::string epi = book.get(summary_.truth ? "fin_truth_yes" : "fin_truth_no").text;
        for (const std::string& l : Ui::wrap(epi, 420)) { Ui::textCentered(l, 320, extra, summary_.truth ? pal::TealHi : pal::Dim); extra += 11; }
        if (summary_.roughLanding) { Ui::textCentered("Rough landing: no pilot guided the shuttle down.", 320, extra, pal::Amber); extra += 11; }
    }
    std::string trust = won ? std::to_string(summary_.trustPercent) + "%  " + (summary_.trustPercent >= 50 ? "(you let them help)" : "(you did it alone)") : "???";
    std::vector<std::pair<std::string, std::string>> rows = {
        {"Hours survived", std::to_string(static_cast<int>(summary_.hours))},
        {"Crew alive", std::to_string(summary_.crewAlive) + " of 4"},
        {"Systems built", std::to_string(summary_.systemsBuilt) + " of 7"},
        {"Crew trust", trust},
        {"Status reports met", summary_.contactHour ? std::to_string(summary_.reportsMet) + " of 2" : "???"},
        {"Truth uncovered", summary_.truth ? "Yes" : "???"},
        {"Contact window", summary_.contactHour ? "Hour " + std::to_string(summary_.contactHour) : "???"}};
    float y0 = std::max(110.0f, extra + 10);
    for (size_t i = 0; i < rows.size(); ++i) {
        float y = y0 + i * 19.0f;
        Ui::rect(120, y - 4, 400, 17, i % 2 ? pal::PanelFill : pal::rgb(0x121C30));
        Ui::text(rows[i].first, 130, y + 1, pal::Dim);
        Ui::textRight(rows[i].second, 510, y + 1, rows[i].second == "???" ? pal::Offline : pal::Ink);
    }
    float total = buttons_.size() * 120.0f + (buttons_.size() - 1) * 16.0f, x0 = 320 - total / 2;
    for (size_t i = 0; i < buttons_.size(); ++i) {
        float x = x0 + i * 136.0f, y = 310;
        Ui::button(x, y, 120, 18, buttons_[i], selected_ == static_cast<int>(i), t);
        int idx = static_cast<int>(i);
        game_.hits().add(Rectangle{x, y, 120, 18}, [this, idx] { activate(idx); }, [this, idx] { selected_ = idx; });
    }
}
