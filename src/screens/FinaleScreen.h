#pragma once
#include "core/Screen.h"
#include "logic/Session.h"
#include "render/Particles.h"
#include <string>
#include <vector>

// The rescue (E1-E3): the shuttle drops through the night onto the landing pad, the survivors walk out
// to it, and the ending text plays. Then the summary screen. A rough landing bounces and throws debris.
class FinaleScreen : public Screen {
public:
    FinaleScreen(Game& game, EndSummary summary);
    void update(float dt) override;
    void draw() override;

private:
    float shuttleY() const;
    void advance();
    EndSummary summary_;
    Particles fx_{500};
    std::vector<std::string> lines_;
    std::vector<std::string> walkers_;
    int line_ = -1;
    float time_ = 0, shown_ = 0, shake_ = 0, flash_ = 0, emit_ = 0;
    bool landed_ = false;
};
