#include "screens/IntroScreen.h"
#include "core/Game.h"
#include "gfx/Sprites.h"
#include "logic/Rng.h"
#include "rlgl.h"
#include "screens/Backdrop.h"
#include "screens/PlayScreen.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float IMPACT_TIME = 1.8f;
constexpr float IMPACT_X = 380, IMPACT_Y = 286;
const char* kTitle = "STRANDED";
constexpr int kTitleScale = 6;

float letterLand(int i) { return 0.35f + i * 0.11f + 0.15f; }
float ease(float k) { k = std::clamp(k, 0.0f, 1.0f); return 1 - (1 - k) * (1 - k) * (1 - k); }

// Dark storm clouds that scroll upward past the camera during the descent.
void stormClouds(float t, float density, float scroll) {
    Rng r(9);
    for (int i = 0; i < 26; ++i) {
        float x = r.next() * 760 - 60, base = r.next() * 900, w = 60 + r.next() * 120, h = 18 + r.next() * 22;
        float y = std::fmod(base - t * scroll * (0.6f + r.next() * 0.8f), 900.0f);
        if (y < 0) y += 900;
        y -= 200;
        if (r.next() > density) continue;
        unsigned char g = static_cast<unsigned char>(28 + r.next() * 30);
        DrawEllipse(static_cast<int>(x), static_cast<int>(y), w, h, Color{g, static_cast<unsigned char>(g + 4), static_cast<unsigned char>(g + 16), 235});
        DrawEllipse(static_cast<int>(x - w * 0.3f), static_cast<int>(y - h * 0.4f), w * 0.5f, h * 0.8f, Color{static_cast<unsigned char>(g + 10), static_cast<unsigned char>(g + 14), static_cast<unsigned char>(g + 28), 235});
    }
}

void shootingStar(float t, float period, float offset) {
    float ph = std::fmod(t + offset, period);
    if (ph > 0.7f) return;
    float k = ph / 0.7f;
    float x = 620 - k * 420 - offset * 40, y = 20 + k * 120 + offset * 10;
    for (int i = 0; i < 14; ++i) DrawPixel(static_cast<int>(x + i * 3), static_cast<int>(y - i * 0.85f), withAlpha(WHITE, (1 - i / 14.0f) * (1 - k * 0.5f)));
    Ui::glow(x, y, 6, WHITE, 0.5f);
}
}  // namespace

IntroScreen::IntroScreen(Game& game, int startSlide) : Screen(game) {
    slides_ = {{Scene::Station, {"intro_1", "intro_2"}},
               {Scene::Descent, {"intro_3", "intro_4"}},
               {Scene::Radio, {"intro_5", "intro_6", "intro_7"}},
               {Scene::Crash, {"intro_8"}},
               {Scene::Dark, {"intro_9", "intro_10", "intro_11"}},
               {Scene::Title, {}}};
    slide_ = std::clamp(startSlide, 0, static_cast<int>(slides_.size()) - 1);
    game_.audio().playMusic("music_intro", 0.6f);
    enterSlide();
}

void IntroScreen::startGame() {
    game_.audio().play("click");
    game_.audio().stopMusic();
    game_.switchTo(std::make_unique<PlayScreen>(game_, nullptr));
}

void IntroScreen::enterSlide() {
    sceneTime_ = 0;
    line_ = 0;
    shown_ = 0;
    fx_.clear();
    Scene s = slides_[slide_].scene;
    if (s == Scene::Descent) game_.audio().play("whoosh");
    if (s == Scene::Crash) game_.audio().play("alarm");
    if (s == Scene::Title) { lettersLanded_ = 0; slammed_ = false; }
}

