#pragma once
#include "logic/Types.h"
#include "raylib.h"
#include <optional>
#include <vector>

// Maps the keyboard to the four game inputs (arrows/WASD, Z, X, Enter) and tracks the mouse
// in 640x360 canvas coordinates (SRS HW-1, NFR-8).
class Input {
public:
    enum class Act { Up, Down, Left, Right, Accept, Back, Pause };

    void poll();
    void setViewport(float scale, float offsetX, float offsetY);

    bool pressed(Act a) const;
    const std::vector<Act>& presses() const { return presses_; }
    std::optional<Dir> heldDirection() const;     // the most recently pressed direction still held
    bool acceptHeld() const { return IsKeyDown(KEY_Z); }
    bool leftHeld() const { return IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A); }
    bool rightHeld() const { return IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D); }

    Vector2 mouse() const { return mouse_; }
    bool mouseInside() const { return mouseInside_; }
    bool clicked() const { return clicked_ && !clickConsumed_; }
    void consumeClick() { clickConsumed_ = true; }
    bool mouseMoved() const { return mouseMoved_; }
    bool mouseHeld() const { return mouseInside_ && IsMouseButtonDown(MOUSE_BUTTON_LEFT); }

private:
    std::vector<Act> presses_;
    std::vector<Dir> heldOrder_;
    Vector2 mouse_{-1, -1};
    bool mouseInside_ = false, clicked_ = false, clickConsumed_ = false, mouseMoved_ = false;
    float scale_ = 1, ox_ = 0, oy_ = 0;
};

static inline bool isDirection(Input::Act a) {
    return a == Input::Act::Up || a == Input::Act::Down || a == Input::Act::Left || a == Input::Act::Right;
}
