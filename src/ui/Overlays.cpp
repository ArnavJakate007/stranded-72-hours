#include "ui/Overlays.h"
#include "core/Audio.h"
#include "gfx/Sprites.h"
#include "logic/Rng.h"
#include <cmath>

// ---------------------------------------------------------------- DialogueBox

void DialogueBox::update(float dt, DialogueQueue& q, float letterSeconds, AudioBank& audio) {
    if (q.empty()) {
        shownLine_ = nullptr;
        return;
    }
    const DialogueLine& line = q.current();
    if (shownLine_ != &line || shownText_ != line.text) {   // a new line started
        shownLine_ = &line;
        shownText_ = line.text;
        shown_ = 0;
        life_ = 0;
        if (!line.speaker.empty() && !line.block) audio.play("static");
    }
    life_ += dt;
    int before = static_cast<int>(shown_);
    shown_ = letterSeconds > 0 ? std::min<float>(static_cast<float>(line.text.size()), shown_ + dt / letterSeconds) : static_cast<float>(line.text.size());
    if (static_cast<int>(shown_) > before && static_cast<int>(shown_) % 3 == 0) audio.play("blip");
    // routine radio lines close by themselves so they never block play
    if (!line.block && shown_ >= line.text.size() && life_ > line.text.size() * 0.03f + 4.5f) {
        q.pop();
        shownLine_ = nullptr;
    }
}

bool DialogueBox::accept(DialogueQueue& q) {
    if (q.empty()) return false;
    const DialogueLine& line = q.current();
    if (shown_ < line.text.size()) {
        shown_ = static_cast<float>(line.text.size());
        return true;
    }
    q.pop();
    shownLine_ = nullptr;
    return true;
}

bool DialogueBox::back(DialogueQueue& q) {
    if (q.empty() || q.current().block) return false;
    q.pop();
    shownLine_ = nullptr;
    return true;
}

void DialogueBox::draw(const DialogueQueue& q, float t, HitList& hits, const std::function<void()>& onClick) const {
    if (q.empty()) return;
    const DialogueLine& line = q.current();
    const float x = 132, y = 262, w = 376, h = 66;
    bool narr = line.speaker.empty();
    Ui::panel(x, y, w, h, narr ? pal::rgb(0x6B5A2A) : pal::PanelLineHi, pal::PanelFillDark);
    hits.add(Rectangle{x, y, w, h}, onClick);
    size_t count = std::min(line.text.size(), static_cast<size_t>(shown_));
    std::string visible = line.text.substr(0, count);
    auto drawWrapped = [&](float tx, float ty, int maxW) {
        size_t used = 0;
        auto lines = Ui::wrap(line.text, maxW);
        for (size_t i = 0; i < lines.size(); ++i) {
            std::string part = used < visible.size() ? visible.substr(used, std::min(lines[i].size(), visible.size() - used)) : "";
            Ui::text(part, tx, ty + i * 11.0f, pal::Ink);
            used += lines[i].size() + 1;
        }
    };
    if (narr) {
        drawWrapped(x + 12, y + 10, static_cast<int>(w - 28));
    } else {
        Icons::portrait(line.speaker, x + 8, y + 8);
        if (life_ < 0.35f && !line.block) {
            Rng r(static_cast<uint32_t>(t * 30));
            for (int i = 0; i < 6; ++i) Ui::rect(x + 4 + std::floor(r.next() * (w - 60)), y + 4 + std::floor(r.next() * (h - 8)), 20 + std::floor(r.next() * 40), 1, Color{200, 210, 230, 64});
        }
        std::string name = line.speaker == "voss" ? "DIRECTOR VOSS" : line.speaker;
        for (char& c : name) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        Ui::text(name, x + 44, y + 7, speakerColor(line.speaker));
        if (!line.block) Ui::textRight("RADIO", x + w - 10, y + 7, pal::Dim2);
        drawWrapped(x + 44, y + 19, static_cast<int>(w - 56));
    }
    if (shown_ >= line.text.size() && static_cast<int>(t * 2) % 2 == 0) Ui::downTriangle(x + w - 14, y + h - 11, pal::Amber);
}

// ---------------------------------------------------------------- ChoiceMenu

void ChoiceMenu::open(const std::string& who, const std::string& title, std::vector<TalkOption> options) {
    who_ = who;
    title_ = title;
    options_ = std::move(options);
    selected_ = 0;
    open_ = true;
}

void ChoiceMenu::choose(int i) {
    if (i < 0 || i >= static_cast<int>(options_.size())) return;
    auto run = options_[i].run;
    open_ = false;
    if (run) run();
}

void ChoiceMenu::handle(Input::Act a) {
    int n = static_cast<int>(options_.size());
    if (a == Input::Act::Up) selected_ = (selected_ + n - 1) % n;
    if (a == Input::Act::Down) selected_ = (selected_ + 1) % n;
    if (a == Input::Act::Accept) choose(selected_);
    if (a == Input::Act::Back) open_ = false;
}

