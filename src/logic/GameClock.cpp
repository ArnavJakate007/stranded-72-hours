#include "logic/GameClock.h"
#include <cmath>

void GameClock::reset(float hour, bool running) {
    hour_ = hour;
    running_ = running;
}

std::vector<int> GameClock::skipTo(float hour) {
    std::vector<int> crossed;
    for (int h = static_cast<int>(std::floor(hour_)) + 1; h <= static_cast<int>(std::floor(hour)); ++h) crossed.push_back(h);
    hour_ = hour;
    return crossed;
}

std::vector<int> GameClock::advance(float realSeconds, float secondsPerHour, float multiplier) {
    if (!running_) return {};
    return skipTo(hour_ + realSeconds * multiplier / secondsPerHour);
}

int GameClock::openWindow() const {
    for (int w : WINDOWS)
        if (hour_ >= w && hour_ < w + WINDOW_LENGTH) return w;
    return 0;
}

int GameClock::nextWindow() const {
    for (int w : WINDOWS)
        if (hour_ < w + WINDOW_LENGTH) return w;
    return 0;
}
