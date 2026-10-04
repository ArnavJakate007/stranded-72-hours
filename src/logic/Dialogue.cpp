#include "logic/Dialogue.h"
#include <algorithm>

void DialogueQueue::say(const std::string& id, const std::vector<std::string>& args) {
    DialogueEntry e = book_.get(id, args);
    lines_.push_back({e.speaker, e.text, e.block});
}

void DialogueQueue::sayOnce(const std::string& id, const std::vector<std::string>& args) {
    if (std::find(spoken_.begin(), spoken_.end(), id) != spoken_.end()) return;
    spoken_.push_back(id);
    say(id, args);
}

void DialogueQueue::sayText(const std::string& speaker, const std::string& text, bool block) {
    lines_.push_back({speaker, text, block});
}
