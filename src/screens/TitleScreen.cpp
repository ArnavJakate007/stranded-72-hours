#include "screens/TitleScreen.h"
#include "core/Game.h"
#include "screens/Backdrop.h"
#include "screens/HowToScreen.h"
#include "screens/IntroScreen.h"
#include "screens/PlayScreen.h"
#include <cmath>

TitleScreen::TitleScreen(Game& game) : Screen(game) {
    game_.audio().playMusic("music_intro", 0.5f);
    hasSave_ = game_.saves().hasSave();
    items_ = {"New run"};
    if (hasSave_) items_.push_back("Continue");
    items_.push_back("How to play");
    items_.push_back("Quit");
}

void TitleScreen::activate(int index) {
    game_.audio().play("click");
    const std::string& it = items_[index];
    if (it == "New run") game_.switchTo(std::make_unique<IntroScreen>(game_, 0));
    else if (it == "Continue") {
        SaveData d;
        if (game_.saves().readSave(d)) game_.switchTo(std::make_unique<PlayScreen>(game_, &d));
    } else if (it == "How to play") game_.switchTo(std::make_unique<HowToScreen>(game_));
    else game_.quit();
}

void TitleScreen::update(float dt) {
    weather_.update(dt, game_.time());
    int n = static_cast<int>(items_.size());
    for (Input::Act a : game_.input().presses()) {
        if (a == Input::Act::Up) { selected_ = (selected_ + n - 1) % n; game_.audio().play("click"); }
        if (a == Input::Act::Down) { selected_ = (selected_ + 1) % n; game_.audio().play("click"); }
        if (a == Input::Act::Accept || a == Input::Act::Pause) { activate(selected_); return; }
    }
}

void TitleScreen::draw() {
    float t = game_.time();
    ClearBackground(pal::Void);
    Backdrop::stars(t);
    Backdrop::gasGiant(470, 120, 120, t);
    Backdrop::moonHorizon();
    weather_.drawScreenSnow(0.6f);
    // logo: a cold glow behind the title, a shadowed wordmark, a breathing amber subtitle
    Ui::glow(320, 74, 150, pal::rgb(0x8FB4FF), 0.14f + 0.03f * std::sin(t * 1.5f));
    float tx = 320 - Ui::width("STRANDED", 4) / 2.0f;
    Ui::text("STRANDED", tx + 2, 58, pal::rgb(0x1A2238), 4, false);
    Ui::text("STRANDED", tx, 56, pal::Ink, 4, false);
    Ui::glow(320, 100, 70, pal::Amber, 0.10f + 0.05f * std::sin(t * 2.0f));
    Ui::textCentered("72 HOURS", 320, 94, pal::Amber, 2);
    Ui::rect(320 - 90, 112, 180, 1, withAlpha(pal::Amber, 0.5f));
    Ui::textCentered("A survival story on the frozen moon Kepler-X", 320, 118, pal::Dim);
    Ui::panel(244, 140, 152, 14 + items_.size() * 24.0f, pal::PanelLine, Color{11, 18, 32, 230});
    for (size_t i = 0; i < items_.size(); ++i) {
        float x = 260, y = 150 + i * 24.0f;
        bool f = selected_ == static_cast<int>(i);
        Ui::button(x, y, 120, 18, items_[i], f, t);
        int idx = static_cast<int>(i);
        game_.hits().add(Rectangle{x, y, 120, 18}, [this, idx] { activate(idx); }, [this, idx] { selected_ = idx; });
    }
    // ending collection: locked silhouettes until reached once
    const char* codes[] = {"F1", "F2", "F3", "F4", "E1", "E2", "E3"};
    const char* names[] = {"Signal Lost", "Lost on Ice", "Collapse", "Turned Back", "Together", "Empty Seats", "Alone"};
    float y = 268;
    int found = 0;
    for (const char* c : codes) found += game_.saves().endings().count(c) > 0;
    Ui::textCentered("ENDINGS FOUND " + std::to_string(found) + "/7", 320, y, pal::Dim2);
    for (int i = 0; i < 7; ++i) {
        bool got = game_.saves().endings().count(codes[i]) > 0;
        bool good = codes[i][0] == 'E';
        float x = 36 + i * 82.0f;
        Ui::panel(x, y + 12, 76, 30, got ? (good ? pal::Teal : pal::rgb(0x8A2E2E)) : pal::rgb(0x1C2840), got ? pal::Select : pal::rgb(0x0C1220));
        if (got) Ui::glow(x + 38, y + 27, 30, good ? pal::TealHi : pal::Red, 0.10f);
        Ui::textCentered(codes[i], x + 38, y + 17, got ? (good ? pal::TealHi : pal::RedSoft) : pal::Dim2);
        Ui::textCentered(got ? names[i] : "???", x + 38, y + 28, got ? pal::Ink : pal::Offline);
    }
    Ui::textCentered("Arrows choose  -  Z select  -  F11 fullscreen", 320, 330, pal::Dim2);
}
