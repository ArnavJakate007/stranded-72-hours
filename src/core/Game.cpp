#include "core/Game.h"
#include "logic/PowerException.h"
#include "screens/EndScreen.h"
#include "screens/FinaleScreen.h"
#include "screens/IntroScreen.h"
#include "screens/PlayScreen.h"
#include "screens/TitleScreen.h"
#include <algorithm>
#include <cmath>
#include <filesystem>

Game::Game() : saves_(".") {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(VW * 2, VH * 2, "STRANDED: 72 Hours");
    SetWindowMinSize(VW, VH);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);   // Esc must not quit by accident; Enter opens the menu
    canvas_ = LoadRenderTexture(VW, VH);
    SetTextureFilter(canvas_.texture, TEXTURE_FILTER_POINT);
    font_.load();
    Ui::init(&font_);
    audio_.init();
    try {
        data_.loadAll("data");
    } catch (const DataError& e) {
        dataError_ = e.what();   // shown on screen, then the game closes (SRS section 6)
        return;
    }
    settings_.clockMultiplier = data_.balance.get("default_clock_multiplier");
    sprites_.build();
    screen_ = std::make_unique<TitleScreen>(*this);
}

Game::~Game() {
    screen_.reset();
    pending_.reset();
    sprites_.unload();
    Ui::shutdown();
    UnloadRenderTexture(canvas_);
    CloseWindow();
}

void Game::run() {
    while (!WindowShouldClose() && !quit_) {
        if (!dataError_.empty()) {
            drawDataError();
            continue;
        }
        frame();
    }
}

void Game::frame() {
    float dt = std::min(GetFrameTime(), 0.05f);   // animation uses time passed, not frames (NFR-3)
    time_ += dt;
    if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

    // whole-number scaling with black bars (SRS T-2)
    float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
    float scale = std::max(1.0f, std::floor(std::min(sw / VW, sh / VH)));
    if (sw < VW || sh < VH) scale = std::min(sw / VW, sh / VH);
    float ox = std::floor((sw - VW * scale) / 2), oy = std::floor((sh - VH * scale) / 2);
    input_.setViewport(scale, ox, oy);
    input_.poll();
    audio_.setEnabled(settings_.sound);
    audio_.update();

    // mouse over and clicks on last frame's buttons
    hits_.beginFrame();
    if (input_.mouseInside()) {
        if (const HitList::Hit* h = hits_.topAt(input_.mouse())) {
            if (h->onHover) h->onHover();
            if (input_.clicked()) {
                input_.consumeClick();
                if (h->onClick) h->onClick();
            }
        }
    }
    if (pending_) screen_ = std::move(pending_);
    screen_->update(dt);
    if (pending_) screen_ = std::move(pending_);

    BeginTextureMode(canvas_);
    ClearBackground(pal::Bg);
    screen_->draw();
    EndTextureMode();

    BeginDrawing();
    ClearBackground(BLACK);
    DrawTexturePro(canvas_.texture, Rectangle{0, 0, static_cast<float>(VW), -static_cast<float>(VH)},
                   Rectangle{ox, oy, VW * scale, VH * scale}, Vector2{0, 0}, 0, WHITE);
    EndDrawing();
}

void Game::shot(const std::string& path) {
    Image img = LoadImageFromTexture(canvas_.texture);
    ImageFlipVertical(&img);
    ExportImage(img, path.c_str());
    UnloadImage(img);
}