void IntroScreen::next() {
    const Slide& sl = slides_[slide_];
    if (sl.scene == Scene::Title) {
        if (sceneTime_ < 3.0f) sceneTime_ = 3.0f;   // first press finishes the card, the next one starts
        else startGame();
        return;
    }
    if (sl.scene == Scene::Crash && sceneTime_ < IMPACT_TIME + 0.8f) return;   // let the crash play out
    DialogueEntry e = game_.data().dialogue.get(sl.lines[line_]);
    if (shown_ < e.text.size()) {
        shown_ = static_cast<float>(e.text.size());
        return;
    }
    game_.audio().play("click");
    shown_ = 0;
    if (++line_ < static_cast<int>(sl.lines.size())) return;
    if (slide_ + 1 < static_cast<int>(slides_.size())) {
        ++slide_;
        enterSlide();
    }
}

void IntroScreen::updateScene(float dt) {
    float k = sceneTime_;
    static Rng r(123);
    emit_ -= dt;
    bool tick = emit_ <= 0;
    if (tick) emit_ = 0.02f;
    switch (slides_[slide_].scene) {
        case Scene::Descent:
            if (tick) {   // engine exhaust trails up behind the falling lander
                float lx = 320 + std::sin(k * 1.3f) * 14, ly = 150 + std::sin(k * 2.1f) * 4;
                fx_.emit({lx + (r.next() - 0.5f) * 4, ly - 14, (r.next() - 0.5f) * 14, -(90 + r.next() * 60), 0.6f, 3, pal::rgb(0xFFB040), 0, 0.5f, true, true, true});
                fx_.emit({lx + (r.next() - 0.5f) * 8, ly - 18, (r.next() - 0.5f) * 20, -(60 + r.next() * 40), 1.2f, 4, Color{120, 130, 150, 120}, 0, 0.5f, true, false, false});
            }
            if (line_ >= 1 && !warned_) { warned_ = true; game_.audio().play("alarm"); }
            break;
        case Scene::Radio:
            if (tick && r.next() < 0.3f) shake_ = std::max(shake_, 0.06f);
            break;
        case Scene::Crash: {
            if (k > 0.4f && !struck1_) { struck1_ = true; flash_ = 0.25f; game_.audio().play("thunder"); }
            if (k > 1.2f && !struck2_) { struck2_ = true; flash_ = 0.2f; game_.audio().play("thunder"); }
            float p = std::min(1.0f, k / IMPACT_TIME);
            float lx = -40 + (IMPACT_X + 40) * p, ly = 40 + (IMPACT_Y - 40) * p * p;
            if (!impacted_ && tick) {   // smoke and fire stream from the falling lander
                fx_.emit({lx, ly, -40 + (r.next() - 0.5f) * 30, -20 + (r.next() - 0.5f) * 30, 1.6f, 5, Color{70, 74, 88, 200}, -10, 0.8f, true, false, false});
                fx_.emit({lx, ly, -60 + (r.next() - 0.5f) * 30, (r.next() - 0.5f) * 30, 0.4f, 4, pal::rgb(0xFF7A30), 0, 1, true, true, true});
            }
            if (!impacted_ && k >= IMPACT_TIME) {
                impacted_ = true;
                shake_ = 1.0f;
                flash_ = 0.6f;
                game_.audio().play("impact");
                game_.audio().play("crash");
                fx_.burst(IMPACT_X, IMPACT_Y, 90, 260, 1.4f, 3, pal::rgb(0xFFB040), 220, true);
                fx_.burst(IMPACT_X, IMPACT_Y, 50, 180, 1.8f, 3, pal::rgb(0x8F99AB), 260);
                fx_.burst(IMPACT_X, IMPACT_Y, 60, 140, 1.6f, 3, Color{220, 230, 248, 230}, 120);
            }
            if (impacted_ && tick) {   // the wreck burns
                fx_.emit({IMPACT_X + (r.next() - 0.5f) * 20, IMPACT_Y - 6, (r.next() - 0.5f) * 10, -(20 + r.next() * 20), 2.5f, 5 + r.next() * 4, Color{60, 64, 78, 170}, -4, 0.3f, true, false, false});
                fx_.emit({IMPACT_X + (r.next() - 0.5f) * 14, IMPACT_Y - 4, (r.next() - 0.5f) * 10, -(30 + r.next() * 30), 0.5f, 3, pal::rgb(0xFF8A30), 0, 0.5f, true, true, true});
            }
            break;
        }
        case Scene::Dark:
            beat_ -= dt;
            if (beat_ <= 0 && k < 6) { beat_ = 1.15f; game_.audio().play("heartbeat"); }
            break;
        case Scene::Title: {
            int n = static_cast<int>(std::string(kTitle).size());
            while (lettersLanded_ < n && k >= letterLand(lettersLanded_)) {
                bool last = lettersLanded_ == n - 1;
                game_.audio().play(last ? "impact" : "bump");
                shake_ = std::max(shake_, last ? 0.4f : 0.12f);
                float w = static_cast<float>(Ui::width(kTitle, kTitleScale)), x0 = 320 - w / 2;
                float lx = x0 + Ui::width(std::string(kTitle).substr(0, lettersLanded_), kTitleScale) + 15;
                fx_.burst(lx, 132, last ? 30 : 10, last ? 120 : 60, 0.8f, 2, Color{220, 230, 248, 230}, 120);
                ++lettersLanded_;
            }
            if (!slammed_ && k >= 1.6f) {
                slammed_ = true;
                flash_ = 0.35f;
                shake_ = 0.5f;
                game_.audio().play("impact");
                game_.audio().play("chime");
                fx_.burst(320, 160, 60, 200, 1.2f, 2, pal::rgb(0xFFB040), 60, true);
            }
            if (tick && r.next() < 0.5f)   // embers drifting up
                fx_.emit({r.next() * 640, 370, (r.next() - 0.5f) * 20, -(20 + r.next() * 40), 6, 2, pal::rgb(0xFF9A40), 0, 0, true, false, true});
            break;
        }
        default:
            if (tick && r.next() < 0.1f) fx_.emit({r.next() * 640, 0, 0, 0, 0.01f, 1, BLANK});
            break;
    }
}

