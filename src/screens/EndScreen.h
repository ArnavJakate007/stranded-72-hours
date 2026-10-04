#pragma once
#include "core/Screen.h"
#include "logic/Session.h"
#include <string>
#include <vector>

// End-of-run summary with "???" for anything missed (PRD P-4), then Restart act / New run / Title (FR-54).
class EndScreen : public Screen {
public:
    EndScreen(Game& game, EndSummary summary);
    void update(float dt) override;
    void draw() override;

private:
    void activate(int index);
    EndSummary summary_;
    std::vector<std::string> buttons_;
    int selected_ = 0;
};
