// STRANDED: 72 Hours
// A single-player 2D survival-management game in C++ with raylib.
// Entry point: create the Game and run the main loop (update, then draw, every frame).
//   STRANDED.exe            play the game
//   STRANDED.exe --smoke    automated walk through every screen, screenshots in build/shots
#include "core/Game.h"
#include <string>

int main(int argc, char** argv) {
    Game game;
    if (argc > 1 && std::string(argv[1]) == "--smoke") return game.smokeTest(argc > 2 ? argv[2] : "build/shots");
    game.run();
    return 0;
}
