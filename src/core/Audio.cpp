#include "core/Audio.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace {
float midi(int note) { return 440.0f * std::pow(2.0f, (note - 69) / 12.0f); }
}  // namespace

AudioBank::~AudioBank() {
    for (auto& kv : sounds_) UnloadSound(kv.second);
    if (ready_) CloseAudioDevice();
}

void AudioBank::init() {
    InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    if (!ready_) return;
    using W = Wave;
    synth("hum", {{0, 90, 300, .5f, W::Saw, .35f}, {0, 180, 600, .5f, W::Sine, .2f}});
    synth("buzz", {{0, 70, 52, .24f, W::Square, .45f}});
    synth("click", {{0, 1000, 760, .04f, W::Square, .25f}});
    synth("alarm", {{0, 880, 880, .11f, W::Square, .3f}, {.17f, 660, 660, .11f, W::Square, .3f}});
    synth("blip", {{0, 1400, 1400, .015f, W::Square, .08f}});
    synth("step_snow", {{0, 180, 120, .04f, W::Noise, .08f}});
    synth("step_metal", {{0, 420, 300, .03f, W::Square, .1f}});
    synth("bump", {{0, 110, 80, .07f, W::Square, .25f}});
    synth("pick", {{0, 660, 990, .08f, W::Square, .25f}, {.08f, 990, 1320, .08f, W::Square, .25f}});
    synth("wind", {{0, 300, 120, .6f, W::Noise, .12f}});
    synth("crash", {{0, 120, 30, .9f, W::Saw, .6f}, {0, 60, 20, 1.2f, W::Noise, .5f}});
    synth("ok", {{0, 523, 523, .1f, W::Square, .25f}, {.11f, 784, 784, .18f, W::Square, .25f}});
    synth("spark", {{0, 900, 200, .15f, W::Noise, .3f}});
    synth("static", {{0, 200, 200, .3f, W::Noise, .1f}});
    synth("heartbeat", {{0, 70, 50, .12f, W::Sine, .8f}, {.22f, 65, 45, .14f, W::Sine, .6f}});
    synth("impact", {{0, 160, 40, .5f, W::Square, .5f}, {0, 80, 30, .7f, W::Noise, .45f}});
    synth("whoosh", {{0, 600, 200, .5f, W::Noise, .18f, .2f}});
    synth("thunder", {{0, 80, 30, 1.6f, W::Noise, .55f, .02f}});
    synth("chime", {{0, midi(76), midi(76), .5f, W::Sine, .3f}, {.12f, midi(81), midi(81), .6f, W::Sine, .3f}, {.24f, midi(88), midi(88), .9f, W::Sine, .25f}});
    // music: slow minor pad for the intro and the cold nights, warm major pad for the rescue
    synthMusic("music_intro", {{57, 60, 64}, {53, 57, 60}, {48, 55, 60}, {55, 59, 62}}, 4.0f, 0.09f, false);
    synthMusic("music_game", {{45, 52, 57}, {41, 48, 53}, {43, 50, 55}, {40, 47, 52}}, 6.0f, 0.0f, false);
    synthMusic("music_finale", {{48, 55, 60, 64}, {53, 57, 60, 65}, {45, 52, 57, 60}, {55, 59, 62, 67}}, 3.0f, 0.1f, true);
}