void IntroScreen::update(float dt) {
    sceneTime_ += dt;
    shake_ = std::max(0.0f, shake_ - dt);
    flash_ = std::max(0.0f, flash_ - dt);
    fx_.update(dt);
    updateScene(dt);
    const Slide& sl = slides_[slide_];
    if (!sl.lines.empty()) {
        DialogueEntry e = game_.data().dialogue.get(sl.lines[line_]);
        float ls = game_.settings().letterSeconds();
        bool waiting = sl.scene == Scene::Crash && sceneTime_ < IMPACT_TIME + 0.4f;
        if (!waiting) shown_ = ls > 0 ? std::min<float>(static_cast<float>(e.text.size()), shown_ + dt / ls) : static_cast<float>(e.text.size());
    }
    Input& in = game_.input();
    if (in.pressed(Input::Act::Back)) {
        if (sl.scene == Scene::Title) startGame();
        else { slide_ = static_cast<int>(slides_.size()) - 1; enterSlide(); }   // skip straight to the title card
        return;
    }
    if (in.pressed(Input::Act::Accept) || in.pressed(Input::Act::Pause) || in.clicked()) next();
}

// ---------------------------------------------------------------- scenes

void IntroScreen::drawStation(float t, float k) {
    Backdrop::parallaxStars(t, -8, 0);
    shootingStar(t, 3.2f, 0);
    shootingStar(t, 4.7f, 1.3f);
    Ui::glow(440, 200, 220, pal::rgb(0x7D55B4), 0.25f);
    Backdrop::gasGiant(440, 200, 150, t);
    float bob = std::sin(t * 0.8f) * 3;
    Backdrop::bigStation(190 + k * 4, 112 + bob, t);
    // the lander docked under the station, waiting
    Backdrop::lander(190 + k * 4, 140 + bob, 0, false, t);
    std::string cap = "HALCYON STATION  //  ORBIT OF KEPLER PRIME";
    size_t n = std::min(cap.size(), static_cast<size_t>(k * 30));
    Ui::text(cap.substr(0, n), 20, 40, pal::Soft);
    if (n < cap.size() || static_cast<int>(t * 3) % 2) Ui::rect(20 + Ui::width(cap.substr(0, n)) + 1.0f, 40, 4, 7, pal::Amber);
    Ui::text("40 CREW  -  NEAREST HELP: 7 LIGHT-YEARS", 20, 52, pal::Dim2);
}

