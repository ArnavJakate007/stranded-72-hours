#include "screens/HowToScreen.h"
#include "core/Game.h"
#include "screens/Backdrop.h"
#include "screens/TitleScreen.h"

void HowToScreen::update(float) {
    if (game_.input().pressed(Input::Act::Accept) || game_.input().pressed(Input::Act::Back) || game_.input().pressed(Input::Act::Pause) || game_.input().clicked()) {
        game_.audio().play("click");
        game_.switchTo(std::make_unique<TitleScreen>(game_));
    }
}

void HowToScreen::draw() {
    ClearBackground(pal::Void);
    Backdrop::stars(game_.time());
    Ui::panel(40, 20, 560, 320, pal::PanelLineHi, pal::PanelFillDark);
    Ui::text("HOW TO PLAY", 56, 32, pal::Amber, 2);
    struct Col { const char* head; const char* lines[5]; };
    const Col cols[4] = {
        {"CONTROLS", {"Arrows or WASD: walk (tap to turn)", "Z: talk, pick up, use", "Hold Z: cut or set up", "X: back  -  Enter: menu", "Mouse: click to walk or use"}},
        {"YOUR GOALS", {"Contact HALCYON by Hour 24", "Radio works only at H8, H16, H24", "Keep the crew alive to Hour 72", "Follow the amber arrow and", "the Next Goal box"}},
        {"SURVIVAL", {"Four colony bars drain every hour", "Power units run one system each", "Outside, steps and zones cost", "suit oxygen. Get back before 0.", "Deep zones pay best, cost most"}},
        {"CREW", {"Walk up to crew and press Z", "Hand them jobs you could do", "yourself. Trusting them is how", "you get the best ending.", "Idle tags show unused help"}}};
    for (int i = 0; i < 4; ++i) {
        float x = 56 + (i % 2) * 270.0f, y = 62 + (i / 2) * 124.0f;
        Ui::text(cols[i].head, x, y, pal::Amber);
        for (int j = 0; j < 5; ++j) Ui::text(cols[i].lines[j], x, y + 16 + j * 13.0f, pal::Ink);
    }
    Ui::textCentered("Repairs are short puzzles. Failing one costs time, never the run.", 320, 300, pal::Soft);
    Ui::textCentered("Z or X to go back", 320, 318, pal::Dim2);
}
