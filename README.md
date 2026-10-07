# My_GD

A Geometry Dash–like rhythm platformer in C with CSFML. Fan project, not affiliated with RobTop Games.

> **Status: playable rewrite.** The deterministic simulation of `PLAN.md` runs the game: Steps 1 to 8 of its Appendix H are in (geometry, loader, the tick with slopes, steps and the jump zone, portals and corridors, the game layer with death, respawn, progress and the F3 overlay, then the bot). Of `FEATURES.md`, the input (1: a thread polls the jump buttons every millisecond, and a press counts in the 240 Hz tick it happened in, whatever the frame rate) the settings store (2: `save/settings.txt`), the widget toolkit (3), music selection (4: a library in `music/`, per-level songs and overrides, sync) the options screen (5), the gamemode framework (6: one table row and one file per mode, gravity portals), the UFO (7), the wave with its trail (8) and the ball (9, with tests that play every corridor mode upside down) are in; the rest is next. Anything marked *planned* below doesn't exist yet.

## What it is

The player runs right at a constant speed and only ever presses one button. Blocks and slopes carry it, spikes kill it, portals change what it is (cube, ship, UFO, wave, ball). Everything else is level design.

Under the hood it's split in two:

- **the simulation** (`src/sim/`): pure C, no SFML, a fixed 240 Hz timestep, `double` everywhere, no randomness. The same level plus the same inputs gives the same run on any machine, which is what makes replays, checkpoints and the test bot possible;
- **the game layer**: window, rendering, audio, menus, input. It reads the simulation and draws it, and never decides anything about the physics.

The physics is a *semi*-physics engine: one body moves, it has a velocity, forces change that velocity, and its shapes are swept through the level every tick. The player has a rigid square (horizontal faces, spikes, portals), an inscribed circle (slopes and ceilings) and a small inner box that kills on contact. `PLAN.md` section 3 and 4 describe all of it precisely.

## Build

Requires a C compiler, `make`, **CSFML 2.6** (graphics, window, audio, system) and the Xlib headers (`libx11`, which SFML itself needs on Linux: the input thread calls `XInitThreads`).

| System | Install |
|---|---|
| Arch | `pacman -S csfml` |
| Debian / Ubuntu | `apt install libcsfml-dev libx11-dev` |
| From source | build SFML 2.6, then CSFML 2.6 against it (`/usr/local` works: the Makefile finds it) |

```sh
make              # release build -> ./my_gd
make debug        # -g3 -O0 with ASan and UBSan -> ./my_gd_debug
make test         # build and run the unit tests (sim and toolkit core: no CSFML needed)
make fuzz_parser  # level-parser fuzzer, needs clang
make re           # rebuild from scratch
```

Objects and dependency files go to `build/release` or `build/debug`, so the two builds never mix.

## Run

```sh
./my_gd           # the game
./my_gd -h        # usage
./my_gd --check levels/1.gd   # load a level without a window and report on it
```

The game finds its `res/`, `levels/` and `music/` folders next to the executable, so it can be started from anywhere.

Exit code `84` means a missing asset, an unreadable level or a bad argument, with a message saying which. `--check` exits `1` when the level can't be read or has invalid lines (object lines it had to skip), `0` otherwise: ignored fields and unknown header keys are only warnings. Its path is relative to where you run it.

## Controls

| Action | Key |
|---|---|
| Jump / hold | `Space`, `Up`, or left click (`jump_bindings`) |
| Back to the level list | `Escape` |
| Restart the attempt | `R` (`restart_key`) |
| Debug overlay (hitboxes, contacts, tick, speeds) | `F3` |
| Pause | *(planned)* |

The keys are rebindable in `save/settings.txt`, written with the defaults the first time the game quits. Every key is `key=value`, one per line:

```text
music_volume=80
fullscreen=0
window_width=1280
window_height=720
vsync=1
fps_limit=0              # 0, 60, 120, 144 or 240; ignored while vsync is on
show_percent=1
show_progress_bar=1
show_attempts=1
jump_bindings=Space,Up,MouseLeft,Joy0
restart_key=R
```

- **Key names** are SFML's: `A`–`Z`, `Num0`–`Num9`, `Space`, `Up`, `LShift`, `F1`–`F15`, `Numpad0`–`Numpad9` and the rest of `sfKeyCode`; `MouseLeft`, `MouseRight`, `MouseMiddle`; `Joy0`–`Joy15`. Case doesn't matter. Up to 6 jump inputs.
- An empty `restart_key=` unbinds it. `Escape` can't be bound, and a key can't both jump and restart.
- An invalid value keeps its default, with a warning naming the line. Unknown keys are kept and written back; comments aren't.
- If the file exists but can't be read, the game uses the defaults and never overwrites it.
- `sfx_volume`, `menu_song`, `checkpoint_key`, `remove_checkpoint_key` and `audio_offset_ms` are read and kept for the features that will use them. Gamepad buttons are read but not polled yet.

