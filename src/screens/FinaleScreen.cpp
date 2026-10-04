#include "screens/FinaleScreen.h"
#include "core/Game.h"
#include "gfx/Sprites.h"
#include "logic/Rng.h"
#include "rlgl.h"
#include "screens/Backdrop.h"
#include "screens/EndScreen.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float PAD_X = 400, PAD_Y = 262;   // where the shuttle's feet touch down
constexpr float LAND_TIME = 6.5f;
}  // namespace

FinaleScreen::FinaleScreen(Game& game, EndSummary summary) : Screen(game), summary_(std::move(summary)) {
    lines_.push_back(summary_.code == "E2" && summary_.crewAlive == 0 ? "fin_E2_alone" : "fin_" + summary_.code);
    if (summary_.roughLanding) lines_.push_back("fin_rough");
    lines_.push_back(summary_.truth ? "fin_truth_yes" : "fin_truth_no");
    walkers_.push_back("kiran");
    const char* crew[] = {"maya", "arjun", "elena", "hana"};
    for (int i = 0; i < summary_.crewAlive && i < 4; ++i) walkers_.push_back(crew[i]);
    game_.audio().playMusic("music_finale", 0.55f);
    game_.audio().play("whoosh");
}

float FinaleScreen::shuttleY() const {
    float touch = summary_.roughLanding ? LAND_TIME * 0.8f : LAND_TIME;
    float k = std::min(1.0f, time_ / touch);
    // a smooth landing slows over the pad; a rough one keeps its speed until it hits
    float ease = summary_.roughLanding ? 1 - std::pow(1 - k, 1.6f) : 1 - std::pow(1 - k, 3.0f);
    float y = -60 + (PAD_Y - 22 + 60) * ease;
    if (summary_.roughLanding && time_ > touch && time_ < touch + 0.6f) y -= std::sin((time_ - touch) / 0.6f * 3.14159f) * 10;   // one bounce
    return y;
}

void FinaleScreen::advance() {
    const DialogueEntry e = game_.data().dialogue.get(lines_[line_]);
    if (shown_ < e.text.size()) {
        shown_ = static_cast<float>(e.text.size());
        return;
    }
    game_.audio().play("click");
    shown_ = 0;
    if (++line_ >= static_cast<int>(lines_.size())) game_.switchTo(std::make_unique<EndScreen>(game_, summary_));
}

void FinaleScreen::update(float dt) {
    time_ += dt;
    shake_ = std::max(0.0f, shake_ - dt);
    flash_ = std::max(0.0f, flash_ - dt);
    fx_.update(dt);
    float sy = shuttleY();
    emit_ -= dt;
    if (time_ < LAND_TIME + 0.5f && emit_ <= 0) {
        emit_ = 0.016f;
        static Rng r(31);
        for (int s : {-4, 4})
            fx_.emit({PAD_X + s + (r.next() - 0.5f) * 3, sy + 36, (r.next() - 0.5f) * 20, 60 + r.next() * 40, 0.5f, 3, pal::rgb(0xFFB040), 0, 1, true, true, true});
        if (PAD_Y - (sy + 22) < 90)   // the plume kicks up snow from the pad
            for (int i = 0; i < 3; ++i) {
                float dir = r.next() < 0.5f ? -1.0f : 1.0f;
                fx_.emit({PAD_X + dir * r.next() * 10, PAD_Y + 2, dir * (60 + r.next() * 120), -(10 + r.next() * 30), 1.4f, 2 + r.next() * 3, Color{220, 230, 248, 200}, 30, 1.2f, true, false, false});
            }
    }
    if (!landed_ && time_ >= (summary_.roughLanding ? LAND_TIME * 0.8f : LAND_TIME)) {
        landed_ = true;
        shake_ = summary_.roughLanding ? 0.8f : 0.3f;
        flash_ = summary_.roughLanding ? 0.3f : 0.0f;
        game_.audio().play(summary_.roughLanding ? "impact" : "chime");
        fx_.burst(PAD_X, PAD_Y, summary_.roughLanding ? 70 : 30, summary_.roughLanding ? 140 : 70, 1.6f, 3, Color{220, 230, 248, 220}, 60);
        if (summary_.roughLanding) fx_.burst(PAD_X, PAD_Y - 6, 30, 160, 1.0f, 2, pal::rgb(0xFF9A40), 120, true);
    }
    if (line_ < 0 && time_ > LAND_TIME + 1.5f) line_ = 0;
    Input& in = game_.input();
    if (line_ >= 0 && line_ < static_cast<int>(lines_.size())) {
        DialogueEntry e = game_.data().dialogue.get(lines_[line_]);
        float ls = game_.settings().letterSeconds();
        shown_ = ls > 0 ? std::min<float>(static_cast<float>(e.text.size()), shown_ + dt / ls) : static_cast<float>(e.text.size());
        if (in.pressed(Input::Act::Accept) || in.pressed(Input::Act::Pause) || in.clicked()) advance();
    } else if (in.pressed(Input::Act::Back)) {
        game_.switchTo(std::make_unique<EndScreen>(game_, summary_));
    }
}

