# My_GD

A Geometry Dash–like rhythm platformer in C with CSFML. Fan project, not affiliated with RobTop Games.

> **Status: playable rewrite.** The deterministic simulation of `PLAN.md` runs the game: Steps 1 to 8 of its Appendix H are in (geometry, loader, the tick with slopes, steps and the jump zone, portals and corridors, the game layer with death, respawn, progress and the F3 overlay, then the bot). Of `FEATURES.md`, the input (1: a thread polls the jump buttons every millisecond, and a press counts in the 240 Hz tick it happened in, whatever the frame rate) the settings store (2: `save/settings.txt`), the widget toolkit (3), music selection (4: a library in `music/`, per-level songs and overrides, sync) the options screen (5), the gamemode framework (6: one table row and one file per mode, gravity portals), the UFO (7), the wave with its trail (8) the ball (9, with tests that play every corridor mode upside down) jump pads and orbs (10.1, 10.2), speed portals (10.3), mini (10.4), the robot, the swing and the spider (10.5), saws (10.6) and the level editor (11: open a level or make a new one, place, delete, save, select, move, copy, undo, turn, resize, start positions, playtest, the song and its beat lines, verify with the bot, warnings, autosave) are in; the rest is next. Anything marked *planned* below doesn't exist yet.

## What it is

The player runs right at a constant speed and only ever presses one button. Blocks and slopes carry it, spikes and saws kill it, portals change what it is (cube, ship, UFO, wave, ball, robot, swing, spider) or how fast it goes, pads and orbs throw it. Everything else is level design.

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
| Previous / next start position (no best and no completion from one) | `A` / `E` |
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

## Level editor

The third button of the main menu opens it: a list of the levels to open, and a field to name a new one (it takes the next free id).

Two tools, `Tab` switches between them: **Place** and **Select**.

| Input | Action |
|---|---|
| Left click, or drag (Place) | Place the palette's entry in the cell under the mouse (and drop the selection) |
| Left click (Select) | Select the object; with Shift, add or remove it |
| Left drag from an object (Select) | Move the selection, by grid steps |
| Left drag from nothing (Select) | A box: selects what it touches; with Shift, adds it |
| Right click, or drag | Delete what is under the mouse |
| Arrows / Shift+arrows | Move the selection a grid step / one pixel |
| Z, Q, S, D | Move the selection a whole block (100 px) up, left, down, right |
| R / Shift+R | Turn the selection 90 degrees (several objects turn as one piece) |
| `[` / `]` | Turn it 15 degrees |
| Delete or Backspace | Delete the selection |
| Ctrl+A | Select everything |
| Ctrl+C / Ctrl+V | Copy the selection / paste it at the mouse |
| Ctrl+D | Duplicate the selection one grid step to the right |
| Ctrl+Z / Ctrl+Y or Ctrl+Shift+Z | Undo / redo, 256 steps |
| 1 to 9, Up / Down with nothing selected, or a click on the palette | Choose what to place |
| Wheel / Shift+wheel | Scroll along the level / up and down |
| Ctrl+wheel | Zoom around the mouse, 25% to 400% |
| Middle button, or Space and drag | Pan |
| G | Grid 50 / 25 |
| L | The level's panel: its song, offset, BPM and first beat (Shift+click: bigger steps) |
| P | Hear the level's song from the middle of the canvas; again to stop |
| Enter | Playtest from the level's start |
| Shift+Enter | Playtest from a start position: the selected one, or the last one before the middle of the canvas |
| V | Verify: the bot looks for a way through the level (Escape stops it) |
| N, or a click on the strip at the bottom | Go to the next object that looks wrong |
| Ctrl+S | Save |
| Escape | Cancel a drag, then drop the selection, then leave (asks to save, discard or cancel if there are unsaved changes) |

Holding a move key, Ctrl+Z or Ctrl+Y repeats it.

With something selected, a panel in the canvas's top right corner shows its properties, each with a minus and a plus: turn (15 degrees), width and height (grid units), kind (a portal's mode, a pad's or an orb's colour, a gravity portal's direction). A saw stays round: its width and height change together. A speed portal has a speed row instead of a kind.