The full list is in `FEATURES.md` 2.2. The **options screen** (the main menu's options button) edits them, with the keyboard or the mouse:

- **Sections:** Audio, Gameplay, Controls, Display and Data, switched with `Tab` / `Shift+Tab` or a click. `Up`/`Down` move through the rows, `Left`/`Right` change a value, `Enter` activates, `Escape` goes back and saves what changed.
- **Controls:** activate a row, then press any key, mouse button or gamepad button. `Escape` cancels, `Backspace` clears. An input another row has swaps the two rows, and the last jump input can't be removed.
- **Display:** a new fullscreen setting or window size asks to be kept, and reverts by itself after 10 s, so an unsupported mode fixes itself.
- **Data:** Reset progress asks first, and keeps the old file as `save/progress.txt.bak`. Open save folder opens `save/` and prints its path.

## Music

Songs live in `music/` (`.ogg` recommended; `.wav`, `.flac` and `.mp3` work too), described in `music/songs.txt`:

```text
# file | title | artist | license | default_offset_seconds
stereo_sunrise.ogg | Stereo Sunrise | Some Artist | CC BY 4.0 | 0.00
```

- A level picks its song with `music <file>` and `music_offset <seconds>` in its header; without one, it plays `back_mus.ogg`.
- In the level list, click a level's "Song:" line to choose your own song for it (saved with your progress). Moving through the list plays 10 s of each song.
- The menus play `menu_song` (`settings.txt`, default `menu_loop.ogg`), or the library's first song if it's missing, and pick up where they left it after a level.
- Song file names can't contain spaces or any of `#|=`. A file that doesn't decode is skipped with a message.
- Keep the license of every song you ship in `songs.txt`: the end screen credits title, artist and license.

`./my_gd --check` prints which song a level plays and warns when the level is longer than it (the song then loops). Details in `FEATURES.md` 4.

## Levels

A level is a text file in `levels/`, named after its **id**: digits only, with a `.gd` extension (`levels/10280.gd`). Adding a level is dropping such a file there.

```text
name Stereo Madness          # the header: what the level is
author Alexnex
music stereo_madness.ogg
start_gamemode ship          # optional: where and how the attempt starts

# the body: one object per line
block 1000 200 1
spike 3400 0 2 rot=180       # ceiling spike
block 5000 650 2 w=8 h=1     # 400 x 50 platform
slope 6000 750 2             # 100 x 100, 45 deg, rising to the right
portal 2100 750 2 ship
gravity 4000 750 2 up         # gravity portal: the player falls upward
```

- **Header**: any line whose first word isn't an object type, written `key <value>`. `name`, `author`, `music`, `music_offset`, `bpm` and `first_beat` describe the level; the optional `start_gamemode`, `start_speed`, `start_size`, `start_gravity`, `start_x` and `start_y` say where and how an attempt begins. Unknown keys are ignored with a warning, so the file still loads.
- **Body**: `type x y size [word] [key=value ...]`, with types `block`, `slope`, `spike`, `portal` (whose word is the gamemode: `cube`, `ship`, `ufo`, `wave`, `ball`) and `gravity` (a gravity portal, whose word is `up` or `down`).
- `x` and `y` are the object's top-left corner in world pixels, `y` grows downward and the ground's surface is at `y = 850`.
- `size` is in grid units of 50 px, so `size 2` is the 100 x 100 block that matches the player. `w=` and `h=` override it per axis, `rot=` turns the object by any angle.
- Sizes must be at least 1: a zero or negative size rejects the line, naming the file and the line.
- `x`, `y` and the object's width and height must stay within 10 000 000 px (over two hours of level): beyond that the line is rejected the same way.
- **Your progress is never written into a level file.** Attempts and bests live in `save/progress.txt`, keyed by the level's id, so editing or sharing a level never touches your records. A level gets a line there once you've played it, and a best below 100% is never saved as 100.

The full grammar, including the planned `pad`, `orb`, `saw`, `speed`, `mini` and `gravity` objects, is in `PLAN.md` 7.2 and `FEATURES.md` 12.

## Layout

```text
include/        headers (include/sim/ for the simulation)
src/            game layer (window, scenes, menus, rendering)
src/sim/        the simulation: no SFML, no globals, deterministic
src/ui/         the widget toolkit's core: no SFML either, tested the same way
src/fx/         effects that only read the simulation (the wave's trail): no SFML, tested
levels/         level files
music/          songs and songs.txt
res/            textures, fonts, sounds
tests/          unit tests, engine tests, the bot
PLAN.md         the rewrite: architecture, engine, phases
FEATURES.md     gamemodes, objects, editor, music, options, tests
```

## Tests

`make test` builds a test binary that links **only** the simulation, so it needs no window and no CSFML. It covers the collision cases engines usually get wrong (seams between blocks, exact gaps, containment, tunneling at high speed, slopes, step-ups, the jump zone), pins the physics constants, checks that the same inputs give the same state hash twice, and that every press lands in the tick it happened in, however frames group the ticks. A bot walks each level with the real engine and reports whether it found a way through: information, never a build failure. `--check` prints the same line. `make fuzz_parser` builds a libFuzzer target that feeds random bytes to the level loader and plays them; CI runs it for 60 s. Details in `PLAN.md` 8.

## Physics numbers

The constants are Geometry Dash's own, converted to our scale (1 GD block = the player = 100 px): normal speed 1038.6 px/s, cube jump 1.9522 velocity units (2027.6 px/s) against 0.876 acceleration units of gravity, corridors 1000 px tall for the ship, UFO and wave and 800 for the ball. `FEATURES.md` 6.9 lists them all with their sources.

## License

The code is under the MIT license: see `LICENSE`.

Geometry Dash is © RobTop Games. This is an independent fan reimplementation of its mechanics, not affiliated with or endorsed by RobTop Games.
