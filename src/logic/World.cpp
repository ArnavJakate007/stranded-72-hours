#include "logic/World.h"
#include "logic/Rng.h"
#include <cmath>

const std::string World::kNone;

int Room::X() const { return World::BASE_X + x * 16; }
int Room::Y() const { return World::BASE_Y + y * 16; }

namespace {
std::string edgeKey(IVec a, IVec b) {
    if (b.x < a.x || (b.x == a.x && b.y < a.y)) std::swap(a, b);
    return std::to_string(a.x) + "," + std::to_string(a.y) + "|" + std::to_string(b.x) + "," + std::to_string(b.y);
}
struct OldSolid { int x, y, w, h; const char* name; };
// Furniture rectangles in the original 640x360 base layout; shifted by OLD_OX/OLD_OY.
const OldSolid kFurniture[] = {
    {174, 68, 26, 32, "Comms Dish"}, {204, 96, 44, 14, "Comms Console"}, {232, 60, 18, 28, "Server Rack"}, {168, 120, 14, 11, "Crate"},
    {262, 60, 30, 12, "O2 Tank"}, {344, 60, 30, 12, "O2 Tank"}, {289, 59, 14, 14, "O2 Fan"},
    {440, 58, 32, 12, "Medical Cabinet"}, {396, 84, 16, 30, "Empty Bed"}, {446, 84, 16, 30, "Hana Bed"},
    {189, 163, 42, 42, "Power Core"}, {174, 200, 14, 16, "Power Core"},
    {300, 94, 40, 12, "Command Desk"}, {294, 136, 52, 32, "Map Table"}, {358, 90, 14, 18, "Plant"}, {260, 202, 14, 16, "Plant"},
    {394, 158, 76, 18, "Planter"}, {394, 186, 76, 18, "Planter"},
    {170, 242, 20, 34, "Bunk"}, {202, 242, 20, 34, "Bunk"}, {234, 242, 20, 34, "Bunk"}, {266, 242, 20, 34, "Bunk"},
    {172, 296, 70, 14, "Lockers"}, {286, 293, 12, 14, "Plant"},
    {346, 240, 30, 26, "Crates"}, {354, 268, 16, 13, "Crates"}, {378, 286, 14, 12, "Crates"}, {346, 290, 14, 12, "Crates"},
    {396, 228, 32, 14, "Shelf"}, {398, 288, 31, 13, "Water Barrels"}, {432, 262, 40, 40, "Purifier Plot"}};
}  // namespace

World::World() {
    rooms_ = {
        {"comms", "Comms", "comms", 0, 0, 6, 6, {205, 215, 255}, {169, 162, 148}, {159, 152, 137}},
        {"north", "O2 Plant", "oxygen", 6, 0, 8, 2, {200, 228, 255}, {169, 162, 148}, {159, 152, 137}},
        {"medical", "Medical Bay", "medical", 14, 0, 6, 6, {255, 236, 215}, {205, 191, 168}, {194, 180, 156}},
        {"eng", "Power Core", "core", 0, 6, 6, 5, {255, 160, 105}, {169, 146, 122}, {158, 136, 112}},
        {"command", "Command", "", 6, 2, 8, 9, {255, 204, 145}, {192, 154, 110}, {180, 143, 100}},
        {"green", "Greenhouse", "greenhouse", 14, 6, 6, 5, {214, 255, 176}, {168, 154, 114}, {157, 143, 104}},
        {"quarters", "Crew Quarters", "heater", 0, 11, 9, 6, {255, 192, 125}, {184, 148, 112}, {173, 138, 102}},
        {"south", "Hall", "", 9, 11, 2, 6, {255, 204, 145}, {176, 144, 112}, {165, 134, 103}},
        {"storage", "Storage", "", 11, 11, 9, 6, {255, 198, 138}, {184, 151, 108}, {173, 140, 98}},
    };
    doors_ = {{true, 6, 3.5f, 1.5f}, {true, 14, 3.5f, 1.5f}, {false, 9.25f, 2, 1.5f}, {true, 6, 8, 1.5f}, {true, 14, 8, 1.5f},
              {false, 9, 11, 2}, {true, 9, 13.5f, 1.5f}, {true, 11, 13.5f, 1.5f}, {false, 9, 17, 2}};
    for (const Door& d : doors_) {
        if (d.vertical) {
            for (int r = static_cast<int>(std::floor(d.b)); r <= static_cast<int>(std::ceil(d.b + d.len)) - 1; ++r)
                doorEdges_.insert(edgeKey({static_cast<int>(d.a) - 1 + BASE_TX, r + BASE_TY}, {static_cast<int>(d.a) + BASE_TX, r + BASE_TY}));
        } else {
            for (int c = static_cast<int>(std::floor(d.a)); c <= static_cast<int>(std::ceil(d.a + d.len)) - 1; ++c)
                doorEdges_.insert(edgeKey({c + BASE_TX, static_cast<int>(d.b) - 1 + BASE_TY}, {c + BASE_TX, static_cast<int>(d.b) + BASE_TY}));
        }
    }
    zones_ = {
        {"Wreck Field", "Deep: reactor", 20, 87, 56, 11, 14, true}, {"Wreck Field", "Middle: cargo bay", 10, 74, 40, 13, 30},
        {"Wreck Field", "Entrance", 5, 63, 38, 11, 32},
        {"Ice Cave", "Deep", 20, 2, 53, 14, 16, true}, {"Ice Cave", "Middle", 10, 5, 37, 17, 14}, {"Ice Cave", "Entrance", 5, 16, 44, 17, 12},
        {"Ridge", "Summit", 20, 44, 2, 14, 7, true}, {"Ridge", "Slope: wind", 10, 36, 8, 30, 10}, {"Ridge", "Path", 5, 45, 18, 10, 4},
        {"Landing Zone", "Locked until H60", 0, 70, 3, 28, 32}, {"Crash Site", "Around the base", 0, 26, 22, 40, 32}};
    wilds_ = {"Kepler-X", "Open ice", 0, 0, 0, 0, 0};
    lamps_ = {{624, 430}, {624, 676}, {976, 430}, {976, 676}, {770, 716}, {832, 716}, {980, 520}, {488, 770}};
    debris_ = {{79, 62}, {80, 62}, {81, 62}};
    wreckage_ = {{78, 63}, {82, 63}, {84, 61}, {77, 60}, {83, 64}};
    braces_ = {{38, 30}, {63, 36}};
    hatch_ = {{92, 58}, {93, 58}};
    padDebris_ = {{84, 15}, {88, 22}, {83, 23}};
    mounts_ = {{79, 12}, {93, 12}, {79, 26}, {93, 26}, {93, 19}, {86, 27}};
    generate();
}

