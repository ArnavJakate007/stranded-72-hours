#include "logic/SaveManager.h"
#include <cstdio>
#include <fstream>

std::string SaveData::get(const std::string& key, const std::string& fallback) const {
    auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

float SaveData::num(const std::string& key, float fallback) const {
    auto it = values_.find(key);
    if (it == values_.end()) return fallback;
    try {
        return std::stof(it->second);
    } catch (...) {
        return fallback;
    }
}

SaveManager::SaveManager(std::string folder) : folder_(std::move(folder)) { loadProgress(); }

bool SaveManager::writeSave(const SaveData& data) const {
    std::ofstream out(folder_ + "/save.txt", std::ios::trunc);
    if (!out) return false;
    out << "version=" << VERSION << "\n";
    for (const auto& kv : data.all()) out << kv.first << "=" << kv.second << "\n";
    return static_cast<bool>(out);
}

bool SaveManager::readSave(SaveData& out) const {
    std::ifstream in(folder_ + "/save.txt");
    if (!in) return false;
    std::string line;
    if (!std::getline(in, line) || line != "version=" + std::to_string(VERSION)) return false;
    SaveData d;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) return false;   // damaged file: hide Continue instead of crashing
        d.set(line.substr(0, eq), line.substr(eq + 1));
    }
    if (!d.has("hour") || !d.has("bar_Oxygen")) return false;
    out = d;
    return true;
}

bool SaveManager::hasSave() const {
    SaveData d;
    return readSave(d);
}

void SaveManager::deleteSave() const { std::remove((folder_ + "/save.txt").c_str()); }

void SaveManager::unlockEnding(const std::string& code) {
    endings_.insert(code);
    writeProgress();
}

void SaveManager::loadProgress() {
    std::ifstream in(folder_ + "/progress.txt");
    std::string line;
    static const std::set<std::string> known = {"F1", "F2", "F3", "F4", "E1", "E2", "E3"};
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (known.count(line)) endings_.insert(line);   // unknown codes are ignored
    }
}

void SaveManager::writeProgress() const {
    std::ofstream out(folder_ + "/progress.txt", std::ios::trunc);
    for (const auto& e : endings_) out << e << "\n";
}
