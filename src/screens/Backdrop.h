#pragma once
#include "raylib.h"

// Space scenery shared by the title, intro, finale and end screens: stars, the purple gas giant, HALCYON,
// the ARK-7 lander, the rescue shuttle and storm effects.
class Backdrop {
public:
    static void stars(float t);
    static void parallaxStars(float t, float speedX, float speedY);   // three depth layers drifting
    static void gasGiant(int cx, int cy, int r, float t);
    static void station(int x, int y, float t);
    static void bigStation(float cx, float cy, float t);               // close-up HALCYON with a spinning ring
    static void lander(float x, float y, float angle, bool fire, float t);
    static void shuttle(float x, float y, float flame, float t);         // rescue shuttle seen from the side, nose up
    static void lightning(float x, float y0, float y1, unsigned seed, Color c);
    static void moonHorizon();
    static void letterbox(float amount);                                 // cinematic black bars, 0..1
};
