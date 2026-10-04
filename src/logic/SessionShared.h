#pragma once
// Helpers shared by the Session*.cpp files (Session is one class split by responsibility).
#include "logic/Types.h"
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace session_detail {
inline const IVec kStart{50, 30};

struct Spawn { const char* name; int x, y; };
inline const Spawn kSpawns[] = {
    // Act 1-2
    {"Power cell", 55, 39}, {"Trauma kit", 78, 51}, {"Antenna dish", 82, 55}, {"Flight log", 84, 50},
    {"Amplifier", 12, 43}, {"Large power cell", 7, 61}, {"Flight recorder", 5, 59},
    // Act 3-4
    {"Seeds", 76, 50}, {"Thermal layer", 80, 56}, {"Medical stock", 84, 56}, {"Generator coil", 93, 61}, {"Nav computer", 91, 52},
    {"Ice", 22, 48}, {"Ice", 26, 51}, {"Ice", 23, 53}, {"Ice", 27, 49},
    // common materials (areas never refill, so there is no point grinding)
    {"Scrap", 66, 44}, {"Scrap", 70, 58}, {"Scrap", 64, 55}, {"Scrap", 26, 48}, {"Scrap", 25, 53}, {"Scrap", 64, 41}, {"Scrap", 68, 43},
    {"Scrap", 71, 40}, {"Scrap", 66, 60}, {"Scrap", 69, 66}, {"Scrap", 73, 64}, {"Scrap", 77, 45}, {"Scrap", 80, 44}, {"Scrap", 85, 46},
    {"Scrap", 88, 45}, {"Scrap", 84, 66}, {"Scrap", 62, 58},
    {"Cables", 72, 62}, {"Cables", 22, 52}, {"Cables", 65, 52}, {"Cables", 70, 50}, {"Cables", 76, 62}, {"Cables", 86, 60},
    {"Cables", 79, 66}, {"Cables", 67, 57}, {"Cables", 72, 42}, {"Cables", 81, 46}, {"Cables", 87, 63}, {"Cables", 75, 60}};

inline std::string fmt0(float v) { return std::to_string(static_cast<int>(std::lround(v))); }

inline std::string joinList(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + v[i];
    return out;
}

inline std::vector<std::string> splitList(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

inline std::string bools(const std::vector<bool>& v) {
    std::string s;
    for (bool b : v) s += b ? '1' : '0';
    return s;
}
inline void readBools(const std::string& s, std::vector<bool>& v) {
    for (size_t i = 0; i < v.size() && i < s.size(); ++i) v[i] = s[i] == '1';
}

// Who is trained for which job: Arjun machines, Maya comms, Elena the greenhouse and medical, Hana navigation.
inline bool rightSkill(const std::string& crew, const std::string& job) {
    if (crew == "arjun") return job == "leak" || job == "purifier" || job == "brace" || job == "repair" || job == "reactor";
    if (crew == "maya") return job == "tx";
    if (crew == "elena") return job == "greenhouse" || job == "cover";
    if (crew == "hana") return job == "plot";
    return false;
}
}  // namespace session_detail