void IntroScreen::drawDescent(float t, float k) {
    ClearBackground(pal::rgb(0x050812));
    Backdrop::parallaxStars(t, 0, -90 - k * 20);
    // the moon swells up from below as the lander falls
    float rise = std::min(1.0f, k / 8.0f);
    Ui::disc(320, static_cast<int>(1200 - rise * 300), 900, pal::rgb(0x5A6D92));
    Ui::disc(320, static_cast<int>(1204 - rise * 300), 896, pal::rgb(0x8FA3C8));
    Ui::glow(320, 300 - rise * 300 + 300, 300, pal::rgb(0x8FB4FF), 0.15f);
    stormClouds(t, std::min(1.0f, 0.3f + k * 0.1f), 180 + k * 30);
    fx_.draw();
    float lx = 320 + std::sin(k * 1.3f) * 14, ly = 150 + std::sin(k * 2.1f) * 4;
    Backdrop::lander(lx, ly, 1.5708f + std::sin(k * 1.7f) * 0.15f, true, t);
    // flight HUD
    int alt = std::max(0, 18400 - static_cast<int>(k * 900));
    Ui::panel(500, 60, 120, 56, pal::rgb(0x2C5A80), Color{6, 12, 22, 200});
    Ui::text("ARK-7 DESCENT", 508, 66, pal::Ice);
    Ui::text("ALT", 508, 80, pal::Dim);
    Ui::textRight(std::to_string(alt) + " m", 612, 80, pal::Ink);
    Ui::text("V/S", 508, 92, pal::Dim);
    Ui::textRight("-" + std::to_string(310 + static_cast<int>(std::sin(t * 3) * 6)) + " m/s", 612, 92, pal::Ink);
    Ui::text("WIND", 508, 104, pal::Dim);
    Ui::textRight(std::to_string(60 + static_cast<int>(k * 9)) + " kn", 612, 104, line_ >= 1 ? pal::Red : pal::Amber);
    if (line_ >= 1 && static_cast<int>(t * 3) % 2 == 0) {   // flashes no faster than 3 times a second
        Ui::rect(0, 30, 640, 4, pal::Red); Ui::rect(0, 326, 640, 4, pal::Red); Ui::rect(0, 30, 4, 300, pal::Red); Ui::rect(636, 30, 4, 300, pal::Red);
        Ui::panel(220, 52, 200, 30, pal::Red, Color{40, 6, 8, 220});
        Ui::textCentered("STORM WARNING", 320, 59, pal::Red, 2);
    }
}

