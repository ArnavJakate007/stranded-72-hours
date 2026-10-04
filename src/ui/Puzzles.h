#pragma once
#include "core/Input.h"
#include "gfx/Ui.h"
#include "logic/Rng.h"
#include "logic/Types.h"
#include <memory>
#include <string>
#include <vector>

class AudioBank;

// Abstract repair mini-game. The play screen only talks to this interface, so it can run any puzzle
// without knowing which one it is (polymorphism; GDD "Puzzle and children").
class Puzzle {
public:
    enum class State { Playing, Solved, Failed };
    Puzzle(std::string title, std::string hint, float timeLimit, AudioBank& audio);
    virtual ~Puzzle() = default;

    void update(float dt, const Input& in);   // counts down the timer, then lets the child react
    void draw(float t, HitList& hits, const Input& in);
    virtual void handle(Input::Act a) = 0;
    State state() const { return state_; }
    const std::string& failReason() const { return failReason_; }

    static std::unique_ptr<Puzzle> create(PuzzleKind kind, int difficulty, float timeLimit, AudioBank& audio);

protected:
    virtual void step(float dt, const Input& in) { (void)dt; (void)in; }
    virtual void drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) = 0;
    void solve();
    void mistake(float penaltySeconds);
    void fail(const std::string& why);

    AudioBank& audio_;
    Rng rng_;
    float timeLeft_, timeLimit_;
    float shake_ = 0;

private:
    std::string title_, hint_, failReason_;
    State state_ = State::Playing;
};

// Connect each coloured wire end on the left to the same colour on the right.
class WirePuzzle : public Puzzle {
public:
    WirePuzzle(int pairs, float timeLimit, AudioBank& audio);
    void handle(Input::Act a) override;
protected:
    void drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) override;
private:
    void pick(int side, int index);
    std::vector<int> left_, right_;      // colour index at each slot
    std::vector<int> link_;              // left slot -> right slot, -1 if not connected
    int side_ = 0, cursor_ = 0, held_ = -1;
};

// Rotate pipe tiles until the flow runs from the inlet on the left to the outlet on the right.
class PipePuzzle : public Puzzle {
public:
    PipePuzzle(int cols, int rows, float timeLimit, AudioBank& audio);
    void handle(Input::Act a) override;
protected:
    void drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) override;
private:
    void rotate(int index);
    void flow();
    int cols_, rows_, inRow_, outRow_, cx_ = 0, cy_ = 0;
    std::vector<int> mask_;     // N=1 E=2 S=4 W=8
    std::vector<char> wet_;
};

// Move the needle onto the drifting signal and hold it there until the lock fills.
class TunePuzzle : public Puzzle {
public:
    TunePuzzle(float timeLimit, AudioBank& audio);
    void handle(Input::Act a) override { (void)a; }
protected:
    void step(float dt, const Input& in) override;
    void drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) override;
private:
    float pos_ = 0.2f, target_ = 0.5f, lock_ = 0, clock_ = 0;
    float mouseX_ = -1;
};

// Watch the lights flash, then press them in the same order.
class SequencePuzzle : public Puzzle {
public:
    SequencePuzzle(int length, float timeLimit, AudioBank& audio);
    void handle(Input::Act a) override;
protected:
    void step(float dt, const Input& in) override;
    void drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) override;
private:
    void press(int light);
    void replay();
    std::vector<int> seq_;
    int entered_ = 0, cursor_ = 0, flashLight_ = -1;
    float showTime_ = 0, pressFlash_ = 0;
    bool showing_ = true;
};

// Burn through each weak point with the plasma cutter. Holding Z (or the mouse) cuts fast but heats the
// torch; letting it overheat is a mistake. Short taps cut slower but stay cool.
class CutPuzzle : public Puzzle {
public:
    CutPuzzle(int points, float timeLimit, AudioBank& audio);
    void handle(Input::Act a) override;
protected:
    void step(float dt, const Input& in) override;
    void drawBoard(float x, float y, float w, float h, float t, HitList& hits, const Input& in) override;
private:
    void cutBy(float amount, float heat);
    std::vector<float> cut_;      // 0..1 progress per weak point
    std::vector<float> spot_;     // horizontal position 0..1
    int cursor_ = 0;
    float heat_ = 0, sparkTimer_ = 0, overheatLock_ = 0;
    bool mouseDown_ = false;
};