void World::set(int x, int y, Tile t) {
    if (inBounds(x, y)) tiles_[idx(x, y)] = t;
}
void World::rect(int x, int y, int w, int h, Tile t) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i) set(i, j, t);
}
void World::ellipse(int cx, int cy, int rx, int ry, Tile t) {
    for (int j = cy - ry; j <= cy + ry; ++j)
        for (int i = cx - rx; i <= cx + rx; ++i) {
            float dx = float(i - cx) / rx, dy = float(j - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) set(i, j, t);
        }
}
void World::line(int x0, int y0, int x1, int y1, int w, Tile t) {
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    if (n == 0) n = 1;
    for (int s = 0; s <= n; ++s) {
        int x = static_cast<int>(std::lround(x0 + float(x1 - x0) * s / n));
        int y = static_cast<int>(std::lround(y0 + float(y1 - y0) * s / n));
        rect(x - (w >> 1), y - (w >> 1), w, w, t);
    }
}
void World::markRect(int x, int y, int w, int h, const std::string& name) {
    for (int ty = y / 16; ty <= (y + h) / 16; ++ty)
        for (int tx = x / 16; tx <= (x + w) / 16; ++tx) {
            if (!inBounds(tx, ty)) continue;
            int ox = std::min(tx * 16 + 16, x + w) - std::max(tx * 16, x);
            int oy = std::min(ty * 16 + 16, y + h) - std::max(ty * 16, y);
            if (ox > 0 && oy > 0 && ox * oy >= 96) solids_[idx(tx, ty)] = name;
        }
}
void World::markTile(int tx, int ty, const std::string& name) {
    if (inBounds(tx, ty) && solids_[idx(tx, ty)].empty()) solids_[idx(tx, ty)] = name;
}

