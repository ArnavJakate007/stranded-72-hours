#include "core/Input.h"
#include <algorithm>

namespace {
struct Binding { int key; Input::Act act; };
const Binding kBindings[] = {
    {KEY_UP, Input::Act::Up}, {KEY_W, Input::Act::Up}, {KEY_DOWN, Input::Act::Down}, {KEY_S, Input::Act::Down},
    {KEY_LEFT, Input::Act::Left}, {KEY_A, Input::Act::Left}, {KEY_RIGHT, Input::Act::Right}, {KEY_D, Input::Act::Right},
    {KEY_Z, Input::Act::Accept}, {KEY_X, Input::Act::Back}, {KEY_ENTER, Input::Act::Pause}, {KEY_KP_ENTER, Input::Act::Pause}};

std::optional<Dir> toDir(Input::Act a) {
    switch (a) {
        case Input::Act::Up: return Dir::Up;
        case Input::Act::Down: return Dir::Down;
        case Input::Act::Left: return Dir::Left;
        case Input::Act::Right: return Dir::Right;
        default: return std::nullopt;
    }
}
bool dirDown(Dir d) {
    switch (d) {
        case Dir::Up: return IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);
        case Dir::Down: return IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
        case Dir::Left: return IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
        default: return IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
    }
}
}  // namespace

void Input::setViewport(float scale, float offsetX, float offsetY) {
    scale_ = scale;
    ox_ = offsetX;
    oy_ = offsetY;
}

void Input::poll() {
    presses_.clear();
    for (const Binding& b : kBindings)
        if (IsKeyPressed(b.key)) {
            presses_.push_back(b.act);
            if (auto d = toDir(b.act)) {
                heldOrder_.erase(std::remove(heldOrder_.begin(), heldOrder_.end(), *d), heldOrder_.end());
                heldOrder_.push_back(*d);
            }
        }
    heldOrder_.erase(std::remove_if(heldOrder_.begin(), heldOrder_.end(), [](Dir d) { return !dirDown(d); }), heldOrder_.end());

    Vector2 raw = GetMousePosition();
    Vector2 m{(raw.x - ox_) / scale_, (raw.y - oy_) / scale_};
    mouseMoved_ = m.x != mouse_.x || m.y != mouse_.y;
    mouse_ = m;
    mouseInside_ = m.x >= 0 && m.y >= 0 && m.x < 640 && m.y < 360 && IsCursorOnScreen();
    clicked_ = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && mouseInside_;
    clickConsumed_ = false;
}

bool Input::pressed(Act a) const { return std::find(presses_.begin(), presses_.end(), a) != presses_.end(); }

std::optional<Dir> Input::heldDirection() const {
    if (heldOrder_.empty()) return std::nullopt;
    return heldOrder_.back();
}