void IntroScreen::drawRadio(float t, float k, bool speaking) {
    ClearBackground(pal::rgb(0x0A0D16));
    // cockpit window with snow rushing past
    Ui::rect(70, 38, 500, 150, pal::rgb(0x06080F));
    BeginScissorMode(70, 38, 500, 150);
    Rng r(5);
    for (int i = 0; i < 90; ++i) {
        float a = r.next() * 6.2831853f, sp = 0.4f + r.next();
        float d = std::fmod(r.next() * 300 + t * 180 * sp, 300.0f);
        float x = 320 + std::cos(a) * d, y = 113 + std::sin(a) * d * 0.5f;
        float len = d * 0.08f;
        DrawLineEx(Vector2{x, y}, Vector2{x - std::cos(a) * len, y - std::sin(a) * len * 0.5f}, d > 120 ? 2.0f : 1.0f, Color{210, 222, 245, static_cast<unsigned char>(std::min(255.0f, d * 1.4f))});
    }
    if (static_cast<int>(k * 0.8f) % 3 == 2) Backdrop::lightning(180 + std::fmod(k * 97, 260), 38, 150, static_cast<unsigned>(k), pal::rgb(0xC8D8FF));
    EndScissorMode();
    // frame and struts
    Ui::rect(66, 34, 508, 4, pal::rgb(0x2A3346)); Ui::rect(66, 188, 508, 6, pal::rgb(0x2A3346));
    Ui::rect(66, 34, 4, 160, pal::rgb(0x2A3346)); Ui::rect(570, 34, 4, 160, pal::rgb(0x2A3346)); Ui::rect(318, 34, 4, 156, pal::rgb(0x2A3346));
    // dashboard
    Ui::rect(40, 194, 560, 52, pal::rgb(0x141A26));
    for (int i = 0; i < 14; ++i) Ui::rect(56 + i * 20.0f, 200, 12, 6, static_cast<int>(t * 3 + i) % 3 ? pal::rgb(0x2A3346) : pal::Red);
    // oscilloscope: the voice on the radio
    Ui::rect(360, 200, 200, 40, pal::rgb(0x061A12));
    float amp = speaking ? 14.0f : 2.0f;
    for (int x = 0; x < 198; ++x) {
        float y = std::sin(x * 0.18f + t * 18) * amp * std::sin(x * 0.03f + t * 2) + std::sin(x * 0.7f + t * 40) * amp * 0.25f;
        DrawPixel(361 + x, 220 + static_cast<int>(y), pal::rgb(0x5EFF8A));
    }
    Ui::text("ARK-7  -  DESCENT 00:19:40", 60, 212, pal::Amber);
    Ui::text("STORM CELL AHEAD", 60, 226, static_cast<int>(t * 2) % 2 ? pal::Red : pal::RedSoft);
    if (static_cast<int>(t * 1.5f) % 2) Ui::glow(320, 120, 260, pal::Red, 0.08f);
}

void IntroScreen::drawCrash(float t, float k) {
    ClearBackground(pal::rgb(0x0B0F1A));
    stormClouds(t, 0.7f, 30);
    if (struck1_ && k < 0.75f) Backdrop::lightning(150, 0, 260, 11, pal::rgb(0xDDE6FF));
    if (struck2_ && k < 1.45f) Backdrop::lightning(520, 0, 280, 23, pal::rgb(0xDDE6FF));
    Rng r(static_cast<uint32_t>(t * 30));
    for (int i = 0; i < 140; ++i) Ui::rect(std::floor(r.next() * 640), std::floor(r.next() * 360), 1, 6 + std::floor(r.next() * 10), Color{200, 215, 240, 110});
    Backdrop::moonHorizon();
    fx_.draw();
    if (!impacted_) {
        float p = std::min(1.0f, k / IMPACT_TIME);
        float lx = -40 + (IMPACT_X + 40) * p, ly = 40 + (IMPACT_Y - 40) * p * p;
        Backdrop::lander(lx, ly, 0.5f + k * 2.2f, true, t);
    } else {
        // wreck in the snow and the expanding shockwave
        Backdrop::lander(IMPACT_X, IMPACT_Y - 2, 0.35f, false, t);
        float s = k - IMPACT_TIME;
        if (s < 1.0f) {
            Ui::ring(static_cast<int>(IMPACT_X), static_cast<int>(IMPACT_Y), static_cast<int>(10 + s * 260), withAlpha(pal::rgb(0xFFE0B0), 1 - s), 3);
            Ui::ring(static_cast<int>(IMPACT_X), static_cast<int>(IMPACT_Y), static_cast<int>(s * 160), withAlpha(WHITE, (1 - s) * 0.6f), 2);
        }
        Ui::glow(IMPACT_X, IMPACT_Y - 4, 30 + std::sin(t * 9) * 4, pal::rgb(0xFF7A30), 0.45f);
    }
}