void World::generate() {
    tiles_.assign(W * H, Tile::Snow);
    solids_.assign(W * H, "");
    zones_[9].sub = "Locked until H60";
    trees_.clear();
    boulders_.clear();
    // Ridge band in the north with a carved path, windy slope and summit
    rect(0, 0, W, 22, Tile::Rock);
    rect(46, 17, 8, 6, Tile::Snow); ellipse(51, 12, 14, 5, Tile::Snow); rect(37, 9, 28, 8, Tile::Snow);
    ellipse(51, 5, 6, 3, Tile::Snow); rect(46, 3, 11, 5, Tile::Snow); line(51, 7, 51, 10, 3, Tile::Snow);
    // Landing zone clearing
    rect(70, 4, 28, 31, Tile::Snow);
    // Ice cave massif with three chambers
    rect(0, 33, 32, 37, Tile::CaveWall);
    line(32, 50, 24, 50, 3, Tile::CaveFloor); ellipse(24, 50, 4, 4, Tile::CaveFloor); line(21, 49, 16, 45, 2, Tile::CaveFloor);
    ellipse(13, 44, 6, 5, Tile::CaveFloor); line(12, 48, 9, 56, 2, Tile::CaveFloor); ellipse(8, 60, 5, 4, Tile::CaveFloor);
    Rng r(21);
    // Wreck field scorch plates
    for (int i = 0; i < 40; ++i) {
        int cx = 63 + static_cast<int>(r.next() * 33), cy = 40 + static_cast<int>(r.next() * 27);
        ellipse(cx, cy, 1 + static_cast<int>(r.next() * 2), 1, Tile::Metal);
    }
    // Landing zone fence, gate and pad
    for (int x = 76; x <= 95; ++x) { set(x, 8, Tile::Fence); set(x, 30, Tile::Fence); }
    for (int y = 8; y <= 30; ++y) { set(76, y, Tile::Fence); set(95, y, Tile::Fence); }
    set(76, 19, Tile::Gate); set(76, 20, Tile::Gate);
    rect(80, 12, 13, 15, Tile::Pad);
    // ARK-7 cargo bay hull with a hatch on the west side, plus the reactor wreck
    rect(74, 48, 13, 11, Tile::Hull); rect(75, 49, 11, 9, Tile::Metal); set(74, 52, Tile::Metal); set(74, 53, Tile::Metal);
    rect(87, 49, 4, 6, Tile::Hull);
    rect(89, 58, 8, 9, Tile::Hull); rect(90, 59, 6, 7, Tile::Metal); set(92, 58, Tile::Metal); set(93, 58, Tile::Metal);
    // Trails
    rect(49, 43, 2, 8, Tile::Path); rect(31, 49, 20, 2, Tile::Path); rect(50, 46, 24, 2, Tile::Path); rect(72, 46, 2, 8, Tile::Path);
    rect(61, 23, 2, 24, Tile::Path); rect(49, 23, 14, 2, Tile::Path); rect(49, 17, 2, 7, Tile::Path);
    rect(62, 30, 9, 2, Tile::Path); rect(69, 19, 2, 13, Tile::Path); rect(70, 19, 6, 2, Tile::Path);
    rect(BASE_TX, BASE_TY, 20, 17, Tile::Base);
    rect(PAD_TILE.x, PAD_TILE.y, 6, 6, Tile::Pad);
    // World border
    rect(0, 0, W, 2, Tile::Rock); rect(0, H - 2, W, 2, Tile::Rock); rect(0, 0, 2, H, Tile::Rock); rect(W - 2, 0, 2, H, Tile::Rock);
    // Ice crystals around chamber edges, then re-carve the tunnels so nothing blocks them
    const int chambers[3][4] = {{24, 50, 4, 4}, {13, 44, 6, 5}, {8, 60, 5, 4}};
    for (const auto& ch : chambers)
        for (int k = 0; k < 9; ++k) {
            float a = r.next() * 6.2831853f;
            int x = static_cast<int>(std::lround(ch[0] + std::cos(a) * (ch[2] - 0.6f)));
            int y = static_cast<int>(std::lround(ch[1] + std::sin(a) * (ch[3] - 0.6f)));
            if (tile(x, y) == Tile::CaveFloor && std::abs(y - 50) > 1 && !(std::abs(x - ch[0]) < 2 && std::abs(y - ch[1]) < 2)) set(x, y, Tile::Ice);
        }
    line(32, 50, 24, 50, 3, Tile::CaveFloor); line(21, 49, 16, 45, 2, Tile::CaveFloor); line(12, 48, 9, 56, 2, Tile::CaveFloor);
    const int keep[][2] = {{12, 43}, {7, 61}, {5, 59}, {25, 53}, {26, 48}, {22, 52}, {13, 44}, {8, 60}, {24, 50}};
    for (const auto& k : keep) set(k[0], k[1], Tile::CaveFloor);

    // Trees and boulders, kept away from trails and the main areas
    auto nearAny = [&](int x, int y, int rad) {
        for (int j = -rad; j <= rad; ++j)
            for (int i = -rad; i <= rad; ++i) {
                Tile t = tile(x + i, y + j);
                if (t == Tile::Path || t == Tile::Pad || t == Tile::Gate || t == Tile::Base || t == Tile::Metal) return true;
            }
        return false;
    };
    const int exclude[][4] = {{35, 21, 31, 27}, {44, 2, 15, 21}, {75, 7, 22, 25}, {62, 37, 36, 31}, {26, 46, 8, 8}};
    for (int y = 2; y < H - 2; ++y)
        for (int x = 2; x < W - 2; ++x) {
            if (tile(x, y) != Tile::Snow) continue;
            bool ex = false;
            for (const auto& e : exclude) ex = ex || (x >= e[0] && x < e[0] + e[2] && y >= e[1] && y < e[1] + e[3]);
            if (ex || nearAny(x, y, 2)) continue;
            float v = r.next();
            if (v < 0.10f) {
                trees_.push_back({x, y, 4 + static_cast<int>(r.next() * 4)});
                solids_[idx(x, y)] = "Tree";
            } else if (v < 0.13f) {
                boulders_.push_back({x, y, 10 + static_cast<int>(r.next() * 5)});
                solids_[idx(x, y)] = "Boulder";
            }
        }
    for (int i = 0; i < 14; ++i) {
        int x = 40 + static_cast<int>(r.next() * 24), y = 9 + static_cast<int>(r.next() * 8);
        if (tile(x, y) == Tile::Snow && std::abs(x - 51) > 2 && solids_[idx(x, y)].empty()) {
            boulders_.push_back({x, y, 10 + static_cast<int>(r.next() * 5)});
            solids_[idx(x, y)] = "Boulder";
        }
    }
    for (const auto& f : kFurniture) markRect(f.x + OLD_OX, f.y + OLD_OY, f.w, f.h, f.name);
    for (const IVec& p : lamps_) markTile(p.x / 16, (p.y - 4) / 16, "Lamp");
    solids_[idx(LEAK.x, LEAK.y)] = "Oxygen Leak";
    for (const IVec& d : debris_) solids_[idx(d.x, d.y)] = "Debris";
    solids_[idx(ANTENNA.x, ANTENNA.y)] = "Antenna Site";
    solids_[idx(GENERATOR.x, GENERATOR.y)] = "Wreck Generator";
    for (const IVec& b : braces_) solids_[idx(b.x, b.y)] = "Brace Point";
    for (const IVec& h : hatch_) solids_[idx(h.x, h.y)] = "Reactor Hatch";
    solids_[idx(REACTOR.x, REACTOR.y)] = "Reactor";
    for (const IVec& d : padDebris_) solids_[idx(d.x, d.y)] = "Pad Debris";
    for (const IVec& m : mounts_) solids_[idx(m.x, m.y)] = "Light Mount";
    solids_[idx(BEACON_SITE.x, BEACON_SITE.y)] = "Beacon Site";
    std::vector<IVec> keptWreckage;
    for (const IVec& w : wreckage_)
        if (tile(w.x, w.y) == Tile::Snow || tile(w.x, w.y) == Tile::Metal) {
            solids_[idx(w.x, w.y)] = "Wreckage";
            keptWreckage.push_back(w);
        }
    wreckage_ = keptWreckage;
}