void AudioBank::synth(const std::string& name, const std::vector<Note>& notes) {
    const int rate = 22050;
    float total = 0;
    for (const Note& n : notes) total = std::max(total, n.startSec + n.duration);
    int frames = static_cast<int>(total * rate) + 1;
    std::vector<float> mix(frames, 0.0f);
    uint32_t noise = 12345;
    for (const Note& n : notes) {
        int start = static_cast<int>(n.startSec * rate), len = static_cast<int>(n.duration * rate);
        float phase = 0;
        for (int i = 0; i < len && start + i < frames; ++i) {
            float k = static_cast<float>(i) / len;
            float f = n.f1 + (n.f2 - n.f1) * k;
            phase += f / rate;
            phase -= std::floor(phase);
            float s;
            switch (n.wave) {
                case Wave::Square: s = phase < 0.5f ? 1.0f : -1.0f; break;
                case Wave::Saw: s = 2.0f * phase - 1.0f; break;
                case Wave::Sine: s = std::sin(phase * 6.2831853f); break;
                case Wave::Triangle: s = 4.0f * std::fabs(phase - 0.5f) - 1.0f; break;
                default: noise = noise * 1664525u + 1013904223u; s = ((noise >> 9) & 0xFFFF) / 32768.0f - 1.0f; break;
            }
            float attack = std::min(1.0f, (i / static_cast<float>(rate)) / std::max(0.002f, n.attack));
            float env = n.sustain ? std::min(1.0f, (1.0f - k) * 6.0f) : std::exp(-4.0f * k);
            mix[start + i] += s * n.volume * env * attack;
        }
    }
    std::vector<int16_t> pcm(frames);
    for (int i = 0; i < frames; ++i) pcm[i] = static_cast<int16_t>(std::max(-1.0f, std::min(1.0f, mix[i])) * 32000);
    ::Wave w{};
    w.frameCount = static_cast<unsigned>(frames);
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = pcm.data();
    sounds_[name] = LoadSoundFromWave(w);   // copies the samples
}

void AudioBank::synthMusic(const std::string& name, const std::vector<std::vector<float>>& chords, float chordSeconds, float arpVolume, bool bright) {
    std::vector<Note> notes;
    float t = 0;
    for (const auto& chord : chords) {
        for (float m : chord) {
            notes.push_back({t, midi(static_cast<int>(m)), midi(static_cast<int>(m)), chordSeconds + 0.4f, Wave::Triangle, 0.07f, 0.8f, true});
            notes.push_back({t, midi(static_cast<int>(m)) * 1.003f, midi(static_cast<int>(m)) * 1.003f, chordSeconds + 0.4f, Wave::Sine, 0.06f, 0.8f, true});
        }
        notes.push_back({t, midi(static_cast<int>(chord[0]) - 12), midi(static_cast<int>(chord[0]) - 12), chordSeconds, Wave::Sine, 0.12f, 0.3f, true});
        if (arpVolume > 0) {
            int steps = static_cast<int>(chordSeconds * 4);
            for (int i = 0; i < steps; ++i) {
                float m = chord[i % chord.size()] + (bright || i % 8 >= 4 ? 12 : 0);
                notes.push_back({t + i * 0.25f, midi(static_cast<int>(m)), midi(static_cast<int>(m)), 0.35f, Wave::Sine, arpVolume});
            }
        }
        t += chordSeconds;
    }
    synth(name, notes);
}

void AudioBank::play(const std::string& name) {
    if (!ready_ || !enabled_) return;
    auto it = sounds_.find(name);
    if (it != sounds_.end()) PlaySound(it->second);
}

void AudioBank::playMusic(const std::string& name, float volume) {
    if (!ready_) return;
    if (music_ == name) return;
    stopMusic();
    music_ = name;
    auto it = sounds_.find(name);
    if (it == sounds_.end()) return;
    SetSoundVolume(it->second, volume);
    if (enabled_) PlaySound(it->second);
}

void AudioBank::stopMusic() {
    if (!ready_ || music_.empty()) return;
    auto it = sounds_.find(music_);
    if (it != sounds_.end()) StopSound(it->second);
    music_.clear();
}

void AudioBank::update() {
    if (!ready_ || music_.empty() || !enabled_) return;
    auto it = sounds_.find(music_);
    if (it != sounds_.end() && !IsSoundPlaying(it->second)) PlaySound(it->second);
}

void AudioBank::setEnabled(bool on) {
    if (enabled_ == on) return;
    enabled_ = on;
    if (!ready_ || music_.empty()) return;
    auto it = sounds_.find(music_);
    if (it == sounds_.end()) return;
    if (on) PlaySound(it->second);
    else StopSound(it->second);
}
