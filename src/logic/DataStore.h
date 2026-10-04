#pragma once
#include <map>
#include <string>
#include <vector>

// Everything tunable lives in plain text files under data/ (SRS section 6, NFR-11).
// Each record is one line, fields separated by '|', lines starting with '#' are comments.

class Balance {
public:
    void load(const std::string& path);
    float get(const std::string& key) const;   // throws DataError if missing
    int geti(const std::string& key) const { return static_cast<int>(get(key)); }
    void set(const std::string& key, float v) { values_[key] = v; }
    bool has(const std::string& key) const { return values_.count(key) > 0; }

private:
    std::map<std::string, float> values_;
};

struct ItemDef {
    std::string name;
    bool heavy = false;
    std::string tab;          // Parts, Supplies, Story, Tools
    std::string description;
};

class ItemCatalog {
public:
    void load(const std::string& path);
    const ItemDef& get(const std::string& name) const;
    bool isHeavy(const std::string& name) const;
    bool has(const std::string& name) const { return items_.count(name) > 0; }

private:
    std::map<std::string, ItemDef> items_;
};

struct DialogueEntry {
    std::string speaker;   // maya, arjun, elena, hana, voss, kiran, or empty for narration
    bool block = false;    // key line: waits for Z and pauses the clock
    std::string text;
};

class DialogueBook {
public:
    void load(const std::string& path);
    bool has(const std::string& id) const { return lines_.count(id) > 0; }
    // Returns the entry with {0}, {1}, ... replaced by args.
    DialogueEntry get(const std::string& id, const std::vector<std::string>& args = {}) const;

private:
    std::map<std::string, DialogueEntry> lines_;
};

class DataStore {
public:
    void loadAll(const std::string& folder);
    Balance balance;
    ItemCatalog items;
    DialogueBook dialogue;
};

// Splits a line on '|' and trims each field.
std::vector<std::string> splitFields(const std::string& line);