void World::openGate() {
    set(76, 19, Tile::Path);
    set(76, 20, Tile::Path);
    zones_[9].sub = "Shuttle approach";
}

bool World::solidTile(int x, int y) const {
    Tile t = tile(x, y);
    return t == Tile::Rock || t == Tile::CaveWall || t == Tile::Hull || t == Tile::Fence || t == Tile::Ice || t == Tile::Gate;
}

const std::string& World::solidName(int x, int y) const { return inBounds(x, y) ? solids_[idx(x, y)] : kNone; }

void World::setSolidName(int x, int y, const std::string& n) {
    if (inBounds(x, y)) solids_[idx(x, y)] = n;
}

std::string World::roomAt(int tx, int ty) const {
    int bx = tx - BASE_TX, by = ty - BASE_TY;
    if (bx < 0 || by < 0 || bx >= 20 || by >= 17) return "out";
    for (const Room& r : rooms_)
        if (bx >= r.x && bx < r.x + r.w && by >= r.y && by < r.y + r.h) return r.key;
    return "out";
}

const Room* World::room(const std::string& key) const {
    for (const Room& r : rooms_)
        if (r.key == key) return &r;
    return nullptr;
}

bool World::passEdge(IVec a, IVec b) const {
    std::string ra = roomAt(a.x, a.y), rb = roomAt(b.x, b.y);
    return ra == rb || doorEdges_.count(edgeKey(a, b)) > 0;
}

bool World::inWind(int tx, int ty) const { return tx >= 37 && tx < 65 && ty >= 8 && ty < 18 && roomAt(tx, ty) == "out"; }

const Zone& World::zoneAt(int tx, int ty) const {
    for (const Zone& z : zones_)
        if (z.contains(tx, ty)) return z;
    return wilds_;
}
