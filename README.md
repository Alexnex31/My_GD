# My_GD

A Geometry Dash–like rhythm platformer in C with CSFML. Fan project, not affiliated with RobTop Games.

> **Status: mid-rewrite.** The deterministic simulation described in `PLAN.md` is being written phase by phase: its geometry and level loader are in, the tick is next. The old game code is still what `./my_gd` builds, but it reads the previous level format, so **it can't open the levels in this repository any more**; it gets replaced by the new engine's game layer at Step 7 of `PLAN.md` Appendix H. Anything marked *planned* below doesn't exist yet.

## What it is

The player runs right at a constant speed and only ever presses one button. Blocks and slopes carry it, spikes kill it, portals change what it is (cube, ship, and later UFO, wave, ball). Everything else is level design.

Under the hood it's split in two:

- **the simulation** (`src/sim/`, planned): pure C, no SFML, a fixed 240 Hz timestep, `double` everywhere, no randomness. The same level plus the same inputs gives the same run on any machine, which is what makes replays, checkpoints and the test bot possible;
- **the game layer**: window, rendering, audio, menus, input. It reads the simulation and draws it, and never decides anything about the physics.

The physics is a *semi*-physics engine: one body moves, it has a velocity, forces change that velocity, and its shapes are swept through the level every tick. The player has a rigid square (horizontal faces, spikes, portals), an inscribed circle (slopes and ceilings) and a small inner box that kills on contact. `PLAN.md` section 3 and 4 describe all of it precisely.

## Build

Requires a C compiler, `make` and **CSFML 2.6** (graphics, window, audio, system).

| System | Install |
|---|---|
| Arch | `pacman -S csfml` |
| Debian / Ubuntu | `apt install libcsfml-dev` |
| From source | build SFML 2.6, then CSFML 2.6 against it (`/usr/local` works: the Makefile finds it) |

```sh
make              # release build -> ./my_gd
make debug        # -g3 -O0 with ASan and UBSan -> ./my_gd_debug
make test         # build and run the unit tests (planned, sim only: no CSFML needed)
make fuzz_parser  # level-parser fuzzer, needs clang (planned)
make re           # rebuild from scratch
```

Objects and dependency files go to `build/release` or `build/debug`, so the two builds never mix.

## Run

```sh
./my_gd           # the game
./my_gd -h        # usage
```

The game finds its `res/` and `levels/` folders next to the executable, so it can be started from anywhere.

Exit code `84` means a missing asset, an unreadable level or a bad argument, with a message saying which.

## Controls

| Action | Key |
|---|---|
| Jump / hold | `Space`, `Up`, or left click |
| Pause | `Escape` |
| Restart the attempt | `R` *(planned)* |
| Debug overlay (hitboxes, contacts, tick, speeds) | `F3` *(planned)* |

Rebindable keys, a gamepad and the options screen are planned (`FEATURES.md` 2 and 5).

## Levels

A level is a text file in `levels/`, named after its **id**: digits only, with a `.gd` extension (`levels/10280.gd`). Adding a level is dropping such a file there.

```text
name Stereo Madness          # the header: what the level is
author Alexnex
version 2

# the body: one object per line
block 1000 200 1
spike 3400 0 2 rot=180       # ceiling spike
block 5000 650 2 w=8 h=1     # 400 x 50 platform
slope 6000 750 2             # 100 x 100, 45 deg, rising to the right
portal 2100 750 2 ship
```

- **Header**: any line whose first word isn't an object type. `name`, `author`, `version`, and later `song`, `offset`, `bpm`, `first_beat`. Unknown keys are ignored with a warning.
- **Body**: `type x y size [word] [key=value ...]`, with types `block`, `slope`, `spike` and `portal` (whose word is the gamemode: `cube`, `ship`).
- `x` and `y` are the object's top-left corner in world pixels, `y` grows downward and the ground's surface is at `y = 850`.
- `size` is in grid units of 50 px, so `size 2` is the 100 x 100 block that matches the player. `w=` and `h=` override it per axis, `rot=` turns the object by any angle.
- Sizes must be at least 1: a zero or negative size rejects the line, naming the file and the line.
- **Your progress is never written into a level file.** Attempts and bests live in `save/progress.txt`, keyed by the level's id, so editing or sharing a level never touches your records.

The full grammar, including the planned `pad`, `orb`, `saw`, `speed`, `mini` and `gravity` objects, is in `PLAN.md` 7.2 and `FEATURES.md` 12.

## Layout

```text
include/        headers (include/sim/ for the simulation)
src/            game layer (window, scenes, menus, rendering)
src/sim/        the simulation: no SFML, no globals, deterministic   (planned)
levels/         level files
res/            textures, fonts, sounds
tests/          unit tests, engine tests, the bot                     (planned)
PLAN.md         the rewrite: architecture, engine, phases
FEATURES.md     gamemodes, objects, editor, music, options, tests
```

## Tests

`make test` builds a test binary that links **only** the simulation, so it needs no window and no CSFML. It covers the collision cases engines usually get wrong (seams between blocks, exact gaps, containment, tunneling at high speed, slopes, step-ups, the jump zone), pins the physics constants, and checks that the same inputs give the same state hash twice. A bot walks each level with the real engine and reports whether it found a way through: information, never a build failure. Details in `PLAN.md` 8.

## Physics numbers

The constants are Geometry Dash's own, converted to our scale (1 GD block = the player = 100 px): normal speed 1038.6 px/s, cube jump 1.9522 velocity units (2027.6 px/s) against 0.876 acceleration units of gravity, corridors 1000 px tall for the ship, UFO and wave and 800 for the ball. `FEATURES.md` 6.9 lists them all with their sources.

## License

The code is under the MIT license: see `LICENSE`.

Geometry Dash is © RobTop Games. This is an independent fan reimplementation of its mechanics, not affiliated with or endorsed by RobTop Games.
