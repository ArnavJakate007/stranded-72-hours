#pragma once
#include "core/Screen.h"
#include "render/Weather.h"
#include <string>
#include <vector>

// New run, Continue (only if an act save loads), How to play, Quit, and the ending collection (FR-2, PRD P-5).
class TitleScreen : public Screen {
public:
    explicit TitleScreen(Game& game);
    void update(float dt) override;
    void draw() override;

private:
    void activate(int index);
    std::vector<std::string> items_;
    int selected_ = 0;
    bool hasSave_ = false;
    Weather weather_;
};