**Start positions** are the palette's last entry. They are other places to begin an attempt from, for practice: the level's real start doesn't change, and they aren't obstacles. Selected, the panel sets their gamemode, gravity and speed. In the game and in a playtest, **A** and **E** go to the previous and the next one (the level's start is the first of the list) and begin an attempt there. An attempt from a start position still adds to the attempt counter, but records no best and no completion. A short `<1/2>` shows which one it is.

With a BPM set in the level's panel, the canvas shows a line at every beat of the song, every fourth one brighter. While the song plays (P), a pink line shows where it is in the level and the canvas follows it.

A **playtest** plays the level as it is, saved or not, and records nothing. Escape or a death bring the editor back where the player was (the level's end doesn't stop it: the player runs on), with the path of that attempt drawn as a line until the next edit.

The white line is the ground, the green one the level's end, the red one the kill ceiling of a flipped player. A save rewrites the file: objects sorted by position, comments and blank lines gone.

**Verify** (V) runs the bot on the level as it is. It answers "found a path", "found no path: furthest 58%" or "undecided", and draws its run as a blue line; with no path, a red cross marks where it got no further and the canvas goes there. It is information only: the bot's search is coarser than a player's and can miss a path, so nothing is ever blocked by it.

The **strip** along the bottom of the canvas counts what looks wrong: warnings (the same object twice in the same place, objects below the ground or behind the start, a face steeper than 50 degrees that looks up) and hints (a spike inside a block, a last portal that isn't a cube portal). It also shows the level's length against its song's, and the bot's last answer.

The editor **autosaves** to `save/editor_autosave/` every minute while there are unsaved changes, before each playtest, and when the window is closed. It never writes over the level's file. Opening a level that has a newer autosave asks whether to restore it.

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
pad 4500 750 2 yellow         # jump pad: launches on touch
orb 5000 500 2 pink           # jump orb: launches on a click while touching it
saw 5500 750 2                # saw: a disc that kills on any contact
speed 6000 650 2 3            # speed portal: 0.5, 1, 2, 3 or 4
```

- **Header**: any line whose first word isn't an object type, written `key <value>`. `name`, `author`, `music`, `music_offset`, `bpm` and `first_beat` describe the level; the optional `start_gamemode`, `start_speed`, `start_size`, `start_gravity`, `start_x` and `start_y` say where and how an attempt begins. Unknown keys are ignored with a warning, so the file still loads.
- **Body**: `type x y size [word] [key=value ...]`, with types `block`, `slope`, `spike`, `saw` (a disc, 0.42 of its width in radius), `portal` (whose word is the gamemode: `cube`, `ship`, `ufo`, `wave`, `ball`, `robot`, `swing`, `spider`) `gravity` (a gravity portal, whose word is `up` or `down`), `start` (another place to begin an attempt for practice, `start x y mode [up|down] [speed] [mini]`: no size, no hitbox, and the real start stays the header's), `mini` (a size portal, whose word is `on` or `off`: half the size, and GD's smaller jumps), `speed` (a speed portal, whose word is one of GD's five speeds by name: `0.5`, `1`, `2`, `3` or `4`, which are 837, 1039, 1291, 1560 and 1920 px/s, not multiples), `pad` (a jump pad: `yellow`, `pink`, `red` or `blue`) and `orb` (a jump orb, clicked while touching it: `yellow`, `pink`, `red`, `blue`, `green` or `black`).
- `x` and `y` are the object's top-left corner in world pixels, `y` grows downward and the ground's surface is at `y = 850`.
- `size` is in grid units of 50 px, so `size 2` is the 100 x 100 block that matches the player. `w=` and `h=` override it per axis, `rot=` turns the object by any angle.
- Sizes must be at least 1: a zero or negative size rejects the line, naming the file and the line.
- `x`, `y` and the object's width and height must stay within 10 000 000 px (over two hours of level): beyond that the line is rejected the same way.
- **Your progress is never written into a level file.** Attempts and bests live in `save/progress.txt`, keyed by the level's id, so editing or sharing a level never touches your records. A level gets a line there once you've played it, and a best below 100% is never saved as 100.

The full grammar is in `PLAN.md` 7.2 and `FEATURES.md` 12.

## Layout

```text
include/        headers (include/sim/ for the simulation)
src/            game layer (window, scenes, menus, rendering)
src/sim/        the simulation: no SFML, no globals, deterministic
src/ui/         the widget toolkit's core: no SFML either, tested the same way
src/fx/         effects that only read the simulation (the wave's trail): no SFML, tested
src/editor/     the level the editor edits (ids, open, save, hit test, undo, selection): no SFML, tested
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

The constants are Geometry Dash's own, converted to our scale (1 GD block = the player = 100 px): normal speed 1038.6 px/s, cube jump 1.9522 velocity units (2027.6 px/s) against 0.876 acceleration units of gravity, corridors 1000 px tall for the ship, UFO, wave and swing 900 for the spider and 800 for the ball. The robot rises at the normal scroll speed for as long as the button is held, 0.283 s at most (349 px with the fall that follows), then falls like a cube. The swing falls like the ship, and each press turns its gravity round. A press takes the spider straight to the surface above it, within the tick. `FEATURES.md` 6.9 lists them all with their sources.

## License

The code is under the MIT license: see `LICENSE`.

Geometry Dash is © RobTop Games. This is an independent fan reimplementation of its mechanics, not affiliated with or endorsed by RobTop Games.
