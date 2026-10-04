# STRANDED: 72 Hours

A single-player 2D survival-management game in C++17 with raylib. Your lander has crashed on the frozen
moon Kepler-X. Restore power, keep the crew alive, find Hana, reach the HALCYON station during a radio
window, then hold the colony together through a storm until the rescue shuttle lands at Hour 72.
The full game is playable: four acts, seven endings and a Truth Epilogue.

## The four acts

| Act | Hours | What happens |
|---|---|---|
| 1 - Dark | 0-6 | Restore the Power Core, seal the leak, find Hana in the Wreck Field |
| 2 - The Signal | 6-24 | Build the transmitter, set up the antenna on the Ridge, reach HALCYON at Hour 8, 16 or 24 |
| 3 - Holding On | 24-60 | Ice purifier, greenhouse, reactor hatch and coil, status reports at Hours 32 and 56, the Hour 48 storm (braces, tarp, batteries or reactor) |
| 4 - The Light | 60-72 | The landing gate opens: clear the pad, place six landing lights, build and power the beacon, guide the shuttle down |

Endings: **F1** Signal Lost, **F2** Lost on the Ice, **F3** Colony Collapse, **F4** The Shuttle Turns Back,
**E1** Together, **E2** The Empty Seats, **E3** The Lone Commander. Find the flight recorder (deep in the
Ice Cave) and keep Hana alive, or also find the flight log, to unlock the Truth Epilogue. Ask Hana to plot
the descent, or guide the shuttle yourself at Hour 70, or the landing is rough. The title screen keeps
your collection.

## How to play

| Key | Action |
|---|---|
| Arrow keys / WASD | Walk (a quick tap turns on the spot) |
| Z | Talk, pick up, use, repair |
| Hold Z | Set up the antenna, place braces and landing lights, use the plasma cutter |
| X | Back, cancel |
| Enter | Menu (stops the clock): Journal, panels, settings, act tracker |
| Mouse | Optional: click a tile to walk there, click an object to use it |
| F11 | Fullscreen |

- The **Next Goal** box and the **amber arrow** always point at your current task. The **Journal** lists every task and the story so far.
- Every running system uses one **power unit**, and there are never enough units.
- Outside the base, every step and every zone you enter costs **suit oxygen**. Get back inside before it runs out.
- Walk up to crew members and press Z to hand them jobs. Trusting them leads to the best ending.
- Repairs are short puzzles (Wire, Pipe, Sequence, Tune, Cut). Failing one costs 15 minutes, never the run,
  and after two failures the crew talks you through it (one step easier, 50% more time).
- When a build is short of scrap or cables, the arrow points at the nearest pickup.
- The Storage water barrels hold one emergency refill (+25 water) to bridge the gap until the purifier.
- Meet the Hour 56 status report and the Landing Zone gate opens early. The act panel shows how the colony
  is doing against HALCYON's target, and Maya warns four hours before each report.
- Emergencies take over the Next Goal box: low suit air, or a critical bar you can fix right now (the
  Power Panel button flashes and the goal names the system to switch on).
- The game autosaves every 4 game hours when Kiran is safely inside; Continue and Load last save use it.
- Watch **Kiran's health**: radiation in the reactor room, deep cold without the thermal layer and storm debris all hurt. Medical heals him at the base.
- Unused power units charge the **battery**. 70% or more when the storm hits keeps the lights on.

## Build

Requires the 64-bit MinGW-w64 GCC in `C:\mingw64` (GCC 15, with `cmake` and `mingw32-make`).
raylib 5.5 is already built and included in `external/raylib`.

```
build.bat            build the game into build\STRANDED.exe
build.bat run        build and play
build.bat test       build and run the logic self-test (no window)
build.bat debug      debug build with a console
build.bat release    package dist\STRANDED-72-Hours.zip (exe + data, no installs needed)
```

In VS Code: `Ctrl+Shift+B` builds, `F5` builds and runs.

