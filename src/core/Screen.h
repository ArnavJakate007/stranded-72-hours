#pragma once

class Game;

// Abstract base for every screen (Title, Intro, Play, End...). The Game owns exactly one at a time
// and calls update() then draw() each frame (SRS T-5). Pure virtual functions = abstraction.
class Screen {
public:
    explicit Screen(Game& game) : game_(game) {}
    virtual ~Screen() = default;
    Screen(const Screen&) = delete;
    Screen& operator=(const Screen&) = delete;

    virtual void update(float dt) = 0;
    virtual void draw() = 0;

protected:
    Game& game_;
};
