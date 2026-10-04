#pragma once
#include "core/Input.h"
#include "gfx/Ui.h"
#include "logic/Dialogue.h"
#include "logic/Session.h"
#include <functional>
#include <string>
#include <vector>

class AudioBank;

// Bottom-third dialogue box: types the current line letter by letter (0.03 s), shows the blinking
// down-triangle when done, first accept shows the full line, second goes on (FR-56, FR-57).
class DialogueBox {
public:
    void update(float dt, DialogueQueue& q, float letterSeconds, AudioBank& audio);
    // Returns true if the press was used by the box.
    bool accept(DialogueQueue& q);
    bool back(DialogueQueue& q);
    bool typing(const DialogueQueue& q) const { return !q.empty() && shown_ < q.current().text.size(); }
    void draw(const DialogueQueue& q, float t, HitList& hits, const std::function<void()>& onClick) const;

private:
    const DialogueLine* shownLine_ = nullptr;
    std::string shownText_;
    float shown_ = 0, life_ = 0;
};

// Small menu that opens when Kiran talks to a crew member: assign a task, talk, or leave.
class ChoiceMenu {
public:
    void open(const std::string& who, const std::string& title, std::vector<TalkOption> options);
    bool isOpen() const { return open_; }
    void close() { open_ = false; }
    void handle(Input::Act a);
    void draw(float t, HitList& hits);

private:
    void choose(int i);
    bool open_ = false;
    std::string who_, title_;
    std::vector<TalkOption> options_;
    int selected_ = 0;
};

// The Enter menu: stops the clock and gives keyboard access to every panel, settings and the act tracker.
class PauseMenu {
public:
    enum class Choice { None, Resume, Journal, Power, Crew, Map, Cargo, TextSpeed, Sound, ClockSpeed, Quit };
    void open() { open_ = true; selected_ = 0; }
    bool isOpen() const { return open_; }
    void close() { open_ = false; }
    Choice handle(Input::Act a);
    Choice takeClicked() { Choice c = clicked_; clicked_ = Choice::None; return c; }
    void draw(const Session& s, float t, HitList& hits, int textSpeed, bool sound, float clockMul);

private:
    static const std::vector<Choice>& items();
    bool open_ = false;
    int selected_ = 0;
    Choice clicked_ = Choice::None;
};
