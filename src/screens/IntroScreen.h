#pragma once
#include "core/Screen.h"
#include "logic/DataStore.h"
#include "render/Particles.h"
#include <string>
#include <vector>

// The backstory before play (PRD P-1), told as a short cinematic: HALCYON in orbit, the descent into the
// storm, Hana's refused abort, the crash, waking up in the dark, then the title card slams in.
// Lines come from dialogue.txt; Z advances, X skips.
class IntroScreen : public Screen {
public:
    IntroScreen(Game& game, int startSlide);
    void update(float dt) override;
    void draw() override;

private:
    enum class Scene { Station, Descent, Radio, Crash, Dark, Title };
    struct Slide { Scene scene; std::vector<std::string> lines; };
    void next();
    void startGame();
    void enterSlide();
    void updateScene(float dt);
    void drawStation(float t, float k);
    void drawDescent(float t, float k);
    void drawRadio(float t, float k, bool speaking);
    void drawCrash(float t, float k);
    void drawDark(float t, float k);
    void drawTitle(float t, float k);
    void drawTextBox(float t);

    std::vector<Slide> slides_;
    Particles fx_{600};
    int slide_ = 0, line_ = 0;
    float shown_ = 0, sceneTime_ = 0, shake_ = 0, flash_ = 0, beat_ = 0, emit_ = 0;
    int lettersLanded_ = 0;
    bool warned_ = false, impacted_ = false, struck1_ = false, struck2_ = false, slammed_ = false;
};