void FinaleScreen::draw() {
    float t = game_.time();
    bool dim = summary_.code == "E3";
    float sx = shake_ > 0 ? std::round(std::sin(t * 70) * 3 * std::min(1.0f, shake_ / 0.3f)) : 0;
    ClearBackground(pal::rgb(0x03050A));
    rlPushMatrix();
    rlTranslatef(sx, 0, 0);
    Backdrop::stars(t);
    Backdrop::gasGiant(110, 70, 70, t);
    // snowfield and the landing pad
    DrawRectangle(-10, 250, 660, 120, pal::rgb(0x7A8CB0));
    DrawRectangle(-10, 250, 660, 2, pal::rgb(0xB0BFD9));
    for (int i = 0; i < 30; ++i) DrawRectangle((i * 97) % 640, 258 + (i * 37) % 90, 8 + i % 12, 1, pal::rgb(0x95A6C8));
    DrawEllipse(static_cast<int>(PAD_X), static_cast<int>(PAD_Y + 6), 90, 14, pal::rgb(0x353D4E));
    DrawEllipseLines(static_cast<int>(PAD_X), static_cast<int>(PAD_Y + 6), 90, 14, pal::rgb(0xC4CFE2));
    // the base on the left, bright unless the colony went dark (E3)
    DrawRectangle(40, 214, 130, 44, pal::rgb(0x283246));
    DrawRectangle(40, 214, 130, 3, pal::rgb(0x3F4B66));
    for (int i = 0; i < 6; ++i) {
        DrawRectangle(52 + i * 19, 228, 10, 7, dim ? pal::rgb(0x2A303E) : pal::rgb(0xFFD08A));
        if (!dim) Ui::glow(57.0f + i * 19, 231, 10, pal::rgb(0xFFBE6E), 0.4f);
    }
    // landing lights around the pad (half of them dark on a rough landing)
    for (int i = 0; i < 6; ++i) {
        float a = i / 6.0f * 6.2831853f;
        float lx = PAD_X + std::cos(a) * 96, ly = PAD_Y + 6 + std::sin(a) * 16;
        DrawRectangle(static_cast<int>(lx) - 1, static_cast<int>(ly) - 4, 3, 4, pal::rgb(0x59606E));
        if (!summary_.roughLanding || i % 2 == 0) {
            bool pulse = static_cast<int>(t * 6 + i) % 6 == 0;
            DrawRectangle(static_cast<int>(lx) - 2, static_cast<int>(ly) - 6, 5, 3, pulse ? WHITE : pal::rgb(0xFFE08A));
            Ui::glow(lx, ly - 5, pulse ? 16.0f : 11.0f, pal::rgb(0xFFD070), 0.55f);
        }
    }
    fx_.draw();
    float flame = time_ < LAND_TIME ? 1.0f - 0.6f * std::min(1.0f, time_ / LAND_TIME) : std::max(0.0f, 1 - (time_ - LAND_TIME) * 2) * 0.4f;
    Backdrop::shuttle(PAD_X, shuttleY(), flame, t);
    if (landed_) {
        // the ramp drops, then the survivors walk from the base to the shuttle; Kiran leaves last
        float ramp = std::min(1.0f, (time_ - LAND_TIME) / 0.8f);
        if (ramp > 0) DrawTriangle(Vector2{PAD_X - 10, PAD_Y - 2}, Vector2{PAD_X - 10 - 22 * ramp, PAD_Y + 4}, Vector2{PAD_X - 10, PAD_Y + 4}, pal::rgb(0x8F99AB));
        for (size_t i = 0; i < walkers_.size(); ++i) {
            float start = LAND_TIME + 1.0f + (walkers_.size() - 1 - i) * 1.1f;
            float k = (time_ - start) / 5.0f;
            if (k <= 0 || k >= 1) continue;
            float x = 150 + (PAD_X - 34 - 150) * k;
            game_.sprites().drawCharacter(walkers_[i], x, 262, Dir::Right, static_cast<int>(t * 6) % 2 == 0, static_cast<int>(t * 6) % 4 < 2);
        }
    }
    rlPopMatrix();
    if (flash_ > 0) DrawRectangle(0, 0, 640, 360, withAlpha(WHITE, flash_ * 2));
    Backdrop::letterbox(std::min(1.0f, time_ / 1.2f));
    if (time_ < 1.5f) DrawRectangle(0, 0, 640, 360, withAlpha(BLACK, 1 - time_ / 1.5f));
    if (time_ > 1 && time_ < 5.5f) {
        float a = std::min(1.0f, std::min(time_ - 1, 5.5f - time_));
        Ui::textCentered("HOUR 72", 320, 46, withAlpha(pal::Amber, a), 2);
        Ui::textCentered("The rescue shuttle breaks through the clouds", 320, 66, withAlpha(pal::Ink, a));
    }
    if (line_ >= 0 && line_ < static_cast<int>(lines_.size())) {
        DialogueEntry e = game_.data().dialogue.get(lines_[line_]);
        const float x = 70, y = 270, w = 500, h = 56;
        Ui::panel(x, y, w, h, pal::rgb(0x6B5A2A), Color{11, 18, 32, 235});
        std::string visible = e.text.substr(0, std::min(e.text.size(), static_cast<size_t>(shown_)));
        size_t used = 0;
        auto lines = Ui::wrap(e.text, static_cast<int>(w - 28));
        for (size_t i = 0; i < lines.size() && i < 4; ++i) {
            std::string part = used < visible.size() ? visible.substr(used, std::min(lines[i].size(), visible.size() - used)) : "";
            Ui::text(part, x + 14, y + 9 + i * 11.0f, pal::Ink);
            used += lines[i].size() + 1;
        }
        if (shown_ >= e.text.size() && static_cast<int>(t * 2) % 2 == 0) Ui::downTriangle(x + w - 16, y + h - 10, pal::Amber);
    }
    Ui::textRight(line_ >= 0 ? "Z next" : "X skip", 632, 348, pal::Dim2);
}