void IntroScreen::drawDark(float t, float k) {
    ClearBackground(pal::rgb(0x020306));
    // the wrecked cabin, lit only by the red emergency lamp
    float lamp = 0.5f + 0.5f * std::sin(t * 2.2f);
    Ui::rect(140, 120, 360, 130, pal::rgb(0x10131C));
    for (int i = 0; i < 9; ++i) Ui::rect(150 + i * 40.0f, 122, 2, 126, pal::rgb(0x1A1F2B));
    Ui::rect(140, 232, 360, 18, pal::rgb(0x1C212C));
    Ui::glow(320, 130, 140, pal::rgb(0xC0281E), 0.25f + 0.15f * lamp);
    Ui::rect(314, 122, 12, 5, lamp > 0.5f ? pal::rgb(0xFF4A3A) : pal::rgb(0x7A1A16));
    Ui::glow(320, 190, 46, pal::rgb(0xBED2FF), 0.15f);
    game_.sprites().drawCharacter("kiran", 320, 232, Dir::Down, false, false);
    bool flick = static_cast<int>(t * 7) % 9 != 0;
    if (k > 1.6f && flick) {
        Ui::textCentered("EMERGENCY POWER 9%", 320, 70, pal::Red, 2);
        Ui::glow(320, 76, 90, pal::Red, 0.12f);
    }
    // eyelids open, blink once, open again
    float open = k < 1.0f ? ease(k) * 0.6f : k < 1.25f ? 0.6f - (k - 1.0f) * 2.4f : ease((k - 1.25f) / 1.2f);
    open = std::clamp(open, 0.0f, 1.0f);
    int lid = static_cast<int>((1 - open) * 182);
    if (lid > 0) {
        DrawRectangle(0, 0, 640, lid, BLACK);
        DrawRectangle(0, 360 - lid, 640, lid, BLACK);
    }
    // vignette iris
    DrawRing(Vector2{320, 180}, 140 + open * 240, 900, 0, 360, 64, BLACK);
}

void IntroScreen::drawTitle(float t, float k) {
    ClearBackground(pal::Void);
    Backdrop::parallaxStars(t, -4, 0);
    Ui::glow(470, 300, 200, pal::rgb(0x7D55B4), 0.2f);
    Backdrop::gasGiant(470, 300, 130, t);
    Backdrop::moonHorizon();
    fx_.draw();
    std::string title = kTitle;
    float w = static_cast<float>(Ui::width(title, kTitleScale)), x0 = 320 - w / 2, y = 108;
    for (size_t i = 0; i < title.size(); ++i) {
        float land = letterLand(static_cast<int>(i)), start = land - 0.15f;
        if (k < start) continue;
        float p = std::min(1.0f, (k - start) / 0.15f);
        float ly = y - (1 - p) * 120;
        float lx = x0 + Ui::width(title.substr(0, i), kTitleScale);
        std::string ch(1, title[i]);
        Ui::text(ch, lx + 3, ly + 3, pal::rgb(0x1A2238), kTitleScale, false);
        Ui::text(ch, lx, ly, k - land < 0.12f && k >= land ? WHITE : pal::Ink, kTitleScale, false);
    }
    if (k >= 1.6f) {
        float s = std::min(1.0f, (k - 1.6f) / 0.12f);
        int sc = s < 1 ? 4 : 3;
        Ui::textCentered("72 HOURS", 320, 160 - (sc - 3) * 4.0f, pal::Amber, sc);
        Ui::rect(320 - 120, 186, 240, 1, withAlpha(pal::Amber, s));
    }
    if (k >= 2.2f) {
        float a = std::min(1.0f, (k - 2.2f) * 2);
        Ui::textCentered("CONTACT HALCYON", 320, 198, withAlpha(pal::Soft, a));
        Ui::glow(320, 218, 60, pal::Red, 0.18f * a);
        bool flick = static_cast<int>(t * 9) % 13 != 0;
        Ui::textCentered("24:00:00", 320, 210, withAlpha(flick ? pal::rgb(0xFF5D52) : pal::rgb(0x7A3A3A), a), 2);
        Ui::textCentered("Survive to Hour 72. Bring everyone home.", 320, 234, withAlpha(pal::Dim, a));
    }
    if (k >= 3.0f && static_cast<int>(t * 2) % 2 == 0) Ui::textCentered("PRESS Z TO BEGIN", 320, 262, pal::Amber);
}