int Game::smokeTest(const std::string& folder) {
    if (!dataError_.empty()) return 2;
    std::filesystem::create_directories(folder);
    saves_ = SaveManager(folder);   // keep the test away from the real save and endings
    auto frames = [this](int n) { for (int i = 0; i < n; ++i) frame(); };   // runs to the end even if the window is asked to close
    frames(30);
    shot(folder + "/01_title.png");
    const std::pair<int, int> introShots[] = {{0, 100}, {1, 150}, {2, 90}, {3, 125}, {4, 200}, {5, 250}};
    for (const auto& is : introShots) {
        switchTo(std::make_unique<IntroScreen>(*this, is.first));
        frames(is.second);
        shot(folder + "/02_intro_" + std::to_string(is.first) + ".png");
    }
    auto playOwned = std::make_unique<PlayScreen>(*this, nullptr);
    PlayScreen* play = playOwned.get();
    switchTo(std::move(playOwned));
    frames(120);
    shot(folder + "/03_base_dark.png");
    Session& s = play->session();
    s.dialogue().clear();
    s.debugRestorePower();
    s.toggleSystem("oxygen");
    s.toggleSystem("medical");
    s.dialogue().clear();
    frames(90);
    shot(folder + "/04_base_lit.png");
    play->debugOpenPanel("power");
    frames(20);
    shot(folder + "/05_power_panel.png");
    play->debugClosePanel();
    const std::pair<PuzzleKind, const char*> puzzles[] = {{PuzzleKind::Wire, "06_wire"}, {PuzzleKind::Pipe, "07_pipe"}, {PuzzleKind::Tune, "08_tune"}, {PuzzleKind::Sequence, "09_sequence"}, {PuzzleKind::Cut, "09b_cut"}};
    for (const auto& p : puzzles) {
        play->debugPuzzle(p.first);
        frames(40);
        shot(folder + "/" + p.second + ".png");
        play->debugClosePuzzle();
    }
    const std::pair<IVec, const char*> places[] = {{{78, 58}, "10_wreck_field"}, {{14, 46}, "11_ice_cave"}, {{51, 13}, "12_ridge"}, {{70, 24}, "13_landing_gate"}};
    for (const auto& p : places) {
        s.debugTeleport(p.first);
        s.dialogue().clear();
        frames(60);
        shot(folder + "/" + p.second + ".png");
    }
    s.debugTeleport({50, 30});
    play->debugPause(true);
    frames(20);
    shot(folder + "/14_menu.png");
    play->debugPause(false);
    for (const char* id : {"journal", "map", "crew", "cargo"}) {
        play->debugOpenPanel(id);
        frames(20);
        shot(folder + std::string("/15_panel_") + id + ".png");
        play->debugClosePanel();
    }
    // late-game states are built from a save so every Act 3/4 prop can be photographed
    auto lateSave = [](Session& from, float hour, const std::string& flags) {
        SaveData d = from.snapshot();
        d.setNum("hour", hour);
        d.set("clock_running", "1");
        d.set("powered", "1");
        d.set("leak_sealed", "1");
        d.set("flags", flags);
        d.setNum("contact_hour", 8);
        d.setNum("battery", 80);
        d.setNum("grow", 9);
        d.setNum("kiran_health", 64);
        d.set("braced", "11");
        for (const char* id : {"oxygen", "medical", "heater", "comms", "greenhouse", "purifier", "beacon"}) d.set(std::string("sys_") + id + "_built", "1");
        for (const char* id : {"oxygen", "heater", "greenhouse", "purifier"}) d.set(std::string("sys_") + id + "_on", "1");
        d.set("crew_hana_state", "idle");
        d.setNum("crew_hana_x", 58);
        d.setNum("crew_hana_y", 31);
        return d;
    };
    SaveData storm = lateSave(s, 49, "111111101111111110110000");
    auto stormOwned = std::make_unique<PlayScreen>(*this, &storm);
    PlayScreen* sp = stormOwned.get();
    switchTo(std::move(stormOwned));
    frames(2);
    sp->session().dialogue().clear();
    sp->debugStorm(true);
    sp->session().debugTeleport({52, 44});
    frames(150);
    shot(folder + "/16_storm_outside.png");
    sp->session().debugTeleport({57, 37});
    frames(40);
    shot(folder + "/17_storm_base.png");
    sp->debugStorm(false);
    sp->session().debugTeleport({92, 61});
    sp->session().dialogue().clear();
    frames(60);
    shot(folder + "/18_reactor.png");
    SaveData late = lateSave(sp->session(), 66, "111111111111111111111101");
    late.set("lit", "111111");
    late.set("pad", "111");
    late.set("sys_beacon_on", "1");
    late.set("sys_greenhouse_on", "0");
    auto lateOwned = std::make_unique<PlayScreen>(*this, &late);
    PlayScreen* lp = lateOwned.get();
    switchTo(std::move(lateOwned));
    frames(2);
    lp->session().dialogue().clear();
    lp->session().debugTeleport({84, 22});
    frames(80);
    shot(folder + "/19_landing_lit.png");

    EndSummary sum;
    sum.code = "E1";
    sum.hours = 72;
    sum.crewAlive = 4;
    sum.systemsBuilt = 7;
    sum.trustPercent = 81;
    sum.contactHour = 8;
    sum.reportsMet = 2;
    sum.truth = true;
    switchTo(std::make_unique<FinaleScreen>(*this, sum));
    frames(300);
    shot(folder + "/20_finale_descent.png");
    frames(300);
    shot(folder + "/21_finale_walk.png");
    switchTo(std::make_unique<EndScreen>(*this, sum));
    frames(20);
    shot(folder + "/22_end_E1.png");
    EndSummary lost;
    lost.code = "F4";
    lost.hours = 72;
    lost.crewAlive = 3;
    lost.systemsBuilt = 6;
    lost.contactHour = 16;
    lost.reportsMet = 1;
    switchTo(std::make_unique<EndScreen>(*this, lost));
    frames(20);
    shot(folder + "/23_end_F4.png");
    switchTo(std::make_unique<TitleScreen>(*this));
    frames(20);
    shot(folder + "/24_title_endings.png");
    return 0;
}

void Game::drawDataError() {
    BeginDrawing();
    ClearBackground(pal::Bg);
    DrawText("STRANDED: 72 Hours could not start.", 40, 40, 30, RAYWHITE);
    DrawText(dataError_.c_str(), 40, 100, 20, pal::Red);
    DrawText("Check that the data folder sits next to the game. Close the window to exit.", 40, 140, 20, LIGHTGRAY);
    EndDrawing();
}
