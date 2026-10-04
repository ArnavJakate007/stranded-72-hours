#pragma once
#include "logic/Types.h"
#include <set>
#include <string>
#include <vector>

enum class Tile : uint8_t { Snow, Rock, Path, CaveFloor, CaveWall, Metal, Hull, Fence, Pad, Ice, Base, Gate };

struct Room {
    std::string key, name, system;   // system id that lights this room, or "core", or ""
    int x, y, w, h;                  // in tiles, relative to the base origin
    RGB light, floorA, floorB;
    int X() const;                   // world pixel rectangle
    int Y() const;
    int W() const { return w * 16; }
    int H() const { return h * 16; }
};

struct Door { bool vertical; float a, b, len; };   // building-relative tile coordinates

struct Zone {
    std::string name, sub;
    int cost;       // suit oxygen charged on entry
    int x, y, w, h;
    bool deep = false;
    bool contains(int tx, int ty) const { return tx >= x && tx < x + w && ty >= y && ty < y + h; }
};

struct Prop { int tx, ty, size; };       // trees and boulders

// The whole map of Kepler-X: 100 x 70 tiles of 16 px.
// Builds itself deterministically and answers collision, room, door and zone questions.
class World {
public:
    static constexpr int W = 100, H = 70, TILE = 16;
    static constexpr int BASE_TX = 40, BASE_TY = 26;         // building origin in tiles
    static constexpr int BASE_X = BASE_TX * TILE, BASE_Y = BASE_TY * TILE;
    static constexpr int OLD_OX = 480, OLD_OY = 370;          // shifts the original base art coordinates into the world

    World();
    void generate();

    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
    Tile tile(int x, int y) const { return inBounds(x, y) ? tiles_[idx(x, y)] : Tile::Rock; }
    bool solidTile(int x, int y) const;
    const std::string& solidName(int x, int y) const;
    void setSolidName(int x, int y, const std::string& n);
    void clearSolid(int x, int y) { setSolidName(x, y, ""); }

    std::string roomAt(int tx, int ty) const;               // "out" when outside the building
    bool passEdge(IVec a, IVec b) const;                     // false if a wall without a door separates them
    bool inCave(int tx, int ty) const { return tx < 32 && ty >= 33; }
    bool inWind(int tx, int ty) const;
    const Zone& zoneAt(int tx, int ty) const;

    const std::vector<Room>& rooms() const { return rooms_; }
    const Room* room(const std::string& key) const;
    const std::vector<Door>& doors() const { return doors_; }
    const std::vector<Prop>& trees() const { return trees_; }
    const std::vector<Prop>& boulders() const { return boulders_; }
    const std::vector<Zone>& zones() const { return zones_; }
    const std::vector<IVec>& lampPosts() const { return lamps_; }   // world pixels
    const std::vector<IVec>& wreckage() const { return wreckage_; }

    // Fixed story locations.
    static constexpr IVec HANA_TRAP{80, 63};
    static constexpr IVec ANTENNA{51, 4};
    static constexpr IVec LEAK{46, 26};
    static constexpr IVec GENERATOR{68, 52};
    static constexpr IVec PAD_TILE{32, 36};
    static constexpr IVec POD{36, 31};             // Hour 20 supply pod (contact at Hour 8)
    static constexpr IVec REACTOR{94, 64};         // T13
    static constexpr IVec BEACON_SITE{86, 10};     // T15
    static constexpr IVec SHUTTLE_PAD{86, 19};     // where the shuttle lands
    const std::vector<IVec>& debris() const { return debris_; }
    const std::vector<IVec>& bracePoints() const { return braces_; }
    const std::vector<IVec>& reactorHatch() const { return hatch_; }
    const std::vector<IVec>& padDebris() const { return padDebris_; }
    const std::vector<IVec>& lightMounts() const { return mounts_; }
    bool inReactorRoom(int tx, int ty) const { return tx >= 90 && tx <= 95 && ty >= 59 && ty <= 65; }
    void openGate();
    bool gateOpen() const { return tile(76, 19) != Tile::Gate; }

private:
    int idx(int x, int y) const { return y * W + x; }
    void set(int x, int y, Tile t);
    void rect(int x, int y, int w, int h, Tile t);
    void ellipse(int cx, int cy, int rx, int ry, Tile t);
    void line(int x0, int y0, int x1, int y1, int w, Tile t);
    void markRect(int x, int y, int w, int h, const std::string& name);
    void markTile(int tx, int ty, const std::string& name);

    std::vector<Tile> tiles_;
    std::vector<std::string> solids_;
    std::vector<Room> rooms_;
    std::vector<Door> doors_;
    std::set<std::string> doorEdges_;
    std::vector<Zone> zones_;
    Zone wilds_;
    std::vector<Prop> trees_, boulders_;
    std::vector<IVec> lamps_, wreckage_, debris_, braces_, hatch_, padDebris_, mounts_;
    static const std::string kNone;
};
