#pragma once
#include "logic/Types.h"
#include "raylib.h"
#include <map>
#include <string>

// Builds the character and tree textures from small text pixel maps at start (no image files).
// Every character sprite gets a 1 px dark outline, the GBA look the design asks for.
class SpriteBank {
public:
    void build();
    void unload();
    // feetX/feetY: bottom centre of the sprite in world or screen pixels.
    void drawCharacter(const std::string& who, float feetX, float feetY, Dir facing, bool stride, bool flip) const;
    void drawPine(int size, float baseX, float baseY) const;

private:
    Texture2D makeCharacter(const std::string& who, Dir facing, bool stride, bool flip) const;
    std::map<std::string, Texture2D> chars_;
    std::map<int, Texture2D> pines_;
};

// Small icons drawn straight from rectangles: items, bar icons, menu icons, portraits.
class Icons {
public:
    static void item(const std::string& name, float x, float y);
    static void bar(int barIndex, float x, float y);         // 0 oxygen, 1 warmth, 2 water, 3 food
    static void menu(const std::string& name, float x, float y, Color c);   // bolt, crew, pin, book
    static void portrait(const std::string& who, float x, float y);   // 28x28
};
