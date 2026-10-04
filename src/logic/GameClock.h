#pragma once
#include <vector>

// Game hours, signal windows and the two countdowns. Shared constants are static members.
class GameClock {
public:
    static constexpr int WINDOWS[3] = {8, 16, 24};   // the Magnetic Tide calms: radio gets through
    static constexpr int WINDOW_LENGTH = 2;           // each window lasts two game hours (one failed align is not game over)
    static constexpr int CONTACT_DEADLINE = 24;
    static constexpr int SHUTTLE_HOUR = 72;
    static constexpr int ACT2_HOUR = 6;
    static constexpr int ACT3_HOUR = 24;
    static constexpr int ACT4_HOUR = 60;
    static constexpr int REPORT_HOURS[2] = {32, 56};
    static constexpr int STORM_HOUR = 48;
    static constexpr int STORM_END = 52;

    void reset(float hour = 0.0f, bool running = false);
    // Advances the clock and returns every whole hour that was crossed.
    std::vector<int> advance(float realSeconds, float secondsPerHour, float multiplier);
    std::vector<int> skipTo(float hour);

    float hour() const { return hour_; }
    bool running() const { return running_; }
    void setRunning(bool r) { running_ = r; }
    int act() const { return hour_ < ACT2_HOUR ? 1 : hour_ < ACT3_HOUR ? 2 : hour_ < ACT4_HOUR ? 3 : 4; }
    static const char* actName(int act) { return act == 1 ? "Dark" : act == 2 ? "The Signal" : act == 3 ? "Holding On" : "The Light"; }
    int openWindow() const;      // 0 if no window is open now
    int nextWindow() const;      // next window that has not closed yet, 0 if none
    float hoursToContactDeadline() const { return CONTACT_DEADLINE - hour_; }
    float hoursToShuttle() const { return SHUTTLE_HOUR - hour_; }

private:
    float hour_ = 0.0f;
    bool running_ = false;
};
