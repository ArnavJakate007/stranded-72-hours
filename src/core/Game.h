#pragma once
#include "core/Audio.h"
#include "core/Input.h"
#include "core/Screen.h"
#include "gfx/PixelFont.h"
#include "gfx/Sprites.h"
#include "gfx/Ui.h"
#include "logic/DataStore.h"
#include "logic/SaveManager.h"
#include <memory>
#include <string>

struct Settings {
    int textSpeed = 0;            // 0 normal (0.03 s per letter), 1 fast, 2 instant
    float clockMultiplier = 4;    // 1 = the documented 60 s per game hour
    bool sound = true;
    float letterSeconds() const { return textSpeed == 0 ? 0.03f : textSpeed == 1 ? 0.012f : 0.0f; }
};

// Owns the window, the 640x360 canvas and every shared service, and holds the current screen
// (composition). Screens are swapped through switchTo() between frames.
class Game {
public:
    static constexpr int VW = 640, VH = 360;

    Game();
    ~Game();
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void run();
    int smokeTest(const std::string& shotFolder);   // scripted walk through every screen, saves screenshots
    void switchTo(std::unique_ptr<Screen> next) { pending_ = std::move(next); }
    void quit() { quit_ = true; }

    Input& input() { return input_; }
    AudioBank& audio() { return audio_; }
    HitList& hits() { return hits_; }
    DataStore& data() { return data_; }
    SaveManager& saves() { return saves_; }
    Settings& settings() { return settings_; }
    SpriteBank& sprites() { return sprites_; }
    float time() const { return time_; }

private:
    void frame();
    void drawDataError();
    void shot(const std::string& path);

    RenderTexture2D canvas_{};
    Input input_;
    AudioBank audio_;
    PixelFont font_;
    HitList hits_;
    DataStore data_;
    SaveManager saves_;
    Settings settings_;
    SpriteBank sprites_;
    std::unique_ptr<Screen> screen_, pending_;
    std::string dataError_;
    float time_ = 0;
    bool quit_ = false;
};