void ChoiceMenu::draw(float t, HitList& hits) {
    if (!open_) return;
    (void)t;
    float w = 110;
    for (const auto& o : options_) w = std::max(w, Ui::width(o.label) + 30.0f);
    float h = 24 + options_.size() * 16.0f, x = 640 - w - 140, y = 258 - h;
    hits.add(Rectangle{0, 0, 640, 360}, [this] { open_ = false; });
    Ui::panel(x, y, w, h, pal::PanelLineHi, pal::PanelFillDark);
    Ui::text(title_, x + 10, y + 7, speakerColor(who_));
    for (size_t i = 0; i < options_.size(); ++i) {
        float yy = y + 20 + i * 16.0f;
        bool f = selected_ == static_cast<int>(i);
        if (f) {
            Ui::rect(x + 5, yy, w - 10, 14, pal::Select);
            Ui::text(">", x + 9, yy + 4, pal::Amber);
        }
        Ui::text(options_[i].label, x + 18, yy + 4, f ? pal::Ink : pal::Dim);
        int idx = static_cast<int>(i);
        hits.add(Rectangle{x + 5, yy, w - 10, 14}, [this, idx] { choose(idx); }, [this, idx] { selected_ = idx; });
    }
}

// ---------------------------------------------------------------- PauseMenu

const std::vector<PauseMenu::Choice>& PauseMenu::items() {
    static const std::vector<Choice> v = {Choice::Resume, Choice::Journal, Choice::Power, Choice::Crew, Choice::Map, Choice::Cargo,
                                          Choice::TextSpeed, Choice::ClockSpeed, Choice::Sound, Choice::Quit};
    return v;
}

PauseMenu::Choice PauseMenu::handle(Input::Act a) {
    int n = static_cast<int>(items().size());
    if (a == Input::Act::Up) selected_ = (selected_ + n - 1) % n;
    if (a == Input::Act::Down) selected_ = (selected_ + 1) % n;
    if (a == Input::Act::Accept) return items()[selected_];
    if (a == Input::Act::Back || a == Input::Act::Pause) return Choice::Resume;
    return Choice::None;
}

void PauseMenu::draw(const Session& s, float t, HitList& hits, int textSpeed, bool sound, float clockMul) {
    if (!open_) return;
    Ui::rect(0, 0, 640, 360, Color{3, 5, 10, 158});
    hits.add(Rectangle{0, 0, 640, 360}, nullptr);
    const float x = 426, w = 214;
    Ui::panel(x, 0, w, 360, pal::PanelLineHi, pal::PanelFillDark);
    Ui::text("MENU", x + 14, 10, pal::Amber, 2);
    Ui::text("The clock is stopped", x + 14, 28, pal::Dim);
    static const char* speeds[3] = {"Normal", "Fast", "Instant"};
    for (size_t i = 0; i < items().size(); ++i) {
        std::string label;
        switch (items()[i]) {
            case Choice::Resume: label = "Resume"; break;
            case Choice::Journal: label = "Journal"; break;
            case Choice::Power: label = "Power Panel"; break;
            case Choice::Crew: label = "Crew Panel"; break;
            case Choice::Map: label = "Map"; break;
            case Choice::Cargo: label = "Cargo"; break;
            case Choice::TextSpeed: label = std::string("Text speed: ") + speeds[textSpeed]; break;
            case Choice::ClockSpeed: label = "Clock: " + std::to_string(static_cast<int>(clockMul)) + "x" + (clockMul == 1 ? " (relaxed)" : " (normal)"); break;
            case Choice::Sound: label = std::string("Sound: ") + (sound ? "On" : "Off"); break;
            default: label = "Quit to title"; break;
        }
        float y = 42 + i * 15.0f;
        bool f = selected_ == static_cast<int>(i);
        Ui::rect(x + 10, y, w - 20, 13, f ? pal::Select : pal::rgb(0x111A2B));
        if (f) Ui::brackets(x + 10, y, w - 20, 13, t);
        Ui::text((f ? "> " : "  ") + label, x + 16, y + 3, f ? pal::Ink : pal::Dim);
        int idx = static_cast<int>(i);
        Choice c = items()[i];
        hits.add(Rectangle{x + 10, y, w - 20, 13}, [this, c] { clicked_ = c; }, [this, idx] { selected_ = idx; });
    }
    Ui::text("ACT TRACKER", x + 14, 198, pal::Dim);
    struct ActRow { const char* n; const char* name; const char* hours; float a, b; };
    const ActRow acts[4] = {{"1", "Dark", "H0 to 6", 0, 6}, {"2", "The Signal", "H6 to 24", 6, 24}, {"3", "Holding On", "H24 to 60", 24, 60}, {"4", "The Light", "H60 to 72", 60, 72}};
    float hour = s.clock().hour();
    for (int i = 0; i < 4; ++i) {
        float y = 210 + i * 36.0f;
        bool done = hour >= acts[i].b, cur = hour >= acts[i].a && hour < acts[i].b;
        Ui::rect(x + 12, y, w - 24, 30, cur ? pal::RowHi : pal::PanelFill);
        Ui::rect(x + 12, y, 2, 30, done ? pal::Teal : cur ? pal::Amber : pal::rgb(0x26324A));
        Ui::text(std::string("Act ") + acts[i].n + "  " + acts[i].name, x + 20, y + 5, done || cur ? pal::Ink : pal::Offline);
        std::string right = done ? "Done" : cur ? "Now" : acts[i].hours;
        Ui::textRight(right, x + w - 18, y + 5, done ? pal::TealHi : cur ? pal::Amber : pal::Offline);
        float k = std::max(0.0f, std::min(1.0f, (hour - acts[i].a) / (acts[i].b - acts[i].a)));
        Ui::rect(x + 20, y + 19, w - 40, 4, pal::SegEmpty);
        Ui::rect(x + 20, y + 19, std::round((w - 40) * k), 4, done ? pal::Teal : pal::Amber);
    }
}