`build\STRANDED.exe --smoke` walks through every screen automatically and saves screenshots to `build\shots`.

`build\playtest.exe` plays the whole game through the real rules with a bot
that follows the Next Goal arrow, walks every step, takes human-like time on puzzles and dialogue, and aims
at each ending in turn. Pass a scenario number (0-11) to play just one run.

## Project layout

```
src/logic     the rules: no drawing, no raylib (testable on its own). Session is split by job:
              Session (update, movement), SessionQuests (goals), SessionInteract (objects),
              SessionCrew (power and crew jobs), SessionStory (hourly beats, endings), SessionSave
src/core      Game, Screen base class, input, synthesized audio, colours
src/gfx       pixel font, UI kit, sprites and icons
src/render    world renderer, light map, HUD, weather (storm), particles
src/ui        panels, puzzles, dialogue box, menus
src/screens   Title, Intro (cinematic), How to play, Play, Finale, End
data          balance.txt, items.txt, dialogue.txt (edit to tune; no code change)
tests         selftest.cpp (logic checks and a full playthrough to the E1 ending)
```

## Classes and the OOP concept each one shows

| Class | Job | Concept |
|---|---|---|
| `Game` | Owns the window, canvas, services and the current screen | Composition |
| `Screen` → `TitleScreen`, `IntroScreen`, `HowToScreen`, `PlayScreen`, `FinaleScreen`, `EndScreen` | One per screen; `update()` and `draw()` are pure virtual | Abstraction, inheritance |
| `System` → `OxygenSystem`, `MedicalSystem`, `HeaterSystem`, `CommsSystem`, `GreenhouseSystem`, `PurifierSystem`, `BeaconSystem` | Each refills its bar in its own way through `applyHour()` | Runtime polymorphism |
| `Puzzle` → `WirePuzzle`, `PipePuzzle`, `TunePuzzle`, `SequencePuzzle`, `CutPuzzle` | Repair mini-games; the play screen runs any of them through the base interface | Polymorphism, factory |
| `Particles` | Pooled sparks, smoke, exhaust and debris for the cinematics; never reallocates | Encapsulation, RAII |
| `AudioBank` | Every sound effect and the three music loops are synthesized at start | Encapsulation, RAII |
| `Panel` → `PowerPanel`, `CrewPanel`, `MapPanel`, `JournalPanel`, `CargoPanel` | `draw()` is a template method; children fill `drawContent()` | Template method, inheritance |
| `Colony` | Bars, power budget, systems, crew; power only changes through `switchOn`/`switchOff` | Aggregation, encapsulation |
| `CrewMember` | Name, skill, health, state, job | Encapsulation |
| `Resource` | One colony bar; `+=` refills, `-=` drains, clamped 0 to 100 | Operator overloading |
| `GameClock` | Hours, signal windows, countdowns; windows are static constants | Static members |
| `Inventory<T>` | Carried slots; heavy parts take two | Templates |
| `PowerException`, `DataError` | Thrown when a system cannot run or a data file is bad; caught by the panel / game | Exceptions |
| `SaveManager`, `SaveData`, `DataStore` | Act saves, endings across runs, balance/items/dialogue files | File handling |
| `Session` | One run: world, colony, player, story and every rule; talks to the display only through events | Separation of logic and drawing |

## Notes

- Saves are written at the start of each act (`save.txt`); unlocked endings go to `progress.txt`.
  The smoke test writes its own saves into `build\shots` instead.
- The default clock runs 2x (30 s per game hour); 1x (the documented 60 s per hour, "relaxed") is in the
  Enter menu, and the default lives in `data/balance.txt`. Automated playtests showed that at 4x even a
  careful player cannot reach HALCYON in time, so 4x was removed. Radio windows stay open for 2 hours.
- Suit air: each step and each move into a deeper zone costs air; heading back to a shallower zone is
  free; standing still outside costs 2 per game hour. Waiting at the antenna keeps enough air to walk home.
