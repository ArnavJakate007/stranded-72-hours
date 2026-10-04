#pragma once
#include "core/Screen.h"
#include "logic/SaveManager.h"
#include "logic/Session.h"
#include "render/Hud.h"
#include "render/LightMap.h"
#include "render/Weather.h"
#include "render/WorldRenderer.h"
#include "ui/Overlays.h"
#include "ui/Panels.h"
#include "ui/Puzzles.h"
#include <memory>

// The game itself. Owns one Session (the rules) and everything that shows it: world renderer, light map,
// HUD, panels, puzzles and overlays. It routes input to whichever layer is on top and turns session events
// into sounds, banners and screen changes.
class PlayScreen : public Screen {
public:
    PlayScreen(Game& game, const SaveData* save);
    void update(float dt) override;
    void draw() override;

    // Test hooks for the automated smoke test.
    Session& session() { return session_; }
    void debugOpenPanel(const std::string& id) { openPanel(id); }
    void debugClosePanel() { panel_.reset(); }
    void debugPuzzle(PuzzleKind k);
    void debugClosePuzzle() { puzzle_.reset(); }
    void debugPause(bool open) { if (open) pause_.open(); else pause_.close(); }
    void debugStorm(bool on) { session_.colony().storm = on; weather_.setStorm(on); }

private:
    void routeInput();
    void handleEvents();
    void openPanel(const std::string& id);
    void applyPause(PauseMenu::Choice c);
    bool overlayOpen() const { return panel_ || puzzle_ || choice_.isOpen() || pause_.isOpen(); }
    void drawVignette() const;

    Session session_;
    WorldRenderer world_;
    LightMap lights_;
    Hud hud_;
    Weather weather_;
    DialogueBox dialogueBox_;
    ChoiceMenu choice_;
    PauseMenu pause_;
    std::unique_ptr<Panel> panel_;
    std::unique_ptr<Puzzle> puzzle_;
    PuzzleKind puzzleKind_ = PuzzleKind::None;
    Cam cam_;
    float shake_ = 0, flash_ = 0, endDelay_ = -1;
};
