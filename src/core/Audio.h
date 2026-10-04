#pragma once
#include "raylib.h"
#include <map>
#include <string>
#include <vector>

// All sound effects and music are synthesized in code at start (no audio files to ship).
// If the machine has no audio device the game runs silently (SRS HW-3).
class AudioBank {
public:
    enum class Wave { Square, Saw, Sine, Triangle, Noise };
    struct Note { float startSec, f1, f2, duration; Wave wave; float volume; float attack = 0.002f; bool sustain = false; };

    AudioBank() = default;
    ~AudioBank();
    AudioBank(const AudioBank&) = delete;
    AudioBank& operator=(const AudioBank&) = delete;

    void init();
    void play(const std::string& name);
    void playMusic(const std::string& name, float volume = 0.6f);   // loops until stopped or replaced
    void stopMusic();
    void update();                                                    // restarts the music loop when it ends
    void setEnabled(bool on);
    bool enabled() const { return enabled_; }

private:
    void synth(const std::string& name, const std::vector<Note>& notes);
    void synthMusic(const std::string& name, const std::vector<std::vector<float>>& chords, float chordSeconds, float arpVolume, bool bright);
    std::map<std::string, Sound> sounds_;
    std::string music_;
    bool ready_ = false;
    bool enabled_ = true;
};