void IntroScreen::drawTextBox(float t) {
    const Slide& sl = slides_[slide_];
    if (sl.lines.empty()) return;
    if (sl.scene == Scene::Crash && sceneTime_ < IMPACT_TIME + 0.4f) return;
    DialogueEntry e = game_.data().dialogue.get(sl.lines[line_]);
    const float x = 60, y = 248, w = 520, h = 78;
    bool narr = e.speaker.empty();
    Ui::panel(x, y, w, h, narr ? pal::rgb(0x6B5A2A) : speakerColor(e.speaker), Color{11, 18, 32, 235});
    float tx = x + 14;
    if (!narr) {
        Icons::portrait(e.speaker, x + 10, y + 10);
        Ui::text(e.speaker == "voss" ? "DIRECTOR VOSS - HALCYON" : (e.speaker == "hana" ? "HANA - PILOT" : e.speaker), x + 46, y + 9, speakerColor(e.speaker));
        tx = x + 46;
    }
    std::string visible = e.text.substr(0, std::min(e.text.size(), static_cast<size_t>(shown_)));
    size_t used = 0;
    auto lines = Ui::wrap(e.text, static_cast<int>(w - (tx - x) - 16));
    for (size_t i = 0; i < lines.size(); ++i) {
        std::string part = used < visible.size() ? visible.substr(used, std::min(lines[i].size(), visible.size() - used)) : "";
        Ui::text(part, tx, y + (narr ? 12 : 21) + i * 12.0f, pal::Ink);
        used += lines[i].size() + 1;
    }
    if (shown_ >= e.text.size() && static_cast<int>(t * 2) % 2 == 0) Ui::downTriangle(x + w - 16, y + h - 12, pal::Amber);
}

void IntroScreen::draw() {
    float t = game_.time(), k = sceneTime_;
    const Slide& sl = slides_[slide_];
    ClearBackground(pal::rgb(0x03050A));
    float sx = 0, sy = 0;
    if (shake_ > 0) {
        float m = std::min(1.0f, shake_) * 6;
        sx = std::round(std::sin(t * 83) * m);
        sy = std::round(std::cos(t * 71) * m * 0.6f);
    }
    rlPushMatrix();
    rlTranslatef(sx, sy, 0);
    switch (sl.scene) {
        case Scene::Station: drawStation(t, k); fx_.draw(); break;
        case Scene::Descent: drawDescent(t, k); break;
        case Scene::Radio: drawRadio(t, k, shown_ < game_.data().dialogue.get(sl.lines[line_]).text.size()); break;
        case Scene::Crash: drawCrash(t, k); break;
        case Scene::Dark: drawDark(t, k); break;
        case Scene::Title: drawTitle(t, k); break;
    }
    rlPopMatrix();
    if (flash_ > 0) DrawRectangle(0, 0, 640, 360, withAlpha(pal::rgb(0xFFF4E0), std::min(1.0f, flash_ * 2.2f)));
    if (sl.scene != Scene::Title) Backdrop::letterbox(std::min(1.0f, (slide_ > 0 ? 1.0f : k / 1.0f)));
    if (k < 0.5f && sl.scene != Scene::Title && !(sl.scene == Scene::Dark)) DrawRectangle(0, 0, 640, 360, withAlpha(BLACK, 1 - k / 0.5f));
    drawTextBox(t);
    if (sl.scene != Scene::Title) {
        Ui::textRight("Z next  -  X skip", 632, 348, pal::Dim2);
        for (int i = 0; i < static_cast<int>(slides_.size()) - 1; ++i) Ui::rect(8 + i * 10.0f, 350, 7, 3, i == slide_ ? pal::Amber : i < slide_ ? pal::Dim : pal::rgb(0x26324A));
    }
}
