#pragma once
#include "logic/DataStore.h"
#include <deque>
#include <string>

struct DialogueLine {
    std::string speaker;   // empty = narration / inspect text
    std::string text;
    bool block = false;    // key line: waits for Z and stops the clock (FR-7)
};

// Queue of lines waiting to be shown. The dialogue box (display side) types them out and pops them.
class DialogueQueue {
public:
    explicit DialogueQueue(const DialogueBook& book) : book_(book) {}

    void say(const std::string& id, const std::vector<std::string>& args = {});
    void sayOnce(const std::string& id, const std::vector<std::string>& args = {});
    void sayText(const std::string& speaker, const std::string& text, bool block);
    void clear() { lines_.clear(); spoken_.clear(); }

    bool empty() const { return lines_.empty(); }
    const DialogueLine& current() const { return lines_.front(); }
    bool blocking() const { return !lines_.empty() && lines_.front().block; }
    void pop() { if (!lines_.empty()) lines_.pop_front(); }

private:
    const DialogueBook& book_;
    std::deque<DialogueLine> lines_;
    std::deque<std::string> spoken_;   // ids already said once
};
