#include "logic/DataStore.h"
#include "logic/PowerException.h"
#include <fstream>

namespace {
std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::vector<std::string>> readRecords(const std::string& path, size_t minFields) {
    std::ifstream in(path);
    if (!in) throw DataError("Missing data file: " + path);
    std::vector<std::vector<std::string>> out;
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        auto f = splitFields(t);
        if (f.size() < minFields) throw DataError("Bad line " + std::to_string(lineNo) + " in " + path);
        out.push_back(f);
    }
    return out;
}

const char* REQUIRED_BALANCE[] = {
    "oxygen_start", "warmth_start", "water_start", "food_start", "oxygen_drain", "leak_extra_drain", "warmth_drain",
    "water_drain", "food_drain", "oxygen_refill", "heater_refill", "purifier_refill", "greenhouse_refill", "comms_charge",
    "medical_heal", "crew_low_bar_damage", "low_bar_threshold", "hana_start_hp", "hana_untreated_drain",
    "hana_recover_threshold", "suit_step_cost", "suit_deep_multiplier", "suit_heavy_multiplier", "suit_warning",
    "wait_suit_per_hour", "units_after_t1", "units_with_generator", "crew_task_hours", "puzzle_fail_hours",
    "walk_step_seconds", "heavy_step_seconds", "seconds_per_game_hour", "default_clock_multiplier", "collapse_hours",
    "carry_slots", "storm_warmth_multiplier", "battery_charge_per_free_unit", "battery_safe_level",
    "greenhouse_grow_hours", "reactor_damage", "radiation_per_step", "cold_damage", "storm_debris_damage", "kiran_heal", "water_reserve", "idle_suit_per_hour"};
}  // namespace

std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '|') {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(trim(cur));
    return out;
}

void Balance::load(const std::string& path) {
    for (const auto& r : readRecords(path, 2)) {
        try {
            values_[r[0]] = std::stof(r[1]);
        } catch (...) {
            throw DataError("Not a number for '" + r[0] + "' in " + path);
        }
    }
    for (const char* key : REQUIRED_BALANCE)
        if (!has(key)) throw DataError(std::string("Missing value '") + key + "' in " + path);
}

float Balance::get(const std::string& key) const {
    auto it = values_.find(key);
    if (it == values_.end()) throw DataError("Missing balance value: " + key);
    return it->second;
}

void ItemCatalog::load(const std::string& path) {
    for (const auto& r : readRecords(path, 4)) {
        if (r[2] != "Parts" && r[2] != "Supplies" && r[2] != "Story" && r[2] != "Tools")
            throw DataError("Unknown tab '" + r[2] + "' in " + path);
        items_[r[0]] = ItemDef{r[0], r[1] == "yes", r[2], r[3]};
    }
}

const ItemDef& ItemCatalog::get(const std::string& name) const {
    auto it = items_.find(name);
    if (it == items_.end()) throw DataError("Unknown item: " + name);
    return it->second;
}

bool ItemCatalog::isHeavy(const std::string& name) const {
    auto it = items_.find(name);
    return it != items_.end() && it->second.heavy;
}

void DialogueBook::load(const std::string& path) {
    static const char* speakers[] = {"-", "maya", "arjun", "elena", "hana", "voss", "kiran"};
    for (const auto& r : readRecords(path, 4)) {
        bool known = false;
        for (const char* s : speakers) known = known || r[1] == s;
        if (!known) throw DataError("Unknown speaker '" + r[1] + "' in " + path);
        // the text field may itself contain '|' characters: join the rest back together
        std::string text = r[3];
        for (size_t i = 4; i < r.size(); ++i) text += "|" + r[i];
        lines_[r[0]] = DialogueEntry{r[1] == "-" ? "" : r[1], r[2] == "yes", text};
    }
}

DialogueEntry DialogueBook::get(const std::string& id, const std::vector<std::string>& args) const {
    auto it = lines_.find(id);
    if (it == lines_.end()) return DialogueEntry{"", false, "[missing line: " + id + "]"};
    DialogueEntry e = it->second;
    for (size_t i = 0; i < args.size(); ++i) {
        std::string key = "{" + std::to_string(i) + "}";
        size_t pos;
        while ((pos = e.text.find(key)) != std::string::npos) e.text.replace(pos, key.size(), args[i]);
    }
    return e;
}

void DataStore::loadAll(const std::string& folder) {
    balance.load(folder + "/balance.txt");
    items.load(folder + "/items.txt");
    dialogue.load(folder + "/dialogue.txt");
}
