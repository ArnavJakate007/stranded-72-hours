#pragma once
#include <map>
#include <set>
#include <string>

// A flat key=value snapshot of a run, written at the start of each act (FR-53).
class SaveData {
public:
    void set(const std::string& key, const std::string& value) { values_[key] = value; }
    void setNum(const std::string& key, float v) { values_[key] = std::to_string(v); }
    std::string get(const std::string& key, const std::string& fallback = "") const;
    float num(const std::string& key, float fallback = 0.0f) const;
    bool has(const std::string& key) const { return values_.count(key) > 0; }
    const std::map<std::string, std::string>& all() const { return values_; }
    std::map<std::string, std::string>& all() { return values_; }

private:
    std::map<std::string, std::string> values_;
};

// File handling for save.txt (act checkpoint) and progress.txt (endings across runs).
class SaveManager {
public:
    static constexpr int VERSION = 1;
    explicit SaveManager(std::string folder);

    bool writeSave(const SaveData& data) const;
    bool readSave(SaveData& out) const;   // false if missing, wrong version or damaged (NFR-5)
    bool hasSave() const;
    void deleteSave() const;

    void unlockEnding(const std::string& code);
    const std::set<std::string>& endings() const { return endings_; }

private:
    void loadProgress();
    void writeProgress() const;
    std::string folder_;
    std::set<std::string> endings_;
};
