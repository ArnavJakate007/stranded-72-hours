#include "logic/Player.h"

Player::Player(int carrySlots, std::function<int(const std::string&)> sizeOf) : carry_(carrySlots, std::move(sizeOf)) {}

void Player::reset(IVec tile) {
    teleport(tile);
    facing_ = Dir::Down;
    suit = suitMax = 100.0f;
    health = 100.0f;
    wasInside = true;
    turnDelay = 0.0f;
    gustTimer = 2.5f;
    gustWarning = false;
    carry_.clear();
    trail.clear();
}

void Player::teleport(IVec t) {
    tile_ = from_ = t;
    progress_ = 0.0f;
    path.clear();
    pendingTarget.reset();
}

float Player::pixelX() const { return (from_.x + (tile_.x - from_.x) * progress_) * 16.0f + 8.0f; }
float Player::pixelY() const { return (from_.y + (tile_.y - from_.y) * progress_) * 16.0f + 13.0f; }

void Player::beginStep(IVec to, float duration) {
    trail.push_front(tile_);
    if (trail.size() > 4) trail.pop_back();
    from_ = tile_;
    tile_ = to;
    duration_ = duration;
    progress_ = 0.0001f;
}

bool Player::advance(float dt) {
    if (progress_ <= 0.0f) return false;
    progress_ += dt / duration_;
    if (progress_ < 1.0f) return false;
    progress_ = 0.0f;
    from_ = tile_;
    parity_ ^= 1;
    return true;
}
