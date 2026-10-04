#pragma once
#include "logic/Inventory.h"
#include "logic/Types.h"
#include <deque>
#include <optional>
#include <string>
#include <vector>

// Commander Kiran Das: tile position, smooth step movement, carried items and suit oxygen.
class Player {
public:
    explicit Player(int carrySlots, std::function<int(const std::string&)> sizeOf);
    void reset(IVec tile);

    IVec tile() const { return tile_; }
    IVec from() const { return from_; }
    float progress() const { return progress_; }       // 0 when standing, 0..1 while stepping
    bool moving() const { return progress_ > 0.0f; }
    Dir facing() const { return facing_; }
    int stepParity() const { return parity_; }
    IVec front() const { IVec d = dirVec(facing_); return {tile_.x + d.x, tile_.y + d.y}; }

    // Pixel position of the feet (centre bottom) at the current point of the step.
    float pixelX() const;
    float pixelY() const;

    void face(Dir d) { facing_ = d; }
    void beginStep(IVec to, float duration);
    // Returns true on the frame the step finishes.
    bool advance(float dt);
    void teleport(IVec t);

    Inventory<std::string>& carry() { return carry_; }
    const Inventory<std::string>& carry() const { return carry_; }

    float suit = 100.0f;
    float suitMax = 100.0f;                 // +25 with the supply pod's extra tank
    float health = 100.0f;                  // radiation, deep cold and storm debris hurt; Medical heals at the base
    bool wasInside = true;
    float turnDelay = 0.0f;
    float gustTimer = 2.5f;
    bool gustWarning = false;
    float bumpCooldown = 0.0f;
    std::deque<IVec> path;                 // click-to-walk route
    std::optional<IVec> pendingTarget;      // object to use on arrival
    std::deque<IVec> trail;                 // tiles left behind, for Hana to follow

private:
    IVec tile_{}, from_{};
    Dir facing_ = Dir::Down;
    float progress_ = 0.0f, duration_ = 0.17f;
    int parity_ = 0;
    Inventory<std::string> carry_;
};
