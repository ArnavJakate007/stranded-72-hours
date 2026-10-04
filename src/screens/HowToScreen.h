#pragma once
#include "core/Screen.h"

// Controls, goals, survival and crew rules on one page.
class HowToScreen : public Screen {
public:
    explicit HowToScreen(Game& game) : Screen(game) {}
    void update(float dt) override;
    void draw() override;
};
