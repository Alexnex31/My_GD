# My_GD — Refactor & Fix Plan (v2)

Reviewed commit: `3f625f2` ("added: ship gamemode and better menus").
Build target: gcc + CSFML 2.6 (the version the code builds against as-is). All API names are CSFML 2.x.

## What changed since v1

v1 was written from reading the code. For v2, the claims were **tested by running your actual game code headless** (Xvfb virtual display, OpenAL null audio driver), plus a working prototype of the new core. Several v1 details turned out wrong, and the fixes are in here:

| v1 said | Measured reality | v2 (v3 changes noted) |
|---|---|---|
| Ship portals leak ~9 boundary lists each | 1 today, because the double-shift bug moves the player out of the portal. Fixing only the double shift makes it **14** | Fix both together (5.1) |
| Ship corridor centered on the portal, pushed above ground | Legacy: ceiling at `portal.y - 400`, floor at `portal.y + 450`, ground still applies | v3: centered corridor from the start (5.2) |
| Player hitbox 100×100 against blocks | Legacy block box is 80×90 (shrunk 10 px left, right and top) | v3: a rigid 100×100 square (flat support, hazards, jump zone), an inscribed circle (slopes, head hits), a 40×40 inner box that makes blocks fatal (4.3) |
| Proportional spike hitbox "fixes other sizes" | It changes level 2's size-1 spike | v3: proportional from the start, play-test level 2 (4.2) |
| Per-second constants × float `dt` | Not bit-identical to legacy, and `x += v * dt` drifts | Per-tick units, `x` from an exact double distance (3.2) |
| 240 Hz: lower jump to 1527 px/s | That fixes height but shortens airtime | **1500 px/s + gravity 4855 px/s²** reproduce both exactly (11.1) |

**v2.1:** portals (and later pads, orbs, gravity and speed portals) follow one standard rule: they have an **activation hitbox**, act on the player **once**, then lose their hitbox for the rest of the attempt while staying visible (5.1). The per-object flag is now `spent` (it was `triggered`). These are called **interactive objects**; the word **trigger** is reserved for GD-style triggers (objects that move other objects, change colors, etc.), which are out of scope for this plan but may come later.

**v3.1**: no rise caps (gravity only accelerates up to a mode's fall limit; the ship's thrust only up to its speed limit; orbs, pads, slopes and mode changes can exceed both); jumps are the same force on slopes as on flat ground; the kill ceiling only exists in flipped gravity; `can_jump` is computed last in the tick; orbs act on exactly the tick of contact; the step-up needs downward momentum.

**v3** (semi-physics engine): the core is a new engine instead of a reproduction of today's code (no legacy-parity step). The player has shapes on one center: a **rigid 100 × 100 square** that stands on horizontal faces, detects harm and interactive objects and carries the **jump zone** (the part below the inner box); an inscribed **circle** (radius 50) that stands on tilted faces and takes every head hit; a rigid **40 × 40 inner box** that kills on any neutral contact; and a rotating **icon** for drawing only. Neutral objects never stop the player horizontally: running into a wall is fatal when it reaches the inner box, and horizontal faces up to 30 px higher are stepped onto when the inner box's leading side passes over them. The player can jump when a neutral hitbox touches the jump zone (the part inside the circle for anything, the square's corners for flat faces only) while its momentum is downward relative to the surface it stands on; there's no jump buffer. Faces are horizontal, vertical or tilted, decided per face at load. The simulation computes in double precision. Forces (gravity, thrust, the jump impulse, pads and orbs) set or change the velocity; each tick the shapes are **swept** through the level and respond to the first contact. Hitboxes are full areas (containment counts), categorized as neutral, harm and interactive. Phases 3–5, 8 and 11 are rewritten; everything about legacy switches, the differential test and replay tests is gone. The simulation runs at 240 Hz from the start.

**v2.2** (review pass):

- The simulation is split into **immutable level data** and a small **run state** (3.3). Checkpoints, bot snapshots and determinism tests become a single copy, and `spent` moves from the objects into a bitset in the run state.
- Every object can be **rotated** by any angle, and blocks can have a separate **width and height** (3.3, 7.2).
- A **kill ceiling**, only while gravity is flipped, a few hundred pixels above the highest object, ends runs that fall upward forever (4.7).
- Collision culling uses how far hitboxes **reach**, not sprite widths (4.1). The previous version skipped level 2's spikes 20 px too early.
- **Build**: separate release/debug folders, `-ffp-contract=off` for cross-machine determinism, all headers in `include/` (1.1, 2.2).
- **Rendering** is built for speed: static vertex arrays per chunk, one texture atlas, a handful of draw calls per frame (9.2).
- The **bot never blocks anything**: an "impossible" result is a warning (8.3).
- **Practice mode**, the **restart key** and **same-mode portals** have exact rules (5.2, 6.5, 13).
- **Environment**: Arch Linux commands next to Ubuntu's (Phase 0, Appendix E).

The verification tools are in **`my_gd_lab.zip`** (delivered with this plan, see Appendix E). They let you re-run every measurement below, and they are the backbone of the testing strategy in Phase 8.

---

## Table of contents

- [Part I: Findings (measured)](#part-i-findings-measured)
- [Part II: Principles](#part-ii-principles)
- [Phase 0: Baseline and safety net](#phase-0-baseline-and-safety-net)
- [Phase 1: Tooling, headers, safe fixes](#phase-1-tooling-headers-safe-fixes)
- [Phase 2: Architecture (simulation vs presentation)](#phase-2-architecture-simulation-vs-presentation)
- [Phase 3: The new core (a semi-physics engine)](#phase-3-the-new-core-a-semi-physics-engine)
- [Phase 4: The collision engine](#phase-4-the-collision-engine)
- [Phase 5: Gamemodes, portals, ship corridor](#phase-5-gamemodes-portals-ship-corridor)
- [Phase 6: Death, respawn, attempts, progress](#phase-6-death-respawn-attempts-progress)
- [Phase 7: Level format, loader, level list](#phase-7-level-format-loader-level-list)
- [Phase 8: Testing and CI](#phase-8-testing-and-ci)
- [Phase 9: Rendering](#phase-9-rendering)
- [Phase 10: Scenes, menus, input, audio](#phase-10-scenes-menus-input-audio)
- [Phase 11: Feel tuning](#phase-11-feel-tuning)
- [Phase 12: Assets, licensing, privacy, repository](#phase-12-assets-licensing-privacy-repository)
- [Phase 13: Further features](#phase-13-further-features)
- [Appendix A: Bug list with evidence](#appendix-a-bug-list-with-evidence)
- [Appendix B: Compiler warnings](#appendix-b-compiler-warnings)
- [Appendix C: Final file layout](#appendix-c-final-file-layout)
- [Appendix D: Test levels](#appendix-d-test-levels)
- [Appendix E: The verification lab](#appendix-e-the-verification-lab)
- [Appendix F: Coordinate spaces and units cheat sheet](#appendix-f-coordinate-spaces-and-units-cheat-sheet)
- [Appendix G: The geometry, formula by formula](#appendix-g-the-geometry-formula-by-formula)
- [Appendix H: Build order, file by file](#appendix-h-build-order-file-by-file)
- [Footnote: a future fork with another graphics library](#footnote-a-future-fork-with-another-graphics-library)

---

## Part I: Findings (measured)

Everything here was produced by the lab tools running your real source files. Numbers are exact outputs, not estimates.

### F1. The physics you have today

| Quantity | Per frame (60 FPS) | Per second |
|---|---|---|
| Scroll speed | 12.5 px | 750 px/s |
| Cube gravity | 1.4 px/frame² | 5040 px/s² |
| Cube jump velocity | 26 px/frame | 1560 px/s |
| Cube max fall speed | 43 px/frame | 2580 px/s |
| Ship gravity | 1.55 px/frame² | 5580 px/s² |
| Ship thrust (while held) | +3.5 px/frame² | 12600 px/s² |
| Ship max speed | 28 px/frame | 1680 px/s |

Measured cube jump from flat ground: **apex 228.6 px**, **airtime 37 frames (0.617 s)**, **~462 px of horizontal travel** (4.6 blocks of 100 px). One block is 100 px (a `size 2` object), so the jump clears two blocks with 28 px to spare. Keep these three numbers in mind: they define your game's feel, and the new engine's constants reproduce them exactly (11.1).

### F2. The first attempt plays differently from every retry

`create_player` spawns at `(400, 750)`, `player_dead` respawns at `(350, 790)`. Measured:

- First attempt: lands after 8 frames, at x = 400.
- Retries: land after 4 frames, at x = 350.

Being 50 px further right means obstacles arrive **4 frames (67 ms) earlier on attempt 1**. On level 3, the jump window for the first spike is frames 104–122 on the first attempt and 108–126 on every retry. Muscle memory built on retries is wrong for attempt 1.

### F3. Ship portals: teleport, leak, and why the fix order matters

Level 7, crossing the ship portal at x = 2100, y = 750:

```text
frame 132: mode c->p  player y 800.0 -> 300.0 (dy -500)   ground y 850.0 -> 600.0 (dy -250)
frame 146: mode p->p  (portal fires again)                  second boundary list allocated
boundary lists allocated for ONE ship portal: 2 (1 leaked)
```

`portal_shift` moves the world by -250 but the player by -500, so relative to the level the player is **teleported 250 px up** (for a portal high on screen, it teleports down instead). That displacement moves the player out of the portal's hitbox, which accidentally limits the leak to one extra allocation.

With the double shift fixed **and nothing else**, the player stays inside the portal for its whole width and the leak happens every frame: **15 lists allocated, 14 leaked, per ship portal crossing**. Fix the double shift and the re-firing together (Phase 5 does both by design).

### F4. You can't jump while inside a cube portal

Every frame the player overlaps a cube portal, `check_portal` calls `load_new_player_texture('c')`, which sets `allow_jump = 'n'`. Measured with a cube portal on the ground and the button held: the cube lands at frame 97 inside the portal but can't jump until frame 103, when it leaves. That's a **6-frame (100 ms) jump lock**.

### F5. Wide platforms drop the player

Blocks are only collision-checked while their left edge is between x = 100 and x = 600 on screen. Measured with a 400 px platform: the player lands on it at frame 45, then falls through at frame 71, when the platform still spans x = 100..500 under the player at x = 400.

This is **latent**: every object in your 7 levels is size 1 or 2 (50 or 100 px), and the bug needs blocks of size 5+ (over 210 px wide). The first long platform you add will hit it.

### F6. Best percentage loses its decimals everywhere

```text
file says best=47.83 -> level loader best=47.00, level list best=47.00
my_str_to_word_array("9 5 47.830000") -> [9] [5] [47] [830000]
```

The tokenizer treats `.` as a separator, so both readers see `47`. Once the level is played and saved, `47.00` is written back and the decimals are gone for good.

### F7. Level list order and wrong-level loading

With four extra copies of `level3` named `level10`..`level13`:

```text
list order: level3 level12 level10 level4 level7 level2 level5 level6 level13 level11 level1
```

`readdir` order is arbitrary. Worse, the list shows files by name but loads by the number in the file's header: `level10` (a copy of level 3, header `3 ...`) is displayed as LEVEL10 and loads `levels/level3`.

### F8. Leaks

A LeakSanitizer run over all 7 levels (3000 frames each, with many deaths) reports exactly one leak source: **portals** (the struct, its sprite, and the pointer array, 3 allocations per portal, never freed by `free_objects`). Everything else is freed correctly. That's a good sign: the ownership discipline in the rest of the code is solid.

### F9. All levels are beatable, and a pure-C core can reproduce the old one exactly

- A search bot running your real physics proves **all 7 levels are completable** (level 6 takes 2599 frames, 43 s; level 7 can be completed without pressing anything, since nothing in it is in the player's path).
- A pure-C prototype of the new core (Phase 3–5 design, with legacy switches on) was compared with your real code on **2,800 input scripts** (the bot's winning inputs, randomly perturbed, 400 per level): **2,800/2,800 identical outcomes** (same death frame or same completion frame), maximum position difference **0.0003 px**.
- (v3 no longer aims for exact reproduction, Principle 2, but this result showed the pure, deterministic design works, and the prototype remains the reference for feel numbers.)
- With **every** planned change switched on (no teleport, no jump lock, proportional spikes, new block resolver, centered corridor, 240 Hz with matched jump arc), the bot still completes **all 7 levels**. The whole bot suite runs in under 0.1 s.

### F10. Build and warnings

The code builds with gcc and CSFML 2.6. `-Wall -Wextra` reports 13 warnings (Appendix B), including two real out-of-bounds reads in `main` and an uninitialized return in `music.c`. `menuLoop.mp3` loads fine with CSFML 2.6 (SFML added MP3 support in 2.6; older 2.5 installs can't read it, and the game would then call `sfMusic_setLoop(NULL, ...)`).

### F11. Assets and privacy

- `cecilya.png` and `noe_background.jpeg` are **photos of real people**, used as the editor and options backgrounds, in a public repository.
- `explode_11.ogg`, `playSound_01.ogg`, `quitSound_01.ogg`, `menuLoop.mp3` match Geometry Dash's own resource file names, and the menu background looks like the game's menu art. If they were extracted from the game, they are RobTop's copyrighted assets.
- Many images are byte-identical placeholders of each other (Phase 12 lists them), and 13 files are never loaded.

---

## Part II: Principles

**1. Objects never move.** Today, every frame moves every block, spike and portal (`move_objects`), vertical following moves them again, `portal_shift` again, and `move_objects_back` undoes it all on death. That design is the root of F3, F5 and most of the 535 lines of `physics.c`. In the new design, positions come from the level file and never change. The player moves; a camera follows.

**2. Feel targets, then tuning.** The new core is a new engine (Phase 3), not a reproduction of today's collision code. What it must keep from today is the **feel**: the same scroll speed, the same jump (height, airtime, length, 11.1), and every existing level still beatable. Those are measured targets: unit tests pin the numbers, and the bot checks the levels after every change. Tuning (contact tolerances, bounce, camera) then happens one constant at a time, with a bot run after each. This turns "does it still feel right?" from a guess into a test.

**3. Simulation is pure.** The simulation (player, objects, collisions, camera math) doesn't include SFML graphics or audio headers and doesn't know a window exists. That's what makes headless tests, the bot, replays and CI possible, and it's what made the measurements in Part I cheap (the legacy code needed a virtual display just to load a level, because loading creates sprites).

**4. Every phase ends playable.** Commit after each phase. Phases 3–5 are tightly coupled; do them on one branch if that's easier.

**5. Evidence over intuition.** Every bug fix gets a test level (Appendix D) or a unit test. Every feel change gets a bot run.

**6. Determinism is a feature.** Same level + same inputs = same result, on every machine and compiler. It's what makes replays, the bot, practice checkpoints and bug reports work. Rules: the sim only sees integer ticks and `input_t`, never real time; no randomness; floating-point contraction off (1.1); no `-ffast-math`, ever.

---

## Phase 0: Baseline and safety net

Before changing anything, capture how the game behaves today, so you can prove later that you didn't break it.

### 0.1 Freeze the legacy version

```sh
git tag legacy-physics
git worktree add ../My_GD-legacy legacy-physics    # a second checkout that never changes
make -C ../My_GD-legacy                             # the old binary, for A/B play-testing
```

A worktree is better than a copied folder: it shares history, and `git worktree remove` cleans it up.

### 0.2 Run the lab once and keep the output

```sh
sudo apt install libcsfml-dev xvfb              # Ubuntu / Debian
sudo pacman -S csfml xorg-server-xvfb           # Arch (xvfb-run is in xorg-server-xvfb)
unzip my_gd_lab.zip && cd my_gd_lab
make LEGACY=../My_GD-legacy run-legacy | tee ../baseline.txt
cp ../My_GD-legacy/sol*.txt ../baseline/            # the bot's winning inputs, one char per frame
```

If CSFML was built from source into `/usr/local` (as on your machine), skip the package: the Makefiles find it there by default. Without `xvfb-run`, remove it from the lab's Makefile and the tools open real windows on your display for a moment; the results are the same.

`baseline.txt` should match Part I. It's the reference for the feel targets (Principle 2): speed, jump arc, and which levels are beatable. The `sol*.txt` files are the bot's winning inputs for the legacy physics; the new engine isn't expected to replay them exactly (its collision rules are different), but they're useful for A/B comparisons: run the same inputs through both and see where the outcomes differ.

### 0.3 Play-test notes

Play each level on the legacy binary and write down anything that feels deliberate (a tight timing, a spot where you rely on the portal teleport, the gap under the level 5 wall at x = 1700, which is exactly 100 px, the player's height). These are things the new core must preserve until you decide otherwise.

**Checkpoint:** tag exists, `baseline.txt` committed (or kept), legacy binary runs.

---

## Phase 1: Tooling, headers, safe fixes

### 1.1 Makefile

Problems today:

- `CC = epiclang`, and the link rule hardcodes `epiclang` instead of `$(CC)`.
- No warnings in `CFLAGS`.
- `LDFLAGS` is defined but never used, and the libraries are listed twice.
- **No header dependency tracking.** Editing `struct.h` doesn't rebuild the objects that include it. With struct layouts changing constantly in Phases 3–5, stale objects produce crashes that make no sense (two files disagreeing about a struct's size). This alone justifies the rewrite.
- No debug or test targets.

Design of the new Makefile:

- **Release and debug objects live in separate folders** (`build/release/`, `build/debug/`). With a single set of `.o` files, `make debug` followed by an edit and a plain `make` mixes sanitizer-instrumented and plain objects, and the link fails with undefined `__asan_*` symbols. Separate folders also make `make -j` safe (no `fclean` inside a target).
- The debug build produces `my_gd_debug`, so the two binaries never overwrite each other.
- `-ffp-contract=off`: gcc may otherwise fuse `a * b + c` into one FMA instruction on CPUs that have it, which rounds differently. The same level and inputs would then give different results on different machines, and replays would desync (Principle 6).
- Every header is in `include/` (game headers directly, simulation headers in `include/sim/`), so `-Iinclude` is the only include path.

```make
##
## ALEXNEX PROJECT, 2026
## Makefile
## File description:
## Makefile for my_gd
##

CC        = gcc
CFLAGS    = -Wall -Wextra -Iinclude -MMD -MP -ffp-contract=off
LDLIBS    = -lcsfml-graphics -lcsfml-window -lcsfml-system -lcsfml-audio -lm

BUILD     ?= release
ifeq ($(BUILD),debug)
    CFLAGS  += -g3 -O0 -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
    NAME    = my_gd_debug
else
    CFLAGS  += -O2
    NAME    = my_gd
endif
OUT       = build/$(BUILD)

SIM_SRC   = $(wildcard src/sim/*.c)
GAME_SRC  = $(wildcard src/*.c)
SIM_OBJ   = $(SIM_SRC:%.c=$(OUT)/%.o)
GAME_OBJ  = $(GAME_SRC:%.c=$(OUT)/%.o)
DEP       = $(SIM_OBJ:.o=.d) $(GAME_OBJ:.o=.d)
TEST_SRC  = $(wildcard tests/*.c)
HDR       = $(wildcard include/sim/*.h) $(wildcard tests/*.h)
TEST_FLAGS = -Wall -Wextra -Iinclude -ffp-contract=off -g -fsanitize=address,undefined

all: $(NAME)

$(NAME): $(GAME_OBJ) $(SIM_OBJ)
	$(CC) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(OUT)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

debug:
	$(MAKE) BUILD=debug

# Tests link ONLY the simulation: no CSFML, no window, no audio.
unit_tests: $(TEST_SRC) $(SIM_SRC) $(HDR)
	$(CC) $(TEST_FLAGS) $(filter %.c,$^) -o $@ -lm

test: unit_tests
	./unit_tests

# Parser fuzzing (Phase 8.1): needs clang.
fuzz_parser: tests/fuzz/fuzz_parser.c $(SIM_SRC)
	clang -Iinclude -ffp-contract=off -g -fsanitize=fuzzer,address,undefined $^ -o $@ -lm

clean:
	rm -rf build
	find . -type f \( -name '*~' -or -name '#*#' \) -delete

fclean: clean
	rm -f my_gd my_gd_debug unit_tests fuzz_parser

re: fclean
	$(MAKE) all

-include $(DEP)

.PHONY: all debug test clean fclean re
```

Notes:

- Recipes must be indented with **tabs**.
- `make CC=epiclang` still works.
- `-MMD -MP` writes a `.d` file next to each object listing the headers it used; `-include $(DEP)` makes make rebuild when any of them changes.
- `src/sim/` is created in Phase 2. Until then, `SIM_SRC` is simply empty.
- If you prefer an explicit file list over `wildcard` (some coding styles require it), keep one, but update it every time a file is added.

### 1.2 `.gitignore`

The current file ignores `coucou`, `level1.txt`, `log.txt` and `*.o/*` (which matches nothing: it means "files inside a directory named something.o"). Replace with:

```gitignore
build/
*.o
*.d
my_gd
my_gd_debug
unit_tests
fuzz_parser
save/
*.temp
*.tmp
*~
\#*\#
```

### 1.3 Headers

- `struct.h` includes `mygd.h`, which includes `struct.h`. It compiles only because of the include guards and the order things happen to be included in. Rule: **`struct.h` never includes `mygd.h`**. Each header includes exactly what it needs.
- `mygd.h` includes `<stdlib.h>` twice and ten SFML sub-headers already covered by `<SFML/Graphics.h>`, `<SFML/Audio.h>`, `<SFML/System.h>`. Trim to the umbrella headers.
- Phase 2 splits headers by layer; don't over-invest in reorganizing them now.

### 1.4 Safe fixes (independent of the rewrite)

1. **Out-of-bounds reads in `main`.** `write(2, ..., 54)` on a 45-byte literal and `write(2, ..., 31)` on a 26-byte one read past the strings (gcc flags both). Messages still say `my_hunter` (the file header comment too). Replace with:

   ```c
   static void print_usage(int fd)
   {
       const char *msg = "usage: ./my_gd [-h]\n";

       write(fd, msg, strlen(msg));
   }
   ```

   and use `strcmp(argv[1], "-h") == 0` instead of a length check plus two character checks.

2. **Dead code.** A project-wide grep finds no callers for `my_stricpy`, `create_vector`, `window_disp_clear`, `play_background_music`, `free_music_back`, `play_sound`. The last two have real bugs (`play_sound` returns an uninitialized struct on failure, `play_background_music` leaks on failure). Delete them. Dead code with bugs is the worst kind: someone eventually calls it.

3. **`my_put_nbr`**: `nb = -nb` overflows for `INT_MIN` (undefined behavior), despite the header comment saying it handles every int; `nb_c` is unused; `len_int` returns digits + 1. It's barely used; replace uses with `printf`/`dprintf` or fix it:

   ```c
   int my_put_nbr(int nb)
   {
       long n = nb;

       if (n < 0) {
           my_putchar('-');
           n = -n;
       }
       if (n >= 10)
           my_put_nbr((int)(n / 10));   /* safe: n / 10 always fits */
       my_putchar('0' + n % 10);
       return 0;
   }
   ```

4. **Allocation discipline.** Every `create_*` uses `malloc` without checking and fills fields by hand, so any code path that forgets a field leaves garbage (`start_level`'s error path returns a half-built level; `load_level_data` leaves `objects` uninitialized when the file is missing). Add one helper and use it everywhere a struct is allocated:

   ```c
   void *xcalloc(size_t n, size_t size)
   {
       void *p = calloc(n, size);

       if (p == NULL) {
           write(2, "my_gd: out of memory\n", 21);
           exit(84);
       }
       return p;
   }
   ```

   `calloc` zeroes memory: pointers start `NULL`, numbers `0`, `bool`s `false`. Most "uninitialized field" bugs disappear.

5. **Check resource loads once, centrally.** `load_textures` prints a warning and continues with `NULL` textures; later code passes them to `sfSprite_setTexture`. Missing assets should be a clear startup error: print which file is missing and exit, unless you deliberately support running without it.

6. **Run from any folder.** Every path (`res/`, `levels/`, `save/`) is relative to the current directory, so `cd /tmp && ~/My_GD/my_gd` can't find its assets. At startup, before loading anything, change to the executable's folder:

   ```c
   static void chdir_to_executable(void)
   {
       char path[4096];
       ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
       char *slash;

       if (n <= 0)
           return;                     /* not Linux: keep the current folder */
       path[n] = '\0';
       slash = strrchr(path, '/');
       if (slash == NULL)
           return;
       *slash = '\0';
       if (chdir(path) != 0)
           dprintf(2, "my_gd: cannot enter %s\n", path);
   }
   ```

   This keeps the "runs from its source folder" layout working from anywhere. An installed version would use `$XDG_DATA_HOME` for saves instead (6.4).

**Checkpoint:** `make` and `make debug` build with zero warnings; the game plays exactly as before, and starts from any folder.

---

## Phase 2: Architecture (simulation vs presentation)

### 2.1 Why split

Today, gameplay state and rendering objects are mixed in the same structs: a `block_t` holds its position *and* an `sfSprite`; `level_t` holds physics values *and* `sfText`s *and* an `sfClock`. Consequences:

- You can't load a level without a window, because loading creates sprites (the lab needed Xvfb for this reason).
- You can't test physics without CSFML.
- Freeing a level means freeing hundreds of sprites.
- Physics timing depends on when rendering calls it.

### 2.2 The two layers

All headers live in `include/`: simulation headers in `include/sim/`, game headers directly in `include/`. Sources stay in `src/sim/` and `src/`.

```text
include/sim/        simulation headers (no SFML)
    sim.h           public API of the simulation
    sim_types.h     rect_t, vec2_t, hitbox_t, object_t, player_t, run_state_t, sim_t ...
    constants.h     every physics and layout constant
    progress.h      progress store API
src/sim/            pure C, depends only on libc and libm
    level_load.c    parse a level (file or memory buffer) into objects
    hitbox.c        hitbox shapes at load (rotation, separating axes)
    sweep.c         swept box vs shape: time of impact and normal (4.3)
    sim.c           sim_reset, sim_tick, snapshots, state hash
    collision.c     broadphase, move_and_collide, contact responses, harm, interactive
    modes.c         the MODES table (3.3; FEATURES 6.1 extends it)
    camera.c        camera math (numbers only, no sfView)
    player.c        input, integration, rotation
    progress.c      attempts / best, save file
    bot.c           the solver (Phase 8), also used by --check

include/            game headers: mygd.h, struct.h, view.h ...
src/                the game: CSFML, window, audio, menus
    gd.c, scene.c, input.c, level.c (owns a sim_t + visuals), level_render.c, ...
```

Rules:

1. Nothing in `src/sim/` or `include/sim/` includes an SFML header. Use your own `vec2_t` and `rect_t`.
2. The game layer reads simulation state to draw it, and feeds input to it. It never writes positions.
3. Randomness, time and I/O stay out of `sim_tick`. Given the same level and the same inputs, a tick sequence must always produce the same result (Phase 8 tests that).
4. Visual-only state (explosion animation frame, attempt text timer, smoothing, the ball's rolling) lives in the game layer. One exception: the icon's `rotation` is computed in the tick (9.4) so it's frame-rate independent and replays show the same thing. It never affects gameplay (the rotating icon has no physics, 4.3), so the state hash and replays ignore it.

### 2.3 The simulation API

```c
/* include/sim/sim.h */
#ifndef MYGD_SIM_H
    #define MYGD_SIM_H
    #include "sim/sim_types.h"

int sim_load(sim_t *s, const char *path, sim_log_fn log);          /* 0 on success */
int sim_load_mem(sim_t *s, const char *buf, size_t len,
    const char *name, sim_log_fn log);                              /* same, from memory */
void sim_free(sim_t *s);
void sim_reset(sim_t *s);                      /* back to spawn, every object live again */
void sim_tick(sim_t *s, input_t in);           /* advance one fixed tick */
float sim_percent(const sim_t *s);

/* Run-state snapshots: practice checkpoints, the bot, determinism tests */
void sim_snapshot_init(sim_snapshot_t *snap, const sim_t *s);      /* allocates once */
void sim_snapshot_save(sim_snapshot_t *snap, const sim_t *s);
void sim_snapshot_restore(sim_t *s, const sim_snapshot_t *snap);
void sim_snapshot_free(sim_snapshot_t *snap);
uint64_t sim_state_hash(const sim_t *s);       /* hash of the whole run state */
uint64_t sim_physics_hash(const sim_t *s);     /* the same without the camera (bot memo, 3.3) */

#endif
```

`sim_log_fn` is a function pointer (`void (*)(const char *msg)`) for loader warnings, so the sim never prints directly. `sim_load` reads the file into memory and calls `sim_load_mem`, so tests and the fuzzer (8.1) can parse strings without temp files.

`input_t` is `{bool held; bool pressed;}` from the start. The cube and ship use `held` (the cube also jumps on `pressed`, so a tap shorter than a frame still jumps); `pressed` (one tick per physical press) is needed by the UFO, ball and orbs (FEATURES 1). Using the final signature now avoids changing every call site later.

`sim_tick` returns nothing; the caller checks `s->st.player.alive` and `s->st.complete` afterwards. Death handling (delay, explosion, attempts, save) belongs to the game layer, because it involves time, sound and files.

---

## Phase 3: The new core (a semi-physics engine)

### 3.0 What the engine is

Not a general physics engine: only **one body moves** (the player), nothing pushes back on the level, and nothing rotates physically (the icon's rotation is drawing only). But not a list of ad-hoc rules either. The player is a **body with a velocity**, forces change that velocity, and every tick the player's shapes are **swept** along that velocity through the level (a rigid square on horizontal faces, a circle on tilted faces and ceilings, a rigid inner box for fatal collisions): the engine finds the first surface it runs into, *when* (the fraction of the tick) and *which way that surface faces* (its normal), and the response depends on that contact. The same machinery handles flat blocks, slopes, ceilings, the ground and ship corridors.

The hitbox model:

| Hitbox | Shape | On contact |
|---|---|---|
| **Player rigid square** | 100 × 100, never rotates | stands and slides on **flat** neutral objects (and on a shape's topmost point, 4.4); harm and interactive objects; carries the **jump zone** (below) |
| **Player circle** | radius 50, inscribed in the square | stands and slides on **sloped** neutral objects; takes every **head hit** |
| **Player inner box** | 40 × 40, never rotates, same center | a neutral shape touching it kills: the player ran into something instead of landing on it |
| **Player icon** (dynamic) | 100 × 100, rotates | drawing only (9.4) |
| **Neutral** (blocks, slopes, the ground, corridor surfaces) | convex polygon: rectangle, rotated rectangle, triangle | **floor** (faces up, up to 50°): land and slide along it (the square on horizontal faces, the circle on tilted ones); **ceiling** (the circle): the player bounces back (ship) or dies (cube); **wall**: never stops the player, it goes into it. A horizontal face low enough to stay under the inner box is stepped onto; otherwise it reaches the inner box and the player dies. **The ground and the corridor boundaries never kill** (4.3): they're surfaces the player can't cross |
| **Harm** (spikes; saws later) | rectangle or rotated rectangle (circle for saws, FEATURES 10.6) | any contact kills |
| **Interactive** (portals; pads, orbs later) | rectangle or rotated rectangle | acts once, then disappears for the rest of the attempt. Portals push nothing (they're ghosts); pads and orbs push vertically |

**When can the player jump?** When a neutral hitbox (the ground included) touches the **jump zone**, the part of the rigid square below the inner box, and the player's momentum is downward relative to the surface it stands on (4.3). Inside the circle, any neutral hitbox counts; in the square's corners outside the circle, only flat (horizontal or vertical) faces. This only concerns jumping off neutral surfaces; pads, orbs, portals and spikes are purely the rigid square's business.

**Hitboxes are full shapes.** A collision is any overlap of **areas**, so a hitbox entirely inside another one counts: a player spawned inside a block, a spike hidden inside a portal, a small object swallowed by a big one. The separating-axis tests below (4.3) measure area overlap, so containment is detected like any other overlap, including at the very start of a move.

**Touching is not a collision.** Two hitboxes whose edges are exactly against each other are perfectly next to each other, with nothing in common: draw them in two colors and no point, however far you zoom in, has both. The level is a grid of 100 × 100 tiles and the player is 100 × 100, so a player on a tile and a block on the next tile (above it, in front of it) graze each other without colliding. That's what lets the player slide on a floor, and run exactly under level 5's 100 px gap: its top grazes the block above, and nothing happens.

**The player's own areas are glued.** The jump zone starts exactly where the inner box ends (20 px below the center), with nothing between them, so nothing can slip between the two: a surface lying exactly on that line has its object on one side of it, and that object overlaps the area on its side. A block whose top face is exactly on the line lies below it, in the jump zone: it's a step (4.4). A block whose bottom face is exactly on the line lies above it, in the inner box: it kills.

**Forces.** Every vertical change is a force on the player's body:

- **gravity**: a constant acceleration toward the player's current floor, per gamemode. Its sign is `gravity_dir` (+1: pulls down the screen, −1: pulls up). Everything that changes gravity goes through `player_flip_gravity` (3.4), which flips the sign and nothing else: the player's motion is untouched at the moment of the flip, and the new acceleration does the rest. Gravity portals and the ball's click stop there; GD's blue pads, blue orbs and green orbs flip and then **set** a speed toward the new floor (FEATURES 10.1), which is an impulse like any other, not part of the flip;
- **ship thrust**: an upward acceleration while the button is held;
- **the jump**: an impulse from below that **sets** the rise speed (`vy = JUMP_V`); it only applies when the player can jump (grounded);
- **pads and orbs** (FEATURES 10): impulses that **set** the rise speed to their own value. Setting instead of adding makes a pad launch the same height whatever the player was doing, like GD;
- **contacts**: landing removes the speed into the floor, a ceiling bounce reflects part of it.

Horizontally, the player moves at `vx`: the base scroll speed times the current speed multiplier. Speed portals (FEATURES 10.3) change the multiplier: a constant horizontal boost. Nothing in the level slows the player horizontally; being stopped horizontally is death.

**Deterministic.** Same level + same inputs = same result, on every machine (Principle 6): integer ticks, fixed operation order, no randomness, and every value that depends on a non-exact math function (`sin`, `cos`) rounded to a grid at load time.

### 3.1 Coordinates

- **World space** = level file coordinates, unchanged. With the camera at (0, 0), the screen looks exactly like the first frame today.
- y grows downward (SFML convention). The ground's top surface is `GROUND_Y = 850`.
- The player's position is its **center**. The sprite's origin is already its center (`sfSprite_setOrigin(sprite, {50, 50})`).
- `vx` is the horizontal speed (always to the right). `vy` is the **rise speed**: positive means moving away from the floor, i.e. up with normal gravity. The world displacement for a tick is `(vx, -vy * gravity_dir)`. `gravity_dir` is `+1` (normal) in this plan's scope; FEATURES 9 adds `-1`, and writing everything with it from day one means flipped gravity needs no rewrite.
- Contact **normals** are unit vectors in world space, pointing **out of the obstacle toward the player**. "Up" for the player is `(0, -gravity_dir)`.
- 1 grid unit = 50 px. A `size N` object is `N * 50` px wide and tall, unless the line gives a separate width or height (7.2).
- **Rotation**: any object can be rotated by any angle, in degrees, clockwise on screen (SFML's convention, since y grows downward), around the center of its rect. The rect stored in `object_t` is always the **unrotated** one, exactly as the file describes it. The hitbox rotates with it (4.2).
- **Screen space** = world minus camera. The player always sits at screen x = 350.

Appendix F summarizes all spaces and units.

### 3.2 Units: per tick, not per second times dt

v1 proposed per-second constants multiplied by a float `dt`. `1/240` isn't exactly representable in binary, so `750 * dt` isn't exact, and `x += 750 * dt` drifts a little every tick. Over a long level the error shows in exact-equality situations. Your level 5 has one: the gap under the wall at x = 1700 is **exactly** 100 px, and the player is exactly 100 px tall. It works because every value involved is exact.

So the simulation stores velocities in **px per tick** and accelerations in **px per tick²**, with constants written per second for readability and converted at compile time. The simulation runs at **240 ticks per second** from the start, like GD: collisions are 4× finer than at 60, and the constants below are chosen so the cube's jump has exactly today's feel (11.1).

```c
/* include/sim/constants.h */
#ifndef MYGD_CONSTANTS_H
    #define MYGD_CONSTANTS_H

    #define TICK_RATE           240
    #define PER_TICK(v)         ((double)(v) / TICK_RATE)
    #define PER_TICK2(a)        ((double)(a) / ((double)TICK_RATE * TICK_RATE))

    /* World */
    #define UNIT                50.0
    #define GROUND_Y            850.0
    #define LEVEL_END_PADDING   500.0   /* level ends 500 px after the last object */
    #define KILL_CEILING_MARGIN 600.0    /* flipped gravity only: kill line above the highest object (4.7) */

    /* Player */
    #define PLAYER_HALF         50.0    /* rigid square 100x100, circle radius 50 (4.3) */
    #define PLAYER_SCREEN_X     350.0
    #define PLAYER_SPAWN_X      350.0
    #define PLAYER_SPAWN_Y      (GROUND_Y - PLAYER_HALF)   /* 800, on the ground */

    /* Motion: GD's own numbers (FEATURES 6.9). GD counts in blocks (1 block = 1 player
       = 100 px here) and writes every vertical speed relative to the horizontal one:
       1 velocity unit = the normal scroll speed, 1 acceleration unit = that speed
       squared per block. Constants marked "ours" are values GD doesn't publish. */
    #define BLOCK               100.0    /* one GD block: the player's size              */
    #define SPEED_SLOW          (8.372 * BLOCK)    /* GD's five speeds, written 0.5, 1,  */
    #define SPEED_NORMAL        (10.386 * BLOCK)   /* 2, 3 and 4 in level files (10.3)   */
    #define SPEED_FAST          (12.914 * BLOCK)
    #define SPEED_VFAST         (15.6 * BLOCK)
    #define SPEED_XFAST         (19.2 * BLOCK)
    #define SCROLL_SPEED        SPEED_NORMAL       /* 1038.6 px/s, 4.3275 px/tick        */
    #define V_UNIT              SPEED_NORMAL       /* 1 GD velocity unit                 */
    #define A_UNIT              (SPEED_NORMAL * SPEED_NORMAL / BLOCK)  /* 10786.9 px/s^2 */

    #define CUBE_GRAVITY        (0.876 * A_UNIT)   /* 9449.3 px/s^2                      */
    #define CUBE_JUMP_V         (1.9522 * V_UNIT)  /* 2027.6 px/s: GD's 1.94 raised so the
                                                    measured apex is its 2.1333 blocks (11.1) */
    #define CUBE_MAX_FALL       (2.6 * V_UNIT)     /* 2700.4 px/s                        */
    #define CUBE_SPIN           324      /* deg/s, the icon's spin in the air (cosmetic) */
    #define RISE_EPSILON        (1.0 / 4096.0)   /* px/tick, jump-zone momentum test (4.3) */
    #define SHIP_GRAVITY        (0.99 * A_UNIT)    /* ours: 10678.9 px/s^2               */
    #define SHIP_THRUST         (2.24 * A_UNIT)    /* ours: 24162.7 px/s^2               */
    #define SHIP_MAX_VY         (2.24 * V_UNIT)    /* ours: 2326.5 px/s                  */

    /* Contacts (4.4) */
    #define FLOOR_MIN_DOT       0.64279 /* cos(50 deg): steeper than 50 deg is a wall     */
    #define FLOOR_MAX_TAN       1.19175 /* tan(50 deg)                                    */
    #define PLAYER_INNER_HALF   20.0    /* 40x40 inner box: a neutral touch kills (4.3) */
    #define BOUNCE_RESTITUTION_SHIP 0.3 /* share of the rise speed kept by a ceiling bounce */
    #define BOUNCE_MIN_SPEED    60       /* px/s: slower hits just stop (no micro-bounces) */
    #define MAX_CONTACTS        4        /* contacts resolved per tick                     */
    #define MAX_CANDIDATES      256      /* objects a tick's broadphase may collect (4.1)  */
    #define MAX_TOUCHES         32       /* interactive objects touched in one tick (4.6)  */
    #define CONTACT_SKIN        (1.0 / 1024.0)   /* gap kept from tilted faces (4.4)     */

    /* Camera (screen y of the player's center) */
    #define CAM_TOP_MARGIN      200.0
    #define CAM_BOTTOM_MARGIN   790.0
    #define CAM_TAU             0.08    /* seconds, smoothing time constant (3.5)          */
    #define CAM_LERP            0.0507502406   /* = 1 - exp(-1/(TICK_RATE*CAM_TAU)) */

    /* Ship corridor (5.2) */
    #define CORRIDOR_MAX_HEIGHT 1000.0   /* the tallest mode corridor (ship, UFO, wave) */
    #define VIEW_HEIGHT         1080.0   /* logical screen height the camera reasons in (9.1) */

    /* Death */
    #define DEATH_DELAY_TICKS   (TICK_RATE / 2)   /* 0.5 s */

#endif
```

The trigonometric constants are literals, not `cos(...)` calls at runtime: `cos` can differ in the last bit between C libraries, and a unit test recomputes each literal in double to catch a typo.

The player's horizontal position is never accumulated in float. The run state keeps a **double** `distance` (px scrolled since spawn):

```c
st->distance += p->vx * t;                  /* t: fraction of the tick actually moved */
p->pos.x = PLAYER_SPAWN_X + st->distance;
```

At GD's normal speed `vx` is 4.3275 px/tick, which isn't a binary fraction, so the sum isn't bit-exact; in `double` it drifts by less than 10⁻¹⁰ px over a whole level, far below anything the engine compares. (GD's 4× speed gives exactly 8 px/tick and 3× exactly 6.5.) What has to be exact is **vertical**: `settle` assigns y from the face's own coordinate (4.4), so level 5's 100 px gap still works bit for bit whatever `vx` is. Speed portals change `vx` (FEATURES 10.3).

### 3.3 Types (`include/sim/sim_types.h`)

**Double precision everywhere in the sim.** Every world coordinate, size, velocity and collision computation in `src/sim/` is a `double`. A `float` has 24 bits of precision: past x = 8192 px its smallest step is larger than the 1/1024 px grid and contact skin this engine relies on, and at x = 30 000 (level 6 is about 26 000 px long) it's about 0.002 px, coarser than the skin. A `double` (53 bits) keeps sub-micro-pixel precision over any level, x and y alike. The cost is negligible (a handful of candidates per tick), determinism is unchanged (IEEE double, no contraction), and the renderer converts to `float` relative to the camera when it builds vertices. Every simulation snippet in this plan is written with `double`; `float` only appears where precision doesn't matter: the icon's cosmetic rotation, the percentage shown on screen, and the game layer (vertices, SFML calls).

The simulation is split in two parts:

- **Level data**: everything read from the file, computed at load, then **never modified** (objects, hitboxes, level end, kill ceiling).
- **Run state**: everything that changes during an attempt (tick, distance, player, camera, corridor, culling cursor, which interactive objects are spent). It's small, and copying it is a snapshot.

```c
#ifndef MYGD_SIM_TYPES_H
    #define MYGD_SIM_TYPES_H
    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>

typedef struct vec2 { double x; double y; } vec2_t;   /* the sim computes in double (below) */
typedef struct rect { double x; double y; double w; double h; } rect_t;
typedef void (*sim_log_fn)(const char *msg);

typedef struct input {
    bool held;                /* button down during this tick           */
    bool pressed;             /* first tick after a physical press      */
} input_t;

typedef enum obj_type {
    OBJ_BLOCK,                /* neutral: rectangle                          */
    OBJ_SLOPE,                /* neutral: right triangle (7.2)               */
    OBJ_SPIKE,                /* harm                                        */
    OBJ_PORTAL,               /* interactive: acts once, then untouchable    */
    /* later: OBJ_SAW (harm), OBJ_PAD, OBJ_ORB, OBJ_GRAVITY, OBJ_SPEED (FEATURES 10) */
    OBJ_TYPE_COUNT
} obj_type_t;

typedef enum obj_category { CAT_NEUTRAL, CAT_HARM, CAT_INTERACTIVE } obj_category_t;
extern const obj_category_t OBJ_CATEGORY[OBJ_TYPE_COUNT];

typedef enum gamemode { MODE_CUBE, MODE_SHIP, MODE_COUNT } gamemode_t;

typedef enum hold_state {     /* 3.4: GD's buffered clicks and orb locking */
    HOLD_NONE,                /* button up                                 */
    HOLD_FRESH,               /* down, hasn't produced a jump or orb yet   */
    HOLD_USED                 /* down, already used: jumps, but no orbs    */
} hold_state_t;

#define HB_MAX_VERTS 4

typedef enum shape_kind { SHAPE_POLY, SHAPE_CIRCLE } shape_kind_t;
typedef enum face_kind { FACE_HORIZONTAL, FACE_VERTICAL, FACE_TILTED } face_kind_t;

typedef struct hitbox {       /* full shape: area, not outline (3.0) */
    shape_kind_t kind;
    rect_t aabb;              /* world-space bounds: broadphase, first axes     */
    int nverts;               /* poly: 3 or 4, world space, clockwise on screen */
    vec2_t verts[HB_MAX_VERTS];
    int naxes;                /* poly: separating axes other than x and y       */
    vec2_t axes[HB_MAX_VERTS];     /* unit edge normals, pointing outward      */
    double axis_lo[HB_MAX_VERTS];   /* the shape's projection on each axis      */
    double axis_hi[HB_MAX_VERTS];
    uint8_t face_kind[HB_MAX_VERTS];   /* edge i = verts[i] -> verts[i + 1]:
                                          FACE_HORIZONTAL, FACE_VERTICAL, FACE_TILTED */
    vec2_t face_n[HB_MAX_VERTS];       /* edge i's outward unit normal (G.2)       */
    double face_off[HB_MAX_VERTS];     /* its line: dot(face_n, p) = face_off      */
    vec2_t center;            /* circle (FEATURES 10.6) */
    double radius;
} hitbox_t;

typedef struct object {       /* level data: never modified after load */
    obj_type_t type;
    rect_t rect;              /* unrotated bounds, world space, as in the file */
    double rotation;          /* degrees, clockwise, around the rect's center  */
    hitbox_t hitbox;          /* computed once at load, rotation included      */
    int size;                 /* as written in the file                        */
    gamemode_t portal_mode;   /* OBJ_PORTAL only                                */
    int line;                 /* source line, for messages and stable sort      */
} object_t;

typedef struct level_data {   /* immutable after sim_load */
    object_t *objects;        /* contiguous, sorted by hitbox.aabb.x (then line) */
    size_t nb_objects;
    double reach;              /* max over objects of hitbox.aabb.w: broadphase (4.1) */
    double end_shift;          /* distance at which the level completes (3.4)     */
    double kill_y;             /* kill ceiling, world y (4.7)                     */
    char name[128];
} level_data_t;

typedef struct player {
    vec2_t pos;               /* center, world space; pos.x = spawn + distance */
    vec2_t prev_pos;          /* at the start of the tick (interpolation, 11.3) */
                              /* the three hitboxes share pos as their center (4.3) */
    double vx;                /* px per tick, to the right                      */
    double vy;                /* px per tick, rise speed (away from the floor)  */
    int gravity_dir;          /* +1 normal; -1 flipped (FEATURES 9)             */
    gamemode_t mode;
    bool grounded;            /* supported by a floor this tick                 */
    bool can_jump;            /* the jump zone test of the last tick (4.3)      */
    double surface_rise;      /* rise speed imposed by the supporting surface   */
    vec2_t support_normal;    /* that surface's normal (the icon lies along it) */
    float rotation;           /* the icon's angle, degrees, cosmetic (9.4)      */
    hold_state_t hold;        /* HOLD_NONE, HOLD_FRESH, HOLD_USED (3.4)         */
    bool alive;
} player_t;

typedef struct camera {
    vec2_t pos;               /* world position of the screen's top-left */
} camera_t;

typedef struct touch {        /* an interactive object met during the tick (4.6) */
    size_t index;
    double at;                /* path position: leg index + fraction along it     */
} touch_t;

typedef struct face {         /* one horizontal face of a hitbox (4.4, G.7) */
    double y;
    double x0;
    double x1;
} face_t;

typedef struct ship_bounds {
    bool active;
    double top;               /* world y of the ceiling surface */
    double bottom;            /* world y of the floor surface   */
} ship_bounds_t;

typedef struct run_state {    /* everything an attempt changes */
    long tick;
    double distance;          /* px scrolled since spawn (3.2)  */
    double speed_mult;        /* 1 until speed portals          */
    player_t player;
    camera_t cam;
    ship_bounds_t bounds;
    size_t first_active;      /* broadphase cursor (4.1)        */
    bool complete;
    uint64_t *spent;          /* bitset, one bit per object (5.1) */
    size_t spent_words;
} run_state_t;

typedef struct sim_snapshot {
    run_state_t st;           /* st.spent points into the snapshot's own buffer */
} sim_snapshot_t;

typedef struct sim {                  /* the run state, plus this tick's scratch space */
    level_data_t lvl;
    run_state_t st;
    size_t cand[MAX_CANDIDATES];      /* broadphase result, object indices (4.1)     */
    size_t nb_cand;
    size_t passed[MAX_CANDIDATES];    /* objects passed into as walls this tick (4.4) */
    size_t nb_passed;
    touch_t touch[MAX_TOUCHES];       /* live interactive objects touched (4.6)      */
    size_t nb_touch;
    int legs;                         /* legs advanced this tick: path positions     */
    double tick_x0;                   /* distance when the tick started (G.8)        */
    double tick_left;                 /* the fraction of the tick still to travel    */
} sim_t;

static inline bool is_spent(const run_state_t *st, size_t i)
{
    return (st->spent[i / 64] >> (i % 64)) & 1u;
}

static inline void set_spent(run_state_t *st, size_t i)
{
    st->spent[i / 64] |= (uint64_t)1 << (i % 64);
}

#endif
```

Per-mode parameters live in a table, so the engine never writes `if (mode == ...)` for physics values (FEATURES 6.1 extends it with more modes and fields):

```c
typedef struct mode_ops {
    const char *name;         /* name in level files ("cube", "ship")               */
    double half;               /* rigid square half size, circle radius (4.3)        */
    double inner_half;         /* inner box half size: a neutral touch kills         */
    double gravity;            /* px/tick^2                                          */
    double max_fall;           /* px/tick, fall speed cap                            */
    double head_restitution;   /* ceiling hit: < 0 dies, else share of vy bounced back */
    double corridor_height;   /* the corridor this mode's portal opens, px; 0: none (5.2) */
} mode_ops_t;

extern const mode_ops_t MODES[MODE_COUNT];   /* include/sim/modes.h, src/sim/modes.c */

const mode_ops_t MODES[MODE_COUNT] = {
    [MODE_CUBE] = {.name = "cube", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(CUBE_GRAVITY),
        .max_fall = PER_TICK(CUBE_MAX_FALL), .head_restitution = -1.0},   /* no corridor */
    [MODE_SHIP] = {.name = "ship", .half = PLAYER_HALF, .inner_half = PLAYER_INNER_HALF,
        .gravity = PER_TICK2(SHIP_GRAVITY),
        .max_fall = PER_TICK(SHIP_MAX_VY), .head_restitution = BOUNCE_RESTITUTION_SHIP,
        .corridor_height = 1000.0},
};
```

What each design choice buys you:

- **One contiguous `object_t` array** instead of three `NULL`-terminated pointer arrays: one `free`, one `qsort`, cache-friendly loops, and portals can no longer be forgotten by the free function (F8).
- **No sprite per object.** The game layer builds static vertex arrays from the objects once per level (Phase 9). Loading no longer needs a window.
- **Hitboxes computed at load**, rotation included, with their separating axes precomputed: a sweep test is a handful of multiplications per axis.
- **Categories** (`OBJ_CATEGORY`) decide what a contact does; types only decide shapes and effects. A new object type is a shape plus a category.
- **Level data never changes**, which is Principle 1 applied to the data structure: `const level_data_t *` can be shared by the game, the renderer, the bot and the editor's playtest.
- **The run state is a snapshot.** `sim_snapshot_save` copies `st` plus the spent bitset (a few hundred bytes). That's a practice checkpoint (13.1), a bot branch point (8.3), and the determinism test's comparison (8.1). `sim_state_hash` hashes it for the determinism tests, and `sim_physics_hash` (the same without the camera) gives the bot an exact memo key.
- **`spent` is a bitset in the run state**, not a flag in each object: resetting is one `memset`, snapshots include it for free, and the level data stays untouched.
- **`bool`** instead of `'y'`/`'n'`/`'a'`/`'d'` chars: a typo like `'Y'` can't compile silently anymore.

The snapshot functions, in `sim.c`:

```c
void sim_snapshot_init(sim_snapshot_t *snap, const sim_t *s)
{
    snap->st = s->st;
    snap->st.spent = xcalloc(s->st.spent_words, sizeof(uint64_t));
}

void sim_snapshot_save(sim_snapshot_t *snap, const sim_t *s)
{
    uint64_t *buf = snap->st.spent;

    snap->st = s->st;
    snap->st.spent = buf;
    memcpy(buf, s->st.spent, s->st.spent_words * sizeof(uint64_t));
}

void sim_snapshot_restore(sim_t *s, const sim_snapshot_t *snap)
{
    uint64_t *buf = s->st.spent;

    s->st = snap->st;
    s->st.spent = buf;
    memcpy(buf, snap->st.spent, s->st.spent_words * sizeof(uint64_t));
}
```

(`xcalloc` is the Phase 1 helper; give the sim its own copy so it doesn't depend on the game layer.)

`sim_state_hash` feeds every run-state field (except the icon's cosmetic `rotation`, 2.2) and the bitset into a 64-bit hash such as FNV-1a over the bytes. Hash fields one by one, not the raw struct: padding bytes are uninitialized (Appendix G.9). It's the determinism tests' hash (8.1, 8.2): everything must match.

`sim_physics_hash` is the same without the camera: the fields that decide what happens next (tick, distance, speed, player, corridor, spent bitset). The camera never influences the physics (3.5), so two runs that reach the same physical state with the camera at different heights have exactly the same future. The bot's memo (8.3) uses this one, so it recognizes them as the same state.

### 3.4 The tick

```c
void sim_tick(sim_t *s, input_t in)
{
    run_state_t *st = &s->st;
    player_t *p = &st->player;

    if (!p->alive || st->complete)
        return;
    p->prev_pos = p->pos;
    player_update_hold(p, in);                /* 0. fresh / used / none (below)           */
    player_apply_input(p, in);                /* 1. impulses (jump) and thrust            */
    player_apply_gravity(p);                  /* 2. gravity, speed caps                   */
    move_and_collide(s);                      /* 3. sweep: neutral, harm, interactive (4) */
    if (p->alive)
        collide_kill_ceiling(p, &s->lvl);     /* 4. (4.7)                                 */
    if (p->alive)
        apply_interactive(s);                 /* 5. portals, pads...: in contact order    */
    if (p->alive)
        update_can_jump(s);                   /*    the jump zone, last (4.3)             */
    camera_follow(&st->cam, p, &st->bounds);  /* 6. (3.5)                                 */
    st->tick += 1;
    if (p->alive && st->distance >= s->lvl.end_shift)
        st->complete = true;                  /* 7.                                       */
    if (p->alive)
        player_update_rotation(p);            /* 8. the icon, cosmetic (9.4)              */
}
```

Why this order:

- **Forces before movement** (semi-implicit Euler): the velocity is updated first, then the position moves with the new velocity. It's stable, and it's the order the legacy game used (input, gravity, move), so the jump constants of 11.1 reproduce today's arc exactly.
- **Interactive effects after the whole move**: the sweep collects the interactive objects touched during the tick (4.6) and step 5 applies them in contact order. A portal touched mid-tick changes the mode for the next tick, a difference of at most 4 ms, and the movement code never has to handle a mode change in the middle of a sweep.
- **`can_jump` last**: the jump zone is tested after interactive objects have acted, so anything that just launched the player (a pad, an orb, a gravity flip) makes the momentum test fail on its own. Tested before them, a cube running onto a pad on the ground would still be allowed to jump next tick, and its jump would overwrite the pad's launch.
- **Camera after movement**: it frames where the player is now. It no longer decides anything about the physics (corridors come from portals, 5.2).
- **Completion after everything**: a player who dies on the last tick has died.

Forces (`player.c`):

```c
void player_update_hold(player_t *p, input_t in)     /* first thing in the tick */
{
    if (in.pressed)
        p->hold = HOLD_FRESH;                  /* a new press: a fresh hold */
    else if (!in.held)
        p->hold = HOLD_NONE;                   /* released */
}                                              /* still held: fresh or used, unchanged */

void player_apply_input(player_t *p, input_t in)
{
    bool down = in.held || in.pressed;         /* held: auto-jump on every landing, like GD */

    if (p->mode == MODE_CUBE && down && p->can_jump) {
        p->vy = PER_TICK(CUBE_JUMP_V);         /* an impulse from below: sets the rise speed,
                                                  the same on flat ground and on slopes */
        p->can_jump = false;
        p->grounded = false;
        p->hold = HOLD_USED;                   /* this hold now can't activate orbs */
    }
    if (p->mode == MODE_SHIP && down && p->vy < PER_TICK(SHIP_MAX_VY))
        p->vy = fmin(p->vy + PER_TICK2(SHIP_THRUST),
            PER_TICK(SHIP_MAX_VY) + MODES[MODE_SHIP].gravity);  /* thrust only up to the
                                                  limit; never uses the hold */
}
```

**Holds, buffered clicks and orbs.** This is GD's input rule, and it's exact (no time window, no forgiveness):

- A hold is **fresh** from the moment it's pressed, and a hold that was already down when the attempt started is fresh too (`sim_reset` sets `HOLD_FRESH`; releasing sets `HOLD_NONE`).
- **A buffered click** is a hold started *before* a contact and still held *at* the contact (a landing, entering an orb). It acts then. A press released before the contact does nothing. That's why the jump test is simply "down at the tick `can_jump` is true" (4.3): there is no press memory.
- **Using a hold** (the cube jumping off a surface, FEATURES' ball flipping, the UFO hopping, activating an orb) makes it **used**. A used hold still jumps off every surface it lands on while it stays down (surface jumps don't care whether the hold is fresh or used), but it activates **no orb** until it's released and pressed again.
- **Ship and wave** holds are always fresh: flying, and bumping into surfaces while flying, never use them.
- **Orb priority:** on a tick where the player touches a live orb with a fresh hold, the orb acts and the tick's input is consumed: no surface jump that tick, even if `can_jump` is true (FEATURES 10.2).

`hold` is part of the run state (snapshots, hash). In this plan's scope only the cube's jump uses it; orbs, the ball and the UFO are in FEATURES.

`down` is `held || pressed` for every mode that reads the button's state (cube, ship, and FEATURES' ball and wave): a tap shorter than a frame is never seen as `held`, but its `pressed` tick still gives one tick of jump, thrust or rise.

```c
void player_apply_gravity(player_t *p)
{
    const mode_ops_t *m = &MODES[p->mode];
    double limit = -m->max_fall;

    if (p->grounded && p->surface_rise - m->gravity < limit)
        limit = p->surface_rise - m->gravity;  /* a surface descending faster than the fall
                                                  cap: one tick of gravity past it */
    if (p->vy > limit)                         /* gravity accelerates up to the fall limit, */
        p->vy = fmax(p->vy - m->gravity, limit);   /* and no further */
}
```

**Staying on fast descending slopes.** Riding down a slope, the player's rise speed is the slope's (`surface_rise`, 4.4), and gravity pushing it a little further is what brings it back onto the slope every tick. When the slope descends faster than the fall cap (a 45° slope from 2× speed for the UFO, 3× for the ship and the ball, 4× for the cube), plain capped gravity would add nothing: the player would move exactly parallel to the slope, 1/1024 px above it, and never land again (`grounded` false, the icon's support stale). So a player grounded on the previous tick always gets one tick of gravity past its surface's rise speed, and lands on it again. In the air (`grounded` false) the cap is unchanged: gravity alone never makes a fall faster than `max_fall`.

Flipping gravity (used by FEATURES' gravity portals, ball clicks, and the blue and green pads and orbs, which set a speed right after it; nothing in this plan's scope calls it yet, but it belongs to the core):

```c
void player_flip_gravity(player_t *p)
{
    p->gravity_dir = -p->gravity_dir;   /* the constant acceleration now points the other way */
    p->vy = -p->vy;                     /* vy is relative to gravity: negating it keeps the
                                           world velocity exactly the same */
    p->grounded = false;
    p->can_jump = false;                /* the jump zone flips to the other side (4.3) */
}
```

That's all a gravity change does. `vy` is stored relative to gravity (the rise speed, 3.1), so a flip that didn't negate it would silently reverse the player's motion on screen; negating it means the player keeps moving exactly as it was, and only the acceleration changes. A player falling onto the ground at 500 px/s who flips keeps moving down at 500 px/s for that instant, then decelerates and rises toward its new floor. The same function is used everywhere, so a gravity portal, a ball click and a blue orb can't disagree.

**Caps bind the player's own motion, not objects.** One rule for both directions:

- **Gravity** accelerates toward the floor up to the mode's fall cap (`max_fall`: 2700 px/s for the cube, 2327 for the ship) and no further.
- **Input** (the cube's jump, the ship's thrust, FEATURES' UFO hop) can't push the player upward faster than the mode's own limit: the jump and the hop *set* a fixed speed below it, and the ship's thrust only adds speed up to `SHIP_MAX_VY` (2326.5 px/s). Holding, the ship climbs until it settles exactly at that speed (the thrust is capped at the limit plus one tick of gravity, so after this tick's gravity it sits on the limit); released, it falls up to its fall cap.
- **Objects aren't bound by either cap**: an orb, a pad, riding a slope, a portal, or the momentum carried over from another mode can take the player past the fall cap or the rise limit, in any mode, the ship included. The player keeps that speed: gravity doesn't add to a fall that's already faster than the cap, input doesn't add to a rise that's already faster than the limit, and each keeps acting normally in the other direction (gravity still slows a fast rise, thrust still slows a fast fall).

**Jumps don't depend on slopes.** A jump sets the rise speed to `CUBE_JUMP_V`, whether the cube stands on flat ground, climbs a slope or goes down one: same force everywhere.

The jump only applies when the player **can** jump: `can_jump` is the jump-zone test at the end of the previous tick (4.3). A cube on the ground gets gravity every tick too; its sweep immediately meets the floor at `t = 0` and landing zeroes `vy` again, which keeps it supported and its jump zone touching the floor while running. `vx` isn't touched here: it's `PER_TICK(SCROLL_SPEED) * speed_mult`, set on reset and by speed portals.

Reset:

```c
void sim_reset(sim_t *s)
{
    run_state_t *st = &s->st;

    st->player = (player_t){
        .pos = {PLAYER_SPAWN_X, PLAYER_SPAWN_Y},
        .prev_pos = {PLAYER_SPAWN_X, PLAYER_SPAWN_Y},
        .vx = PER_TICK(SCROLL_SPEED),
        .gravity_dir = 1,
        .mode = MODE_CUBE,
        .grounded = true,
        .can_jump = true,
        .hold = HOLD_FRESH,                   /* a hold carried into the attempt is fresh */
        .support_normal = {0.0, -1.0},        /* standing on the ground */
        .alive = true,
    };
    st->cam = (camera_t){{0.0, 0.0}};
    st->bounds = (ship_bounds_t){0};
    st->tick = 0;
    st->distance = 0.0;
    st->speed_mult = 1.0;
    st->first_active = 0;
    st->complete = false;
    memset(st->spent, 0, st->spent_words * sizeof(uint64_t));   /* every interactive object is live again */
}
```

Every attempt, including the first, starts at `(350, 800)`, already on the ground. Today the first attempt spawns 50 px further right than the retries (F2); that's gone.

Percentage:

```c
float sim_percent(const sim_t *s)
{
    double pct = s->st.distance / s->lvl.end_shift * 100.0;

    return pct > 100.0 ? 100.0f : (float)pct;
}
```

`end_shift`, computed at load: the largest object **right edge** (`hitbox.aabb.x + hitbox.aabb.w`, rotation included) + `LEVEL_END_PADDING`, and at least 100. Every object is fully passed, with 500 px of runway after the last one. (Today's rule used the last object's *left* edge, so a 400 px platform starting at x = 5000 ended the level only 100 px after its end.)

### 3.5 Camera

A smoothed follow, in the sim (it's deterministic and cheap, and FEATURES' out-of-corridor rules and the editor's start positions use it):

```c
void camera_follow(camera_t *c, const player_t *p, const ship_bounds_t *b)
{
    double sy = p->pos.y - c->pos.y;           /* player's screen y */
    double target = c->pos.y;

    if (b->active) {
        c->pos.y = b->top - (VIEW_HEIGHT - (b->bottom - b->top)) / 2.0;   /* the corridor,
                                                       centered on screen (5.2) */
        return;
    }
    if (sy < CAM_TOP_MARGIN)
        target = p->pos.y - CAM_TOP_MARGIN;
    else if (sy > CAM_BOTTOM_MARGIN)
        target = p->pos.y - CAM_BOTTOM_MARGIN;
    else if (p->grounded && c->pos.y < 0.0)
        target = 0.0;                          /* back to the ground view after flying */
    if (target > 0.0)
        target = 0.0;                          /* never below the ground view */
    c->pos.y += (target - c->pos.y) * CAM_LERP;
}
```

- When the player's center leaves the 200..790 band on screen, the camera eases toward keeping it at the edge of the band.
- After a ship section, once the player is grounded, it eases back to the ground view. (Today, in level 7, the ground stays at screen y 600 for the rest of the attempt after the ship section.)
- It never shows below the ground view (today's camera can overshoot a few pixels).
- `CAM_LERP` is derived from a **time constant** (`CAM_TAU`, 3.2), so the camera feels the same at any tick rate; a unit test recomputes it from `TICK_RATE` in double.
- In a corridor, the camera is **locked** in y with the corridor **centered on screen**: a 1000 px corridor leaves 40 px above and below, an 800 px one 140 px (5.2). It doesn't follow the player inside the corridor; it's released when the player leaves it, and then eases back to the follow rule above.
- The lock is set directly by the portal, so entering a corridor is a cut, not a pan. If that feels abrupt in play-testing, `center_corridor` can leave the camera where it is and let a transition ease into the locked position over a few ticks: a presentation choice (11.2).

Horizontal camera: `cam.x = player.x - PLAYER_SCREEN_X`, always (the game layer computes it).

### 3.6 Fixed-timestep loop (game layer)

```c
void handle_playing(gd_t *gd, level_t **level)
{
    level_t *lv;
    sfInt64 frame_us;

    if (*level == NULL)
        *level = level_start(gd, gd->selected_level_id);
    lv = *level;
    if (lv == NULL || level_poll_events(level, gd) != 0)
        return;                                   /* scene changed */
    frame_us = sfClock_restart(lv->clock).microseconds;
    if (frame_us > 250000)
        frame_us = 250000;                        /* no spiral of death after a hitch */
    lv->accumulator += frame_us * TICK_RATE;      /* units: microseconds x TICK_RATE */
    while (lv->accumulator >= 1000000) {
        level_step(lv, gd, input_for_tick(gd));   /* one sim tick + death/complete handling */
        lv->accumulator -= 1000000;
    }
    level_render(gd, lv);
}
```

Why:

- Today, physics runs once per rendered frame. A slow frame makes the whole game run in slow motion. Without the 60 FPS cap, a 144 Hz monitor would run the game 2.4× faster.
- Today's order is render → physics → events, which adds a frame of input latency. The new order is events → ticks → render.
- **Integer accumulator.** `sfTime` is already an integer number of microseconds. Accumulating `frame_us * TICK_RATE` and subtracting exactly `1000000` per tick is exact: no rounding, ever, whatever the tick rate (1/60 s isn't a whole number of microseconds, but `1000000 / TICK_RATE` never has to be computed). A float accumulator loses a little on every subtraction. `lv->accumulator` is an `sfInt64`.
- The simulation itself only sees integer ticks, so it stays deterministic.

`input_for_tick(gd)` returns the `input_t` for the next tick. In Phase 10 it's simply `{input_held(gd), false}`; FEATURES 1 adds exact `pressed` edges without changing this loop.

Restart `lv->clock` when the level starts, after respawn, and on `sfEvtGainedFocus`, so a pause doesn't dump a burst of ticks.

`level_step` wraps the sim with the game rules:

```c
void level_step(level_t *lv, gd_t *gd, input_t in)
{
    if (lv->state == LEVEL_DYING) {
        if (--lv->death_ticks <= 0)
            level_respawn(lv, gd);
        return;
    }
    if (lv->state != LEVEL_PLAYING)
        return;
    sim_tick(&lv->sim, in);
    if (!lv->sim.st.player.alive)
        level_on_death(lv, gd);
    else if (lv->sim.st.complete)
        level_on_complete(lv, gd);
}
```

---

## Phase 4: The collision engine

### 4.1 Broadphase

Objects are sorted by `hitbox.aabb.x`, the left edge of their hitbox (ties broken by `line`, so the order is identical on every libc; `qsort` isn't stable). The player only moves right, so a cursor skips everything that's certainly behind, and the scan stops at the first object that starts beyond the tick's reach:

```c
/* Collects the objects this tick's movement can touch into s->cand (a small fixed array). */
static void broadphase(sim_t *s, rect_t sweep)
{
    const level_data_t *lv = &s->lvl;
    size_t i;

    while (s->st.first_active < lv->nb_objects &&
        lv->objects[s->st.first_active].hitbox.aabb.x + lv->reach <= sweep.x)
        s->st.first_active += 1;
    s->nb_cand = 0;
    for (i = s->st.first_active; i < lv->nb_objects; i++) {
        const hitbox_t *h = &lv->objects[i].hitbox;

        if (h->aabb.x >= sweep.x + sweep.w)
            break;                              /* sorted: nothing further can touch */
        if (rect_overlap(h->aabb, sweep) && s->nb_cand < MAX_CANDIDATES)
            s->cand[s->nb_cand++] = i;
    }
}
```

`sweep` is the bounding box of everything the player can cover this tick: its box at the start and at the end of the unobstructed move `(vx, -vy * gravity_dir)`, grown by the step-up height (4.4). `reach` is the widest hitbox in the level (`max(hitbox.aabb.w)`), computed at load. Every object before the cursor has `hitbox left + reach <= sweep left`, so its right edge is behind the player. The cost per tick is the handful of objects near the player, not the level's length; this replaces today's screen-space window `100 < x < 600`, which causes F5.

Culling uses **hitbox** bounds, not sprite bounds: a hitbox can stick out of its sprite (a rotated object's hitbox bounds differ from its rect), and a sprite-based bound would drop objects early. `cand` lives in `sim_t` next to the run state (it's scratch space, rebuilt every tick, not part of snapshots); `MAX_CANDIDATES` = 256 is far more than a tick can reach, and `--check` warns if a level exceeds it anywhere.

### 4.2 Hitbox shapes and rotation

At load, `hitbox.c` builds each object's shape from its type, **then** rotates it with the object:

| Type | Category | Local shape (before rotation), for a rect `(x, y, w, h)` |
|---|---|---|
| `block` | neutral | the full rect |
| `slope` | neutral | right triangle `(x, y+h)`, `(x+w, y)`, `(x+w, y+h)`: rises from left to right, 45° for a square |
| `spike` | harm | `{x + 0.3w, y + 0.2h, 0.4w, 0.8h}`: the middle 40%, from 20% below the tip to the base. Forgiving like GD's, and proportional to the spike's size |
| `portal` | interactive | the full rect |

(Today's spike box is `{x + 30, y + size * 10, 40, 100 - size * 10}`: identical for `size 2`, but fixed 40 px wide and reaching 50 px below a `size 1` spike's sprite, into the ground. The proportional box fixes other sizes; level 2's size-1 spike at `(1000, 800)` is the one to play-test.)

```c
static vec2_t rotate_point(vec2_t p, vec2_t c, double deg)
{
    double r = deg * M_PI / 180.0;
    double cs = cos(r);
    double sn = sin(r);

    return (vec2_t){c.x + (p.x - c.x) * cs - (p.y - c.y) * sn,
        c.y + (p.x - c.x) * sn + (p.y - c.y) * cs};
}

static double grid(double v)                       /* 1/1024 px grid */
{
    return round(v * 1024.0) / 1024.0;
}

void hitbox_build_poly(hitbox_t *h, const vec2_t *local, int n, rect_t rect, double deg)
{
    vec2_t c = {rect.x + rect.w / 2.0, rect.y + rect.h / 2.0};

    h->kind = SHAPE_POLY;
    h->nverts = n;
    for (int i = 0; i < n; i++) {
        vec2_t v = rotate_point(local[i], c, deg);

        h->verts[i] = (vec2_t){grid(v.x), grid(v.y)};
    }
    h->aabb = bounds_of(h->verts, n);
    build_axes(h);          /* unit outward edge normals, minus x/y and duplicates; lo/hi */
}
```

- **Rounding to a 1/1024 px grid** does two things. At multiples of 90° it removes the tiny `cos(90°) ≈ 6e-17` error, so a block rotated by a quarter turn has exactly axis-aligned edges and level 5's exact 100 px gap still works. And it protects determinism (Principle 6): `sin`/`cos` aren't required to be correctly rounded, so two C libraries can disagree in the last bit; after rounding to the grid, they give the same hitbox. (1/1024 is a power of two, so the rounded values are exact doubles, anywhere in a level.)
- **`build_axes`** (Appendix G.2) computes each edge's outward normal, normalized with `sqrt` (IEEE requires `sqrt` to be correctly rounded, so it's deterministic). Axes parallel to x or y are dropped (the bounding box already covers them), and so are duplicates (a rectangle's opposite edges share an axis). A rotated rectangle keeps 2 axes, a triangle up to 3 (1 for a 45° slope: its two legs are x and y). For each axis, `axis_lo`/`axis_hi` store the shape's projection interval, computed once.
- **Every angle is a real shape**, including for blocks: a block rotated by 30° is a tilted rectangle whose top face is a 30° slope the player can slide on. Nothing is snapped.
- **Face kinds** are set here, once: each edge of a neutral polygon is **horizontal**, **vertical** (both "flat") or **tilted**, from its grid-rounded vertices (exact comparisons: after rounding, a horizontal edge has two equal y). A block at a multiple of 90° has only flat faces; a 45° `slope` has a horizontal bottom, a vertical side and a tilted face; a `slope` rotated 180° has a horizontal **top**. The face kind decides which of the player's shapes stands on that face, which part of the jump zone counts, and where the step-up applies (4.3, 4.4), at no cost during play.

### 4.3 The player's shapes and the sweeps

**The player's shapes.** They all share the player's center (`pos`). Sizes are for the 100 px modes (`half` = 50, `inner_half` = 20):

| Shape | Size | Rotates | Job |
|---|---|---|---|
| **Rigid square** | 100 × 100 | never | stands and slides on **horizontal** faces (and the ground and corridor floors); meets **vertical** faces as walls; detects **harm** and **interactive** objects; carries the **jump zone** |
| **Circle** | radius 50 (inscribed in the square) | — | stands and slides on **tilted** faces; takes **every head hit** (ceilings of any kind) |
| **Inner box** | 40 × 40 | never | any neutral contact kills: the player ran into something instead of landing on it |
| **Icon** (dynamic) | 100 × 100 | yes | drawing only: spins in the air, lies along what the player stands on (9.4). No physics |

**Flat and tilted faces.** Each face of every neutral hitbox is tagged at load (4.2): **horizontal** or **vertical** (together: "flat") or **tilted**. It's decided per face, not per object: a `slope` rotated 180° has a horizontal top, and that top behaves exactly like a block's. The ground and the corridor surfaces are horizontal. It's a real angle-based distinction, computed once, so play pays nothing for it.

Why two support shapes:

- On **horizontal** faces the player behaves like a box: it stays perfectly level until the rigid square has entirely left a ledge (the icon doesn't budge either), and then it falls.
- On **tilted** faces the player behaves like a ball: the circle rests exactly 50 px from the surface whatever its angle, so the tilted icon sits flush on it, and the circle rolls smoothly over the corner where two tilted faces meet. Where a tilted face meets a horizontal one, the square takes over (4.4).

**The jump zone.** The part of the rigid square **below the inner box**: the strip from 20 px below the center to the square's bottom (30 px tall). With flipped gravity it's mirrored: the strip above the inner box. The zone has two parts:

- **inside the circle** (dark green in your drawing): allows a jump when **any** neutral hitbox touches it;
- **outside the circle**, in the square's two corners (light green): allows a jump when a **flat face** (horizontal or vertical) touches it.

The zone test, at the end of the tick, for each neutral candidate and active surface:

- **dark part**: the polygon clipped to the zone's half-plane (below the line at `center + 20`, at most 5 vertices after clipping) overlaps the circle (`overlap_circle_poly`);
- **light part**: one of the polygon's flat faces (a horizontal or vertical segment) intersects the zone rectangle. A segment against an axis-aligned rectangle is a few comparisons. (A flat face lying entirely inside the circle's part is caught by the dark test anyway; the segment test only adds the corners.)

The player can jump (`can_jump`) when a neutral hitbox touches the zone in a part that allows it, **and** it has downward momentum **relative to the surface it's standing on**:

```c
p->can_jump = zone_touched && p->vy <= p->surface_rise + RISE_EPSILON;
```

`surface_rise` is the rise speed the supporting surface imposes: 0 on flat ground, `vx × slope` while sliding up a slope (4.4), 0 when nothing supports the player. So:

- climbing a slope: the slope forces the player up at exactly `surface_rise`, and it can jump;
- rising through a block's edge after a jump: `vy` is far above 0, so it can't;
- jumping next to an uphill slope: the jump's 1.95 units is far more than a 45° slope's 1 unit, so no double jump;
- falling past a block's side and touching it with the zone (dark part: anything; light corners: its vertical side or its horizontal top): it can jump. That's GD's corner forgiveness.
- running into a tall block's side: the block enters the zone before it reaches the inner box, so a player with downward momentum can still jump during those last pixels. Whether the block kills is decided only by the inner box, never by the zone.
- **No jump buffer.** A press or hold only counts if it's there **when** the player is allowed to jump (a "buffered click" in GD is exactly that: a hold started early and still down at the contact, 3.4): `can_jump` comes from the end of the previous tick, and the input of this tick either has `held` (or `pressed`) true, or not. Pressing and releasing just before landing does nothing; holding through the landing jumps on the first tick it's allowed, which is how you rejump as soon as possible.

The jump zone is a **support** test, not a collision: it has to see the surface the player stands on, which the player only grazes (a square resting on the ground touches it without overlapping it). So it counts exact contact: the zone is tested grown by `2 * CONTACT_SKIN`, which also covers the 1/1024 px gap the circle keeps from tilted faces (4.4). `can_jump` is computed at the end of the tick and used by the next tick's input (the cube's jump, FEATURES' ball flip).

**Sweeps.** The core test: one of the player's shapes, moving by `d` during a leg of the move, against one convex shape. It returns **when** they first touch while moving into each other (`t` in 0..1, the fraction of `d`) and the **normal** of what was hit.

```c
/* contact.surface: an object index, or one of the three surfaces (4.3) */
#define SURF_GROUND (-1L)
#define SURF_FLOOR  (-2L)          /* the corridor's floor   */
#define SURF_CEIL   (-3L)          /* the corridor's ceiling */

static inline bool is_surface(long s) { return s < 0; }

typedef struct contact {
    double t;            /* fraction of the move, 0..1                        */
    vec2_t normal;       /* unit, out of the obstacle toward the player       */
    bool flat;           /* the square met the shape's top or bottom (y axis) */
    double offset;       /* the face's line: normal . point = offset          */
    long surface;        /* object index, or SURF_GROUND / SURF_CEIL / SURF_FLOOR */
} contact_t;

typedef enum event_kind { EV_NONE, EV_CONTACT, EV_STEP } event_kind_t;

typedef struct event {   /* what cuts a leg of the move short (4.4) */
    event_kind_t kind;
    double t;            /* fraction of the leg, 0..1; 1 with EV_NONE */
    contact_t contact;   /* EV_CONTACT */
    face_t step;         /* EV_STEP: the face to climb onto          */
} event_t;
```

*Square against a polygon* (`sweep_box_poly`): the separating axis theorem over time. For each axis (y first, then x, then the polygon's own axes), the square's projection is an interval moving at a constant speed; the shapes overlap while **all** the intervals overlap. The first contact is the latest "start overlapping" time over all axes, and the face is the one of that axis.

```c
/* Axis-aligned square (center c, half h) moving by d, against a convex polygon:
   the moment their areas start overlapping, if the square moves into the polygon.
   Areas, not outlines: containment counts. Touching (zero area) doesn't. */
bool sweep_box_poly(vec2_t c, double h, vec2_t d, const hitbox_t *hb, contact_t *out)
{
    double t_in = -INFINITY;
    double t_out = INFINITY;
    vec2_t n_in = {0.0, 0.0};
    int k_in = -1;
    double offset = 0.0;

    for (int k = 0; k < hb->naxes + 2; k++) {
        vec2_t a = hitbox_axis(hb, k);          /* k = 0: y, 1: x, then the shape's axes */
        double r = h * (fabs(a.x) + fabs(a.y));   /* the square's projection radius */
        double lo = hitbox_lo(hb, k) - r;       /* where the center may be, on this axis */
        double hi = hitbox_hi(hb, k) + r;
        double p = c.x * a.x + c.y * a.y;
        double v = d.x * a.x + d.y * a.y;
        double t0;
        double t1;

        if (v == 0.0) {
            if (p <= lo || p >= hi)
                return false;                   /* never overlap on this axis */
            continue;
        }
        t0 = (lo - p) / v;
        t1 = (hi - p) / v;
        if (t0 > t1) {
            double tmp = t0;

            t0 = t1;
            t1 = tmp;
        }
        if (t0 > t_in) {                        /* strict: on a tie, the earlier axis wins */
            t_in = t0;
            n_in = v > 0.0 ? (vec2_t){-a.x, -a.y} : a;
            k_in = k;
            offset = v > 0.0 ? -hitbox_lo(hb, k) : hitbox_hi(hb, k);   /* the face entered */
        }
        if (t1 < t_out)
            t_out = t1;
        if (t_in >= t_out)
            return false;                       /* the overlaps never coincide */
    }
    if (k_in < 0 || t_in > 1.0 || t_out <= 0.0)
        return false;                           /* no moving axis, or not during this move */
    if (n_in.x * d.x + n_in.y * d.y >= 0.0)
        return false;                           /* moving away from, or along, that face */
    out->t = t_in < 0.0 ? 0.0 : t_in;           /* already overlapping: contact now */
    out->normal = n_in;
    out->flat = k_in == 0;
    out->offset = offset;
    return true;
}
```

How to read it:

- `r` is how far the square reaches along the axis; widening the shape's interval by `r` turns "square vs shape" into "the square's center vs a wider interval", so only the center's projection has to move.
- `t0`/`t1` are the times the center's projection enters and leaves that interval. Entering from the low side means the obstacle is on the `+a` side, so the normal toward the player is `-a`.
- The strict comparisons are what make **touching not a collision** (3.0): a square sliding exactly along a block's top face, or exactly under a 100 px gap, has `p == lo` or `p == hi` on that axis and never counts as overlapping. `t_in` is the moment the areas would start overlapping, which is the moment the outlines meet: the response stops the player exactly there.
- **Only moving into a face counts** (`d · normal < 0`): a player sliding along a surface with a rounding sliver of overlap doesn't "hit" it again at `t = 0` on every leg.
- **Axis order breaks ties**: the y axis is tested first, so when the square meets a block's corner exactly (vertical and horizontal contact start at the same time), the contact is vertical: a landing, not a wall.
- **Already overlapping** at the start (`t_in < 0 < t_out`, which includes one shape entirely inside the other) is a contact at `t = 0`, with the normal of the axis where the contact is shallowest in the direction of motion.
- `flat` means the contact came through the y axis: the square met the polygon's topmost part (or bottommost, from below). That's a horizontal face, or a single point when the shape's top is a vertex (a slope's peak, the tip of a block rotated 45°): either way, the square stands on it (4.4).
- `offset` describes the face's line (`normal · point = offset`), taken straight from the shape's precomputed projection. For the y axis it's exactly ± the shape's top (or bottom) world y, so the response can put the square **exactly** on it (4.4).

*Circle against a polygon* (`sweep_circle_poly`): the circle's center touches the polygon when it reaches the polygon **inflated by the radius**: each face pushed out by 50 px along its normal, with a circle of radius 50 around each vertex. The earliest time over those pieces is the contact. For a face, it's the time the center's projection on the face's normal reaches `offset + r`, kept only if the center is then within the face's extent; the normal is the face's. For a vertex, it's the smallest `t` in 0..1 with `|c + t·d − vertex| = r` (a quadratic, solved with `sqrt`), and the normal points from the vertex to the center at that time. Already overlapping counts as a contact at `t = 0`, and only moving into the contact counts, as for the square. `sqrt` is correctly rounded, so this stays deterministic. **Appendix G.4 writes this out**: the face equation, the vertex quadratic, which root to keep and how ties are broken.

*Touch tests* (`sweep_box_touch`, `sweep_circle_touch`, `overlap_box_poly`, `overlap_circle_poly`): the same loops without the moving-into test and without the normal, for everything where "any contact" is all that matters: harm and interactive objects, the inner box, the circle during a step-up's lift (4.4), and the jump zone. Like everything else, they measure area overlap: touching isn't a collision. The swept versions return the first `t` in 0..1 where the areas start overlapping (0 if they already do). The jump zone's dark part is the polygon clipped to the zone's half-plane (below the line at `center + 20`, at most 5 vertices after clipping), tested against the circle; Appendix G.6 gives the clipper and both tests.

The ground and corridor surfaces are infinite half-planes, tested with a single axis (Appendix G.5): the ground is "everything below y = 850" with normal `(0, -1)`, the corridor ceiling "everything above `bounds.top`" with normal `(0, 1)`, the corridor floor "everything below `bounds.bottom`". `SURF_*` values in `contact.surface` identify them (`is_surface`). They're flat.

**Surfaces are one thing: something the player can't cross.** The ground and the corridor boundaries behave exactly the same way:

- They hold the player up (landing, sliding) and stop it (a head hit: a bounce, or a stop), in every mode and both gravities, and they **never kill**. A mode whose head hit is fatal on objects (the cube, the wave) just stops against a surface and slides along it.
- They're never tested by the inner box, and the sweep never skips them: a half-plane can't be entered from the side, so every contact with one is a floor or a ceiling. A player that ends up overlapping a surface is on the wrong side of an uncrossable line, and its next contact with it puts it back on the surface. Example: a wave sliding on the ground (center 15 px above it) takes a cube portal; the portal doesn't move it (5.2), so the cube's bigger square starts 35 px into the ground, and the next tick's contact with the ground lifts it onto it.
- The only way to die from a surface is to have **gone through it**: the player's center beyond its line. The sweep makes that impossible, so it only happens with a bug or a hack; it's checked at the end of the move as a safety net (4.4).
- **The kill line is something else** (4.7): not a surface, never touched as a floor or a ceiling, just the limit that ends a flipped player's endless fall.
- Corridor boundaries exist only while their corridor is active. A portal that removes the corridor or replaces it with another one (5.2) removes the old boundaries' collision **instantly**: the player can never touch, let alone die on, the boundaries it just left. Only their drawing fades out (5.3).

### 4.4 Movement and neutral contacts

The support shapes move and hold the player up (the square on horizontal faces, the circle on tilted ones), the circle takes head hits, the inner box judges, and the rigid square also handles hazards, interactive objects and the jump zone. Neutral objects **never stop the player horizontally**: a player that runs into a block's side keeps going into it, and dies when the block reaches the **inner** box. That's the "stopped in its movement" case. Running into a wall is fatal after 30 px (the gap between the square's edge and the inner box's), unless the block is low enough to step onto (below).

The move is a series of **legs**: straight segments, cut at each event (a neutral contact or a step). Every leg is checked **as it happens**, in this order: first the deaths along it, then the interactive objects it touches, then the player moves, then the event's response. **A death stops everything**: the player is placed exactly where it died, and there's no further movement, contact, step, or interaction in that tick (and none after: the player is dead).

```c
static void move_and_collide(sim_t *s)
{
    player_t *p = &s->st.player;
    vec2_t vel = {p->vx, p->vy};                 /* velocity used for this tick's displacement */
    double remaining = 1.0;

    broadphase(s, tick_sweep_bounds(p));
    s->legs = 0;                                 /* path positions: leg index + fraction */
    s->nb_touched = 0;                           /* interactive objects touched (4.6)    */
    s->nb_passed = 0;                            /* objects passed into this tick (walls) */
    p->grounded = false;                         /* surface_rise keeps the last support's value */
    for (int n = 0; p->alive && remaining > 0.0; n++) {
        bool last = n == MAX_CONTACTS;           /* contact budget used up */
        vec2_t d = {vel.x * remaining, last ? 0.0 : -vel.y * p->gravity_dir * remaining};
        event_t e = {.kind = EV_NONE, .t = 1.0};

        if (!last)
            first_event(s, d, vel, &e);          /* earliest neutral contact or step, if any */
        if (leg_deaths(s, d, e.t))               /* inner box and harm, on [0, e.t]: */
            return;                              /* the player stops where it died  */
        leg_touches(s, d, e.t);                  /* live interactive objects, for step 5 (4.6) */
        advance(s, d, e.t);
        remaining *= 1.0 - e.t;
        if (e.kind == EV_CONTACT)
            respond(s, &e.contact, &vel);        /* a head hit may kill: the loop stops */
        else if (e.kind == EV_STEP)
            step_up(s, &e.step, &vel);           /* a lift, checked like a leg (below) */
    }
    if (p->alive && crossed_surface(s))
        p->alive = false;                        /* went through the ground or a boundary:
                                                    only a bug or a hack can do that (4.3) */
    if (p->alive && !p->grounded)
        p->surface_rise = 0.0;                   /* nothing supports the player now */
}
```

- `advance` moves the player (`distance += d.x * t`, `pos.y += d.y * t`, `pos.x` from `distance`, 3.2) and counts the leg. A position along the tick's path is `leg + t` (the leg's index plus the fraction along it), so "earlier in the tick" is a plain comparison across legs.
- `first_event` finds the earliest event of the leg; **Appendix G.9 writes it out**, with the exact filter that decides which faces and corners each shape may hit and the order that breaks ties (contacts before steps, and among contacts the first one tested at that `t`). In short:
  - the **square** against every neutral hitbox and the ground and corridor floor, keeping only contacts through the **y axis** (coming down onto the shape's top: a horizontal face, or its topmost point when that's a vertex, such as a slope's peak or the tip of a block rotated 45°; the square stands on it level, like on a ledge, and that's intended) or the **x axis** (running into a vertical face, or the shape's leftmost point: a wall). The sweep reports which axis the contact came through (4.3); a contact through any other axis is a tilted face, which the square ignores. A square touching a shape from below is left to the circle;
  - the **circle** against **tilted faces**, and the corners between two tilted faces: every contact;
  - the **circle** against every face and corner that faces the player's ceiling side, and the corridor ceiling, for **head hits**.

  It **skips objects the tested shape already overlaps** at the start of the leg (the player is inside them, not resting on them), and the objects passed into as walls earlier in the tick: those are blocks the player is going into from the side; the inner box and the step-up decide about them. A floor the player rests on is only touched, not overlapped, so it keeps holding the player up. Surfaces are never skipped (4.3): they can't be crossed.
- The rest of the move continues after each event with the corrected velocity, so one tick can land on a slope and slide along it, or land and then bump a ceiling. Horizontal movement is never reduced: a living player always covers exactly `vx` per tick, and `distance` stays exact. If the contact budget runs out, the rest of the move is done horizontally only, which keeps `distance` exact; if the player really is squeezed, the inner box kills it.
- `surface_rise` isn't reset at the start of the tick: it keeps the rise speed of the surface that supported the player at the end of the last tick, so a step (below) early in the tick is judged against the surface the player is actually riding. `land` updates it, and it's set to 0 at the end of the tick if nothing supports the player then.

**Contact response.** It depends on how the surface faces the player's "up" (`(0, -gravity_dir)`):

```c
static void respond(sim_t *s, const contact_t *c, vec2_t *vel)
{
    player_t *p = &s->st.player;
    double up_dot = -c->normal.y * p->gravity_dir;   /* 1: flat floor, -1: flat ceiling */

    if (up_dot >= FLOOR_MIN_DOT)
        land(p, c, vel);                        /* floor, up to 50 deg: stand and slide */
    else if (up_dot <= -FLOOR_MIN_DOT)
        head_hit(p, c, vel);                    /* ceiling: the circle */
    else
        pass_into(s, c);                        /* wall: not stopped, the player enters it */
}
```

**Floor: land and slide.** The player is supported: it keeps its horizontal speed and slides along the surface, so its rise speed becomes whatever keeps it on the surface at speed `vx`. That rise speed is also `surface_rise`, the reference for "downward momentum" in the jump test (4.3):

```c
static void settle(player_t *p, const contact_t *c, double half)
{
    if (c->flat) {                              /* the square, exactly on the shape's top */
        p->pos.y = (c->offset + half) * c->normal.y;   /* n . center = offset + half, n.y = +-1 */
        return;
    }
    p->pos.y += c->normal.y * CONTACT_SKIN;     /* the circle on a slope: 1/1024 px away,
                                                   vertically (x belongs to distance, 3.2) */
}

static void land(player_t *p, const contact_t *c, vec2_t *vel)
{
    double rise = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;

    settle(p, c, MODES[p->mode].half);
    p->grounded = true;
    p->support_normal = c->normal;              /* for the icon (9.4) */
    /* surface direction: n.x * dx + n.y * dy = 0, so dy/dx = -n.x / n.y (world y);
       rise speed = -(world dy) * gravity_dir */
    p->surface_rise = rise;                     /* the surface's own rise, whatever the mode */
    vel->y = rise;
    if (!MODES[p->mode].keep_vy_on_surface)
        p->vy = rise;
}
```

- On a flat face, `settle` puts the square's bottom exactly on the face's y, bit for bit: touching isn't overlapping, so the next legs slide along it. That's also why running exactly under level 5's 100 px gap still works.
- On a slope, the circle stays 1/1024 px (`CONTACT_SKIN`, a power of two, exact) off the surface, so the next sweep along the slope starts outside it; gravity brings it back onto the slope at the next tick (3.4 makes sure it always does, even past the fall cap). Its center is 50 px from the surface whatever the angle.
- On flat ground `normal.x` is 0, so `vy` becomes exactly 0. Going up a 45° slope at 1038.6 px/s, `vy` becomes +1038.6 px/s: the player climbs. Going down, `vy` becomes negative and it descends. Leaving the top of an uphill slope, the player still has that upward speed and briefly lifts off, which is what GD does. (With `keep_vy_on_surface` modes, FEATURES 8.3, only `vel.y` changes, not `p->vy`; `surface_rise` is always the surface's own rise, computed from its normal, so it stays right for every mode.)
- **Walking off a flat ledge** (a horizontal face ending in a corner): the square stays supported as long as any part of its bottom is over the face, so the player stays perfectly level until the square has entirely left it, then falls with gravity alone: there's no snapping down onto a lower surface, however small the drop. (A circle would roll around the corner; that's why horizontal faces support the square.) The same holds when the flat top continues into a **downhill slope**: the player stays level over the slope until the square has left the flat face, then drops onto the slope. That's the intended behavior, not a transition to smooth out. Likewise, where a slope gets steeper downhill, the player flies off the bend and lands further down, as in GD.
- **Where a slope meets a flat top** (a tilted face meeting a horizontal one), at the moment the circle reaches the corner the square's bottom is up to 14.6 px inside the flat part (for a 45° slope). The player is still rising along the slope (`vy = vx`), so by the time the inner box's leading side is over the flat part, the square is at or near its level: it either lands on it normally, or the step-up lifts it the last few pixels (at most 14.6).

**Ceiling: head hit.** Always the circle. It depends on the gamemode (`head_restitution`): the cube dies; the ship bounces back with a share of the speed it came with. Hits slower than `BOUNCE_MIN_SPEED` just stop, so a ship pressed against a ceiling slides along it instead of vibrating. **Surfaces never kill** (4.3): a head hit against the ground or a corridor boundary is a bounce or a stop, never a death. A mode whose head hit is fatal (the cube, the wave) just stops against it and slides along it.

```c
static void head_hit(player_t *p, const contact_t *c, vec2_t *vel)
{
    double e = MODES[p->mode].head_restitution;
    double follow = vel->x * (c->normal.x / c->normal.y) * p->gravity_dir;
    bool surface = is_surface(c->surface);     /* the ground or a corridor boundary */

    if (e < 0.0 && !surface) {
        p->alive = false;                       /* the move stops here (death stops everything) */
        return;
    }
    if (e < 0.0)
        e = 0.0;                                /* a surface: stop and slide, never die */
    p->pos.y += c->normal.y * CONTACT_SKIN;
    p->vy = p->vy > PER_TICK(BOUNCE_MIN_SPEED) ? -p->vy * e : 0.0;
    if (p->vy > follow)
        p->vy = follow;                         /* tilted ceiling: at least follow it down */
    vel->y = p->vy;
}
```

Under a **tilted** ceiling that comes down toward the player, the bounce alone could leave it rising into the slope; `follow` is the rise speed that runs exactly along that ceiling at speed `vx` (the same formula as `land`), and the player's rise speed is capped by it, so a ship slides along a descending ceiling instead of hitting it again every sub-step. On a flat ceiling `follow` is 0 and changes nothing. FEATURES 6.1 gives the other modes: UFO and ball bounce, wave dies (except against corridor boundaries).

**Wall: pass into it.** A surface steeper than 50° doesn't stop the player: the move continues untouched and the object joins the tick's passed list, which `first_event` skips for the rest of the tick (G.9):

```c
static void pass_into(sim_t *s, const contact_t *c)
{
    if (c->surface >= 0 && s->nb_passed < MAX_CANDIDATES)
        s->passed[s->nb_passed++] = (size_t)c->surface;   /* surfaces are never passed */
}
```
 Then either the step-up lifts the player onto it, or it reaches the inner box and the player dies.

**Step-up.** A horizontal face (of any neutral object) that the square has entered from the side, and that lies below the inner box (on its bottom edge or lower) and above the feet (up to 30 px above them), is climbed: at the moment the inner box's **leading side** (its right side, the player moving right) reaches the face, the player is lifted so the square rests on it. **Only if the player could jump at that moment**: its momentum is downward relative to its surface (`vy <= surface_rise`, the same test as the jump zone, 4.3; the face itself is inside the jump zone by construction). A player going up through the corner of a block isn't snapped on top of it: it keeps its fluid motion, phases a little through the corner, and either clears it or meets the inner box.

The step is an **event of the leg** (`first_event`), found at the exact moment it happens, not after the move: x and y move linearly along a leg, so that moment is computed directly.

```c
/* The step event of one up-facing horizontal face f = {y, x0, x1} on this leg, if any. */
static bool step_event(const sim_t *s, vec2_t d, vec2_t vel, face_t f, double *t_out)
{
    const player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double g = p->gravity_dir;
    double lead = p->pos.x + m->inner_half;        /* the inner box's leading side */
    double t = lead >= f.x0 ? 0.0 : (f.x0 - lead) / d.x;   /* when it reaches the face */
    double y = p->pos.y + d.y * t;                 /* the player's center then */
    double feet = y + m->half * g;                 /* the square's bottom (top when flipped) */
    double inner_edge = y + m->inner_half * g;     /* the inner box's bottom */

    if (vel.y > p->surface_rise + RISE_EPSILON)
        return false;                              /* rising: no snap, the motion stays fluid */
    if (t > 1.0 || p->pos.x + d.x * t - m->half >= f.x1)
        return false;                              /* not during this leg, or already past it */
    if ((f.y - inner_edge) * g < 0.0 || (feet - f.y) * g <= 0.0)
        return false;                              /* above the inner box's bottom (the inner
                                                      box decides), or at or below the feet
                                                      (the square lands on it) */
    if (lift_hits_surface(s, y, feet - f.y))
        return false;                              /* no lift into an uncrossable surface */
    *t_out = t;
    return true;
}
```

`first_event` runs it for every up-facing horizontal face of the neutral candidates (a block's top, a rotated slope's flat top), given as segments `{y, x0, x1}`, and keeps the earliest (on a tie, the highest face). The response, `step_up`:

```c
static void step_up(sim_t *s, const face_t *f, vec2_t *vel)
{
    player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double g = p->gravity_dir;
    double target = f->y - m->half * g;          /* the center once the square rests on it */
    vec2_t lift = {0.0, target - p->pos.y};      /* straight against gravity, at most 30 px */

    if (leg_deaths(s, lift, 1.0))                /* harm and the inner box, along the lift  */
        return;
    if (circle_hits_neutral(s, lift)) {          /* a ceiling on the way up: no bounce      */
        p->alive = false;
        return;
    }
    leg_touches(s, lift, 1.0);                   /* interactive objects crossed by the lift */
    advance(s, lift, 1.0);
    p->pos.y = target;                           /* exact, like settle on a flat face       */
    p->grounded = true;
    p->surface_rise = 0.0;
    p->support_normal = (vec2_t){0.0, -g};
    p->vy = 0.0;
    vel->y = 0.0;                                /* the rest of the tick runs along the face */
}
```

1. **The lift** is a leg of its own, straight up against gravity until the square rests on the face (at most 30 px). Everything moves with it, and it's checked like any leg: the inner box and the rigid square (`leg_deaths`), the interactive objects it touches (`leg_touches`), and the **circle against every neutral object** (`circle_hits_neutral`: `sweep_circle_touch` over the lift, no face filter, plus the corridor's ceiling). If the circle runs into anything on the way up, the player dies there, in every mode: a lift is forced, not a flight, so there's no bounce. Grazing a ceiling exactly at the top of the lift isn't a collision (3.0).
2. **The landing**: `pos.y = f.y - half * gravity_dir` is exact, the same assignment `settle` makes on a flat face, so the square rests on the face bit for bit.

- The trigger is exactly what you described: nothing happens while the face only overlaps the outer part of the square; the moment the inner box's leading side reaches the face's start, the player is lifted onto it. That's 30 px of horizontal penetration for a block entered from the side.
- It works for **any object's horizontal face**, since flatness is per face: a low step shaped like a rotated slope, whose top is horizontal, is climbed like a block.
- **A face exactly on the inner box's bottom edge is a step**: the jump zone and the inner box are glued (3.0), and the object lies below that line, in the jump zone, so it's climbed (a 30 px step). Any higher face kills: the object's side reaches the inner box first.
- Because the step is found at its exact moment within the leg, the answer only depends on where the face is **when the leading side reaches it**: climbing a slope toward a step, the player isn't killed by an earlier part of the tick where it was a few pixels lower, and isn't saved by a later one.
- A step into a surface doesn't happen (`lift_hits_surface`: the circle would go past the corridor's ceiling, or the ground when flipped, before the square reaches the face): surfaces can't be crossed and never kill, so the player simply isn't lifted, and the object's side meets the inner box as for any face too high.

**Deaths along a leg** (`leg_deaths`). Before each leg is moved, two touch sweeps run over it, from 0 to the leg's event:

- the **inner box** (40 × 40, never rotated) against every neutral candidate (including the ones passed into as walls: that's how a wall kills), and never against the ground or the corridor boundaries (4.3: surfaces never kill);
- the **rigid square** against every harm object (4.5).

The earliest overlap kills: the player is moved to the exact point where it starts, `alive` becomes false, and the tick's movement ends there. Interactive objects touched on that leg before the death point still count as touched, but step 5 doesn't run for a dead player (3.4), so nothing acts. "Swept" matters at high speed: at 4× the player moves 12.5 px per tick, and a thin wall or spike must not slip between two positions.

**The jump zone.** `update_can_jump` runs at the very end of the tick (3.4), after interactive objects have acted: it tests the jump zone (4.3) against the neutral candidates and surfaces at the final position, and combines it with the momentum condition. The result is the next tick's `can_jump`.

### 4.5 Harm contacts

Harm hitboxes are tested with the **rigid square** (100 × 100, never rotated), swept along each leg as it happens (`leg_deaths`, 4.4), steps' lifts included: a fast player can't pass through a thin spike between two ticks, and a spike touched on the way to a landing still kills. Grazing isn't touching (3.0): a spike whose hitbox edge exactly meets the square's edge doesn't kill.

```c
/* The earliest death on this leg, on [0, t_end]: the inner box against neutral objects,
   the rigid square against harm. Moves the player there and kills it. */
static bool leg_deaths(sim_t *s, vec2_t d, double t_end)
{
    player_t *p = &s->st.player;
    const mode_ops_t *m = &MODES[p->mode];
    double death = INFINITY;
    double t;

    for (size_t k = 0; k < s->nb_cand; k++) {
        const object_t *o = &s->lvl.objects[s->cand[k]];

        if (OBJ_CATEGORY[o->type] == CAT_NEUTRAL)
            t = sweep_box_touch(p->pos, m->inner_half, d, &o->hitbox);   /* INFINITY: no touch */
        else if (OBJ_CATEGORY[o->type] == CAT_HARM)
            t = sweep_box_touch(p->pos, m->half, d, &o->hitbox);
        else
            continue;
        if (t < death)
            death = t;
    }
    if (death > t_end)
        return false;
    advance(s, d, death);                        /* the player stops exactly where it died */
    p->alive = false;
    return true;
}
```

### 4.6 Interactive contacts

Interactive objects are tested with the **rigid square** too, along each leg as it happens (`leg_touches`, 4.4, steps' lifts included), with the same touch sweeps (area overlap). `leg_touches(s, d, t_end)` is `leg_deaths`' twin: it sweeps the rigid square along `[0, t_end]` against every candidate whose category is `CAT_INTERACTIVE` and whose bit isn't set in `spent`, and appends `(index, leg + t)` to the touch list. The list keeps each live interactive object touched during the tick with its path position (`leg + t`). Step 5 of the tick applies them in that order, only if the player is still alive:

```c
static void apply_interactive(sim_t *s)
{
    qsort(s->touch, s->nb_touch, sizeof(touch_t), touch_cmp);   /* (at, index) */
    for (size_t k = 0; k < s->nb_touch; k++) {
        if (!s->st.player.alive)
            return;
        if (!is_spent(&s->st, s->touch[k].index))
            touch_interactive(s, s->touch[k].index);            /* 5.1 */
    }
}
```

`touch_cmp` compares `at` first and the object index second, so the order never depends on `qsort`'s stability. The spent check is repeated here because an object can be spent by an earlier one in the same tick (two portals overlapping). Sorted by `(path position, index)`, each one that wants to act (5.1; an orb only while the hold is still fresh, and it uses the hold) acts once and becomes spent. Because they're swept, a portal is never skipped by a fast player, and because they're applied after the move, their effects (mode change, corridor, a pad's impulse) start on the next tick.

### 4.7 Kill ceiling

With normal gravity, whatever goes up comes back down. With **flipped** gravity (FEATURES 9), a player outside a corridor with nothing above it "falls" upward forever. Without a limit, such a run never dies, and it even **completes the level** when the tick counter reaches `end_shift`. A camera-relative check doesn't work, because the camera follows the player upward.

The limit is fixed in the world, computed once at load:

```c
kill_y = min(highest hitbox top among all objects, GROUND_Y - CORRIDOR_MAX_HEIGHT)
    - KILL_CEILING_MARGIN;                                  /* margin 600 px */
```

```c
void collide_kill_ceiling(player_t *p, const level_data_t *lv)
{
    if (p->gravity_dir < 0 && p->pos.y - MODES[p->mode].half < lv->kill_y)
        p->alive = false;                       /* only exists in flipped gravity */
}
```

It exists **only while gravity is flipped**, in every mode. With normal gravity there's no kill line at all: a pad or an orb can send the player as high as it likes, and gravity brings it back. The ground is always solid, so no floor limit is needed.

Why 600 px: with flipped gravity, anything above the highest object has nothing left to land on, so the line only has to stay clear of what a flipped player can legitimately reach: a corridor's ceiling (below), and an upward-falling player passing just above the top of the level's highest structure on its way to landing on it from the side. 600 px leaves room for both without letting a lost run drag on.

Corridors are never above the kill line. The tallest corridor is 1000 px (5.2), so a centered corridor's ceiling is at most 525 px above its portal's top (500 px, plus up to 25 px of grid snapping), and the portal is an object, so its top is at or below the highest hitbox top: the ceiling stays at least 75 px under the line. A corridor pushed up against the ground has its ceiling at `GROUND_Y - CORRIDOR_MAX_HEIGHT` = −150, which is why that value is part of the `min`: without it, a level whose highest object is a portal at y = 650 would put the kill line at y = 50, far below that corridor's ceiling, and the ship would die touching it.

The renderer doesn't draw the line (GD has no visible ceiling), but the debug overlay does (9.6).

---

## Phase 5: Gamemodes, portals, ship corridor

### 5.1 Interactive objects: one activation, then untouchable

Today `check_portal` runs every frame the player overlaps a portal (about 16 frames at the current speed: 100 px portal + 100 px player, divided by 12.5). Each of those frames re-runs the mode change, which causes F3's leak and F4's jump lock.

The fix is a rule for a whole family of objects, the same one Geometry Dash uses. Objects fall into three categories (`OBJ_CATEGORY`, 3.3; a fourth, **trigger**, is reserved for later: see the naming note below):

| Category | Objects | Collision |
|---|---|---|
| Neutral | block, slope, ground, corridor | swept contact every tick: land and slide, head hit, wall (4.4) |
| Harm | spike (saw later) | any contact kills, every tick (4.5) |
| **Interactive** | portal (mode) now; pad, orb, gravity portal, speed portal later (FEATURES 10) | **acts once**, then **untouchable** (4.6) |

The interactive rule:

1. An interactive object has an **activation hitbox** (`o->hitbox`, computed at load like every other hitbox, rotation included).
2. When the player's rigid square touches it during a tick (and, for orbs, with a **fresh hold**, 3.4), it **acts on the player once**, on exactly that tick: mode change, launch, gravity flip, speed change. Portals don't push the player at all (they're ghosts: no contact response, just their effect); pads and orbs apply a vertical impulse that sets the rise speed.
3. It then becomes **spent**: its bit is set in the run state's `spent` bitset (3.3). It stays in the level and is still drawn, but it has no hitbox for the rest of the attempt. The collision pass skips it before any overlap test.
4. `sim_reset` clears the bitset, so each attempt starts with all interactive objects live. Practice checkpoints save and restore it as part of the run-state snapshot (Phase 13).

Why this is the right model:

- **Correct by construction.** The effect can't re-fire, whatever the effect does to the player (launch, gravity flip, mode change). F3's re-firing leak and F4's lock can't come back, and no interactive-object code has to guard against being called twice.
- **Less work per tick.** Today the portal code runs about 16 times per crossing; now it runs once, and a spent interactive object costs a single flag check instead of a rectangle test plus the effect. It matters most for pads and orbs placed in rows, and the culling cursor (4.1) already keeps the number of candidates small.
- **One rule for every future interactive object.** A new interactive type only needs an activation condition and an effect. "Once per attempt", reset and checkpoint behavior are inherited.
- **Predictable levels.** A level designer knows a portal or pad acts exactly once, where the player first touches it. That's how GD levels are built.

```c
static bool interactive_wants_activation(const sim_t *s, const object_t *o)
{
    (void)s;
    switch (o->type) {
    case OBJ_PORTAL:
        return true;                    /* on touch */
    /* later: OBJ_PAD, OBJ_GRAVITY, OBJ_SPEED -> true (on touch)
              OBJ_ORB -> the player's hold is HOLD_FRESH (touch + fresh hold, FEATURES 10.2) */
    default:
        return false;
    }
}

static void interactive_act(sim_t *s, const object_t *o)
{
    switch (o->type) {
    case OBJ_PORTAL:
        enter_portal(s, o);             /* 5.2 */
        break;
    default:
        break;
    }
}

/* called by apply_interactive (tick step 5) for each touched object, in contact order */
static void touch_interactive(sim_t *s, size_t i)
{
    const object_t *o = &s->lvl.objects[i];

    if (!interactive_wants_activation(s, o))
        return;                         /* e.g. an orb touched without a press */
    interactive_act(s, o);
    set_spent(&s->st, i);               /* keeps its sprite, loses its hitbox */
}
```

An orb the player flies through without a fresh hold isn't spent: its hitbox stays live, so a fresh press later in the overlap still works. It becomes spent only when it actually acts, and acting uses the hold.

**Orbs act on the tick of the contact, never before or after** (FEATURES 10.2):

- if the player already overlaps the orb at the start of the tick with a fresh hold (for example, the button is pressed while inside it), the orb acts at step 0, **before** the surface jump of step 1, which it therefore replaces (orb priority);
- if the rigid square first touches the orb during the tick's movement while the hold is still fresh, the orb acts in step 5 of that same tick, in path order with the other interactive objects. If the player already jumped off a surface in step 1 of this tick, that jump used the hold, and the orb doesn't act.

**Naming.** "Interactive" is deliberate: in GD, **triggers** are a different, more advanced kind of object (they move groups of objects, change colors, toggle things, usually fired by the player crossing their x position, and never drawn in play). They are out of scope here, but if the project gets there, `OBJ_TRIGGER` / `trigger_*` stay free for them, and they'd get their own rules rather than reusing `spent`.

Triggers act on **groups**: an object that a move trigger should move declares that group in its own line (for example a `group=` field, 7.2 reserves the syntax). The exact mechanism is to be designed when triggers are, and it's deliberately not specified here. Two things it will have to deal with, so they aren't a surprise: moving objects break Principle 1 (level data would need per-group offsets in the run state), and the x-sorted broadphase of 4.1 and the chunks of 9.2 assume objects never move (moving groups need their own list).

### 5.2 Entering a portal

```c
static void enter_portal(sim_t *s, const object_t *o)
{
    run_state_t *st = &s->st;
    player_t *p = &st->player;
    const mode_ops_t *m = &MODES[o->portal_mode];

    p->mode = o->portal_mode;
    if (m->corridor_height > 0.0)
        center_corridor(s, o);                  /* also for a same-mode portal: new corridor */
    else
        st->bounds.active = false;
}                                               /* vy is kept as is: momentum carries over */

static void center_corridor(sim_t *s, const object_t *portal)
{
    double center = portal->rect.y + portal->rect.h / 2.0;
    ship_bounds_t *b = &s->st.bounds;

    double height = MODES[s->st.player.mode].corridor_height;   /* per mode (below) */
    double top = center - height / 2.0;

    top = floor(top / UNIT + 0.5) * UNIT;       /* snap to the nearest grid line; ties go down */
    if (top + height > GROUND_Y)                /* never below the ground */
        top = GROUND_Y - height;
    b->active = true;
    b->top = top;
    b->bottom = top + height;
    s->st.cam.pos.y = top - (VIEW_HEIGHT - height) / 2.0;   /* camera locked on it (3.5) */
}
```

- **A mode portal never moves the player.** It only changes the gamemode (and with it the corridor): same position, same `vx` and `vy`, same gravity, same hold. A player entering a portal near its top edge continues from exactly there; it isn't pulled to the portal's center or anywhere else. What happens afterwards is the new mode's own logic: a wave, for example, sets its 45° motion from the next tick (FEATURES 8). (Today, a ship portal teleports the player by 250 px, F3, because the world is shifted twice: a bug of the old implementation.)
- **Corridor**: as tall as the new mode says (`corridor_height`, 3.3), centered on the portal that created it, then **snapped to the grid**: its top moves to the nearest multiple of 50 (on an exact tie, the lower one, `floor(v + 0.5)`, so the result doesn't depend on the sign), which puts the bottom on the grid too. Level designers can then line blocks up with the corridor's surfaces exactly. Finally it's pushed up if needed so it never goes below the ground (how GD does it).
- **Heights are GD's**, converted to our units (1 GD block = 1 player = 100 px, FEATURES 6.9): **ship, UFO and wave 1000 px** (10 blocks), **ball 800 px** (8), and later spider 900 and swing 1000. The cube (and later the robot) has no corridor at all. Every height is a multiple of 50, so the grid snapping keeps both surfaces on grid lines.
- Examples with a 1000 px corridor: a 200 px ship portal at y = 250 has its center at 350, top 350 − 500 = −150, already on the grid: corridor −150..850, its floor exactly on the ground. One at y = 650: center 750, top 250, bottom 1250 is below the ground, so it's pushed up to −150..850 too. A ball portal (800 px) at y = 250: top 350 − 400 = −50: corridor −50..750.
- Its ceiling and floor are neutral surfaces in the sweep (4.3): the ship bounces off the ceiling like off a block's underside, and lands and slides on the floor.
- **The camera is locked** in y on the corridor, centered on screen (its whole height visible, 40 px of margin for a 1000 px one), for as long as the player is in it, and doesn't follow the player inside it. It's released when the player leaves the corridor (a portal to a mode without one), and then eases back to following (3.5).
- **Old boundaries vanish instantly.** When a portal removes the corridor or replaces it (a same-mode portal), `bounds` is overwritten at that moment (tick step 5): from the next tick on, the old boundaries have no collision at all, so a player can't be stopped, bounced or hurt by the corridor it just left, even if it's still touching or past where they were. Only their drawing fades out (5.3). Like every surface, the boundaries never kill anyway (4.3).
- **Same-mode portals** run the same code: a ship portal reached in ship mode **switches to the new corridor**, centered on the second portal. This is the rule for every mode with a corridor (FEATURES 6.6), and Appendix D's `ship_portal` level tests it.
- A rotated portal still opens a horizontal corridor: the corridor uses the portal's **rect**, not its rotation.
- The player texture change (cube icon / ship icon) happens in the game layer: it draws the texture that matches `player.mode`. The sim has no textures.

### 5.3 Ship corridor rendering

Draw the ceiling and floor as tiled strips of the block texture, in world space (the texture set repeated once at load with `sfTexture_setRepeated(tex, sfTrue)`):

```c
static void draw_strip(gd_t *gd, float cam_x, float y)
{
    sfSprite *s = gd->strip_sprite;       /* its own sprite, block texture, repeated */
    float x = floorf(cam_x / 100.0f) * 100.0f;

    sfSprite_setTextureRect(s, (sfIntRect){0, 0, (int)VIEW_W + 200, 100});
    sfSprite_setPosition(s, (sfVector2f){x, y});
    sfRenderWindow_drawSprite(gd->w, s, NULL);
}

void render_ship_bounds(gd_t *gd, const sim_t *sim, float cam_x)
{
    const ship_bounds_t *b = &sim->st.bounds;

    if (!b->active)
        return;
    draw_strip(gd, cam_x, b->top - 100.0f);   /* the strip is 100 px thick */
    if (b->bottom < GROUND_Y)
        draw_strip(gd, cam_x, b->bottom);
}
```

Using a dedicated strip sprite (not the shared block sprite) avoids having to restore its texture rect after each use.

**Fading out.** The collision of an old corridor is gone instantly (5.2), but its strips don't just blink out: the game layer keeps the last boundaries it drew. When `sim->st.bounds` changes (removed, or replaced by another corridor), it moves the old ones to `gd->fading_bounds` with a timer, and draws them for 0.25 s with an alpha going linearly from 255 to 0 (`sfSprite_setColor`), under the new strips if there are any. It's purely visual: the sim never sees the fading strips. A death or a respawn clears them.

### 5.4 What the new engine changes for your levels

The engine is new, not a reproduction of today's collision code, so the 7 levels are re-verified rather than assumed:

- The **feel constants** are GD's own (11.1, FEATURES 6.9), not the legacy game's: the scroll speed is 1038.6 px/s instead of 750, and a cube jump reaches GD's 2.1333 blocks (213.3 px) in 0.425 s over 441 px, instead of 228.6 px in 0.617 s over 462 px. The 7 legacy levels were built around the old feel, so some of them will stop working; they're kept as parsing and regression material, and the real test levels are **rebuilt** on the new physics (Appendix D, FEATURES 13). Unit tests pin the constants (8.1).
- The **bot** runs on every level and prints whether it found a path (8.3, a warning, never a failure). The lab's prototype bot already completes all 7 levels with the rules closest to the new engine (no teleport, no jump lock, proportional spikes, previous-position block resolver, centered corridor, 240 Hz), which is a good sign; the real engine's run is the one that counts.
- **Play-test by hand** the spots where behavior changes the most:
  - level 6's ship section (portal at x = 24700, y = 250): no teleport, a centered corridor, and a ceiling that bounces instead of sliding;
  - level 7's ship section: the corridor is 1000 px tall and pushed against the ground (ceiling at world y −150 instead of 350), with the camera locked on it, centered on screen;
  - level 2's size-1 spike (proportional hitbox);
  - every place where a cube hits a block's underside: it dies now, as in GD;
  - landings near block edges and running into block sides: the inner box (40 × 40) now decides deaths, and steps up to 30 px are stepped onto (4.4). Today's rule used a shrunken 80 × 90 box.

### 5.5 Unknown portal modes

The loader rejects `portal ... <mode>` unless the mode is a name in `MODES` (`cube`, `ship`; FEATURES adds more), with a message naming the file and line. Today a typo leaves `gamemode` uninitialized (random behavior).

---

## Phase 6: Death, respawn, attempts, progress

### 6.1 Death and respawn (game layer)

```c
void level_on_death(level_t *lv, gd_t *gd)
{
    level_record_best(lv);                   /* 6.2: the session's best and popup */
    lv->state = LEVEL_DYING;
    lv->death_ticks = DEATH_DELAY_TICKS;
    lv->death_pos = lv->sim.st.player.pos;   /* where to draw the explosion */
    sfMusic_stop(gd->musics.level);
    sfSound_play(gd->sfx.death);
}

void level_respawn(level_t *lv, gd_t *gd)
{
    sim_reset(&lv->sim);
    lv->state = LEVEL_PLAYING;
    lv->accumulator = 0;
    sfClock_restart(lv->clock);
    level_count_attempt(lv);
    sfMusic_play(gd->musics.level);          /* sfMusic_play on a stopped music restarts it */
}
```

Compare with today's `player_dead`: it frees `portal_blocks`, undoes the accumulated shift of every object, resets the player, and relies on `reset_attempt_display` to reset the gamemode. Four places that must agree. Now the whole reset is `sim_reset`, which touches no object positions because none ever moved.

The half-second delay is where the unused assets go: `res/explos.png` is a 330×110 strip of three 110×110 frames. Draw frame `(DEATH_DELAY_TICKS - death_ticks) * 3 / DEATH_DELAY_TICKS` at `death_pos` and hide the player; play `explode_11.ogg` (subject to Phase 12).

The death delay (and the explosion) is a deliberate change from the legacy game, which respawns instantly. It lives entirely in the game layer, so it doesn't affect the sim, the replays or the bot.

Music today restarts through a side effect (`if (level->shift == 0) sfMusic_play(...)` in `print_level`, true on the first frame of each attempt). Make it explicit as above.

### 6.2 Attempts and best: in memory, written once

Progress belongs to the session while you play, and to the file only when you leave the level. Writing `save/progress.txt` after every death would mean a `write` + `fsync` + `rename` every few seconds, for a number nobody reads until the level ends.

So the level keeps its own counters and updates the store in memory; the file is written when the level is left:

```c
typedef struct level_stats {        /* the session's numbers, in level_t */
    int attempts;                   /* this session only, for the on-screen text */
    float best;                     /* the best of this session                  */
    float practice_best;
    bool dirty;                     /* something to write when leaving           */
} level_stats_t;

void level_count_attempt(level_t *lv)
{
    lv->stats.attempts += 1;
    lv->stats.dirty = true;
    level_show_attempt_text(lv);
}

void level_record_best(level_t *lv)
{
    float pct = sim_percent(&lv->sim);
    float *best = lv->practice ? &lv->stats.practice_best : &lv->stats.best;

    if (pct <= *best)
        return;
    *best = pct;
    lv->stats.dirty = true;
    if (!lv->practice)
        level_show_new_best(lv, pct);        /* "New best! 47%" popup, game layer */
}

/* The one place that touches the store and the file (6.4). */
void level_flush_stats(level_t *lv, gd_t *gd)
{
    progress_entry_t *pe;

    if (!lv->stats.dirty)
        return;
    pe = progress_get(&gd->progress, lv->id);
    pe->attempts += lv->stats.attempts;
    pe->best = fmaxf(pe->best, lv->stats.best);
    if (lv->stats.practice_best > pe->practice_best)
        pe->practice_best = lv->stats.practice_best;
    if (pe->best == lv->stats.best)
        pe->level_hash = lv->file_hash;      /* best achieved on this version */
    lv->stats = (level_stats_t){0};
    progress_save(&gd->progress);
}
```

`level_count_attempt` is called from `level_start` (first attempt), `level_respawn` and `level_restart` (6.5). `level_record_best` runs on death and on completion.

**When the file is written**, and only then:

| Event | Session numbers | File |
|---|---|---|
| Death | attempts + 1, best updated | — |
| Completion | best 100 | — |
| Restart key (6.5) | attempts + 1 | — |
| Back to the level list (end screen, pause, Escape) | flushed | **written** |
| Quitting the game (Quit button, window closed) | flushed | **written** |
| Practice mode (13) | `practice_best` only, no popup | on leaving, like the rest |

The game layer has exactly two call sites: `level_free` (every way out of a level goes through it) and the quit path, which frees the level before closing the window. A crash or a `kill -9` loses the session's numbers, which is the price of not writing every few seconds; nothing else is at risk, since the store is only ever written whole (6.4).

**The list stays right** while you play, because it reads the in-memory store, and the level you just left flushed into it before the list appeared.

### 6.3 Completion

```c
void level_on_complete(level_t *lv, gd_t *gd)
{
    (void)gd;
    level_record_best(lv);                   /* sim_percent is 100 here */
    lv->state = LEVEL_COMPLETE;
    lv->end_screen = create_end_level_screen(lv, gd);
}
```

The end screen shows the session's attempts and the best from the store combined with the session's, so the numbers on screen match what will be written when you leave.

### 6.4 The progress store (replaces `rewrite_level`)

Problems with storing progress in the level files (the first line, rewritten by `rewrite_level`):

- **Content and save data mixed.** Level files are tracked in git; every play session modifies them. Your committed levels contain attempt counts (`level1` says 147).
- **Precision loss** (F6).
- **Wrong best on quit.** Escape mid-run saves the current percentage as best, although the attempt neither died nor finished.
- **Identity by number.** The list shows files by name but the game loads `levels/level%d` using the number in the header (F7).
- **Unsafe save.** `sprintf(temp, "%s.temp", filename)` can overflow (gcc warns), and `remove()` before `rename()` opens a window where the level file doesn't exist; `rename()` alone replaces the target atomically on POSIX.

New module `src/sim/progress.c` (pure C, so it's unit-testable):

```c
#define SAVE_DIR   "save"
#define SAVE_PATH  "save/progress.txt"

typedef struct progress_entry {
    char id[24];            /* the level file's digits, e.g. "10280" (7.2) */
    int attempts;
    float best;
    float practice_best;    /* Phase 13: a stat only */
    uint64_t level_hash;    /* FNV-1a of the level file when `best` was set, 0 = unknown */
    char song[128];         /* FEATURES 4.6: player's song override, "" = none */
    char *extra;            /* unknown key=value fields, written back unchanged */
} progress_entry_t;

typedef struct progress {
    progress_entry_t *entries;
    size_t count;
    size_t cap;
    char path[256];
} progress_t;

int progress_load(progress_t *p, const char *path);    /* missing file = empty store */
int progress_save(const progress_t *p);                 /* 0 on success */
progress_entry_t *progress_get(progress_t *p, const char *id);   /* creates if missing */
void progress_free(progress_t *p);
bool progress_valid_id(const char *id);                 /* digits only, 1 to 18 */
```

File format, one line per level, `key=value` fields after the id:

```text
10280 attempts=33 best=47.83 practice_best=81.20 hash=9f2c41d07ab35e11
```

- Using `key=value` from the start (instead of positional numbers) means new fields (the song override of FEATURES 4.6, practice best, anything later) never need a format migration. Missing fields take defaults; unknown fields are kept in `extra` and written back, so an older build doesn't destroy a newer build's data.
- Load: read a line, take the id (first word), then split the rest on spaces and each field on the first `=`. Numbers with `strtof`/`strtol`, checking the end pointer.
- **The id is the level file's name without `.gd`**, digits only (7.2), so it's always safe in a path and in this space-separated format. A file in `levels/` that isn't `<digits>.gd` is skipped by the level list with a warning ("rename it to play it").
- Save: `mkdir(SAVE_DIR, 0755)` (ignore `EEXIST`), write `save/progress.txt.tmp`, `fflush`, `fsync(fileno(f))`, check that `fclose` returns 0 (that's when write errors surface), then `rename()` it over `save/progress.txt`. A crash or power loss mid-save leaves the old file intact (`fsync` makes sure the new content is on disk before the rename makes it visible).
- Load once in `create_gd`, free in `free_gd`.
- **Written only when a level is left** (6.2): the session's attempts and best live in `level_t` until then. Every path out of a level flushes them, so the file is written once per visit instead of once per attempt.

**Level versions.** `hash` records which version of the level the best was achieved on. The level list computes each level's current hash (FNV-1a over the file's bytes, a few microseconds per level) and, when it differs from the stored one, shows the best with an "edited since" marker. The best is kept: GD keeps progress on updated levels too, and an edit is often a decoration change.

**No migration.** The old counts lived in the level files' first line (7.2); those files were converted once by hand and the originals kept outside the repository. The store starts empty.

Where to put `save/`: next to the binary is simplest while the game runs from its source folder (Phase 1.4 item 6 makes that folder the working directory). If you ever install it, use `$XDG_DATA_HOME/my_gd/` (default `~/.local/share/my_gd/`).

### 6.5 Restart key

The restart key (default `R`, rebindable: FEATURES 2.2) is a voluntary death **without** the delay and explosion: record the best (the player did reach that percentage), count a new attempt, and respawn immediately. Like every other attempt, it only touches the session's numbers (6.2).

```c
void level_restart(level_t *lv, gd_t *gd)
{
    if (lv->state == LEVEL_PLAYING)
        level_record_best(lv);
    level_respawn(lv, gd);                   /* sim_reset, +1 attempt, music restart */
}
```

In practice mode, restart respawns from the **last checkpoint** instead of the start (Phase 13), and also counts an attempt.

---

## Phase 7: Level format, loader, level list

### 7.1 Loader problems today

- `load_object` calls `strcmp(arr[0], ...)` without checking `arr[0]`: an empty or whitespace-only line crashes.
- Missing fields (`spike 1000 750`) read past the end of the token array.
- `my_str_to_word_array` and `my_str_word_array_delim` allocate `strlen(str)` pointers; a 1-character line without newline needs 2 (word + `NULL`): heap overflow.
- A missing file leaves `objects` uninitialized, and the first frame dereferences garbage.
- Only `level3` ends with a newline; the other six don't. The loader happens to cope, but the new one must be tested for it.

### 7.2 Format

A level file is **`levels/<id>.gd`**, and its name is the level's **id: digits only** (`levels/10280.gd`). The id is what the progress store keys on (6.4) and what the level list sorts by; nothing inside the file repeats it. A file in `levels/` whose name isn't digits + `.gd` is not a level: the list skips it with a warning, which also keeps `.temp` files and editor leftovers out (7.5).

The file is a **header** then a **body**:

```text
# comments and blank lines are ignored, anywhere
name Stereo Madness
author Alexnex
version 2

block 1000 200 1
spike 3000 750 2
spike 3400 0 2 rot=180            # ceiling spike
block 5000 650 2 w=8 h=1          # 400 x 50 platform
slope 6000 750 2                  # 100 x 100, 45 deg, rising to the right
slope 6100 750 2 w=4 h=2          # 200 x 100: a 26.6 deg slope
portal 2100 750 2 ship
```

**Header.** Every line whose first word isn't an object type is a header field: `key <rest of the line>`. There's no separator and no fixed order; by convention the header sits at the top. Fields:

| Key | Meaning | Default |
|---|---|---|
| `name` | the level's prose name, shown in the list and on the end screen | the id |
| `author` | who made it | empty |
| `version` | the format version this file was written for | 2 |
| `song` | a file in `res/songs/` (FEATURES 4.5) | empty: the menu default |
| `offset` | seconds of song to skip at the start (FEATURES 4.4) | 0 |
| `bpm`, `first_beat` | the editor's beat grid (FEATURES 11.10) | 0 |

An unknown key is a warning and is ignored, so a file written by a newer build still opens; the editor keeps those lines verbatim when it saves (FEATURES 11.1).

**Player progress is never in the level file**: attempts, best and practice best live in `save/progress.txt`, keyed by the id (6.4). A level file only describes the level, so editing or sharing one never touches anyone's records, and the game never writes to `levels/`.

**Body.** One object per line: `type x y size [word] [key=value ...]`.

- Types: `block`, `slope` (a right triangle rising left to right, 4.2; `rot=` turns it into any other orientation, e.g. `rot=90` for a ceiling slope), `spike`, `portal` (whose extra word is the gamemode).
- `x` and `y` are world pixels, any sign; `size`, `w` and `h` are grid units of 50 px.
- `#` starts a comment anywhere on a line.

Optional `key=value` fields, any object:

| Field | Meaning | Default |
|---|---|---|
| `rot=<degrees>` | rotation, any angle, clockwise, around the rect's center (4.4) | `0` |
| `w=<units>` | width in grid units (50 px) | `size` |
| `h=<units>` | height in grid units | `size` |
| `group=<n>[,<n>...]` | reserved for GD-style triggers (5.1): parsed, stored nowhere yet, one warning per level | none |

Why width and height: with `size` alone, every object is a square, so a "wide platform" of 8 units is also 8 units tall (400 × 400 px), sinking far below the ground. `w`/`h` make real platforms and walls one line each, and keep the long-block case (F5) meaningful.

Why `rot=` instead of `up|down|left|right`: an angle covers the four directions (`0`, `90`, `180`, `270`) and everything in between, for every object type, with one field. Hitboxes follow the angle (4.4).

Sizes: `size`, `w` and `h` must be at least 1. Zero or negative rejects the line (a zero-size object would be invisible and touch nothing, so it's always a mistake). There's no maximum.

**No migration.** The legacy format (a first line of `id attempts best`, files named `levelN`) is not read at all: the seven levels that existed were converted once by hand, and the old copies are kept outside the repository. A legacy file loads as a level whose first line is an invalid object, with one warning.

### 7.3 Parser

Numbers are parsed with `strtod`/`strtol` and the end pointer checked, never with a bare `sscanf`, because `sscanf("%d")` reads `2.5` as `2` and leaves `.5` for the next field, silently accepts trailing junk, and `%f` accepts `nan` and `inf`, which would poison sorting (a comparator with NaN isn't a valid ordering, and `qsort` then misbehaves) and every collision test.

```c
static bool parse_double(const char *s, double *out)
{
    char *end;
    double v;

    errno = 0;
    v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE || !isfinite(v))
        return false;
    *out = v;
    return true;
}

static bool parse_int(const char *s, int *out)
{
    char *end;
    long v;

    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || v < INT_MIN || v > INT_MAX)
        return false;
    *out = (int)v;
    return true;
}

/* line is modified in place (split into words); returns 0, or -1 to skip the line */
static int parse_object(char *line, int lineno, object_t *o, sim_log_fn log)
{
    char *tok[16];
    int n = split_words(line, tok, 16);         /* spaces/tabs, stops at '#' */
    const char *word = NULL;
    int first_field = 4;
    double x;
    double y;
    int size;

    if (n < 4 || !parse_double(tok[1], &x) || !parse_double(tok[2], &y) ||
        !parse_int(tok[3], &size) || size <= 0)
        return -1;
    if (n > 4 && strchr(tok[4], '=') == NULL) {
        word = tok[4];                          /* portal mode, pad kind, ... */
        first_field = 5;
    }
    *o = (object_t){.line = lineno, .size = size,
        .rect = {x, y, size * UNIT, size * UNIT}};
    for (int i = first_field; i < n; i++)
        if (parse_field(o, tok[i], lineno, log) != 0)
            return -1;                          /* e.g. w=-2 or rot=abc */
    return object_init(o, tok[0], word);        /* type, extra word, hitbox (4.2) */
}
```

The type is checked first, before anything else on the line, because that's what tells a header field from an object. `object_init` then rejects a missing or unexpected extra word (`portal` needs a mode, `block`, `slope` and `spike` take none) and an unknown portal mode. `parse_field` handles the table in 7.2; an unknown key logs a warning and is ignored (not the whole line), so newer files open in older builds as far as possible.

Loader (`sim_load` reads the file into memory, then `sim_load_mem` does the work):

1. `fopen("levels/<id>.gd")` and read it whole. On failure, log and return -1; the game goes back to the level list with a message instead of crashing.
2. Walk the lines, strip `\r\n` (files edited on Windows have `\r`), skip blank lines and comments. A last line without a newline is a normal line.
3. A line whose first word isn't an object type is a header field (7.2): store the known keys, warn once for an unknown one.
4. `parse_object`; on failure log `levels/10280.gd:12: invalid line, skipped` and continue.
5. Two passes over the buffer: the first counts the lines, the second fills one allocation of that size (the object array ends up exactly as long as the number of valid objects). One allocation, no realloc, and the same result whatever the file.
6. After reading: sort by `(hitbox.aabb.x, line)` with an explicit comparator (`(a > b) - (a < b)`, never a float subtraction cast to int), then compute `reach`, `end_shift` (3.4), `kill_y` (4.7), allocate the `spent` bitset (`(n + 63) / 64` words) and call `sim_reset`. A level with no object is legal: `reach` 0, `end_shift` 100, and `kill_y` from the corridor constant alone.

### 7.4 A `--check` mode

Add `./my_gd --check levels/10280.gd`: loads the level with the sim only (no window), prints warnings, and runs the bot (Phase 8).

Warnings: invalid lines, identical objects on top of each other, objects below the ground, neutral surfaces steeper than 50° facing up where the player could land (they act as walls, 4.4), unknown fields, more than `MAX_CANDIDATES` objects within one tick's reach (4.1), and anything past the end.

The bot's result is **information, never a failure**. The bot is a heuristic search with a time limit, so "no path found" can be wrong; it prints `bot: no path found (furthest 83%) - check it by hand` and moves on. You decide whether it matters.

Exit code: 1 if the file can't be loaded or has invalid lines; 0 otherwise, whatever the bot says. This makes level-making safer without ever blocking you, and CI can run it on every level.

### 7.5 Level list

Problems today: arbitrary order (F7); the directory is read twice (`count_levels` then `fill_names_list`), and if they disagree `level_buttons` contains uninitialized pointers that get freed later; `.temp` files would appear as levels; best is formatted by a hand-written `float_to_str`; the play button's hit area is a 100×100 square on a 104×123 texture; with 10+ levels the grid (3 columns, rows every 250 px from y = 200) runs off the bottom of the screen (the 10th level's button lands at y = 1050, the 13th off-screen).

Fixes:

- Read the directory once into a growable array, keeping only names that are `<digits>.gd` (7.2) and warning about the rest, which also skips `.temp` files and editor leftovers.
- Sort by the id as a **number**, so 2 comes before 10 with no natural-order comparator to write.
- Identify levels by that id (`gd->selected_level_id` is a `char *` of digits); load `levels/<id>.gd`.
- Show the level's `name` line (read with the loader, header only), and attempts and best from the progress store, with the "edited since" marker (6.4).
- Format with `snprintf(buf, sizeof(buf), "Best: %.2f%%", best)`.
- Hit-test with the sprite's real bounds: `sfFloatRect_contains(&bounds, x, y)` with `bounds = sfSprite_getGlobalBounds(sprite)`.
- Pages: 9 levels per page, Left/Right arrows or the mouse wheel to change page, and a "2 / 3" page indicator.
- Keyboard navigation (arrows + Enter) is cheap to add once there's a "selected index".

---

## Phase 8: Testing and CI

This is the phase that makes every other phase safe. It's also where the split from Phase 2 pays off: all tests below link only `src/sim/`, with no CSFML, window or audio.

### 8.1 Unit tests

Plain C with a tiny assert macro (or Criterion if you already use it):

```c
#define CHECK(cond) do { if (!(cond)) { \
    fprintf(stderr, "%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
    failures++; } } while (0)
```

What to test (levels are given as strings to `sim_load_mem`, no temp files):

- **Parser:** blank lines, comments (full-line and after an object), `\r\n`, missing fields, unknown types, bad portal modes, negative coordinates, no trailing newline, legacy header detection, `name` line. Rejected: `size` `0`, `-1`, `2.5`, `nan`, `inf`, `1e99`, `w=0`, `h=-2`, trailing junk (`750x`). Accepted: `rot=`, `w=`, `h=`, `group=` (ignored with one warning), an unknown field (ignored, line kept).
- **Progress:** the file is written once per visit, not per attempt: 20 simulated deaths leave `save/progress.txt` untouched (same mtime and bytes), and leaving the level writes `attempts=20` once; save → load round trip keeps `47.83` and `practice_best`; an unknown `key=value` survives a load/save cycle; a missing file gives an empty store; `progress_get` creates entries; invalid ids are refused; a failed save leaves the previous file untouched.
- **Collisions**, with hand-built objects: land on a block from above; die hitting its side; ship slides under; spike hitbox edges (1 px inside kills, 1 px outside doesn't); the wide-block case (F5, now `w=8 h=1`); the 100 px gap under a wall (level 5).
- **Broadphase:** a 45°-rotated block (its hitbox bounds stick out of its rect) still stops a player at its corner; every test level gives the same results with the broadphase and with a brute-force loop over all objects (compare `sim_state_hash` every tick).
- **Rotation:** blocks rotated by 90°, 180°, 270° have bit-exact axis-aligned hitboxes (level 5's gap still passes when its blocks are written with `rot=90`); a spike at `rot=180` on the ceiling kills a player touching its tip from below and not one passing 1 px under it; a 45° spike: a box touching the bounding box's empty corner survives (the separating-axis test), a box touching the diagonal edge dies; a block at `rot=30` is a real tilted shape: landing on its top face slides along a 30° slope.
- **Kill ceiling:** with flipped gravity, a player whose top goes 1 px above `kill_y` dies; with normal gravity, a pad launching the cube 2000 px above the highest object doesn't kill it (it comes back down); a flipped ship against the ceiling of a corridor opened by the highest object never touches the line.
- **Speed limits:** the jump is exactly 2027.6 px/s (1.9522 units) on flat ground, up a slope and down a slope; gravity never accelerates past `max_fall`, but a player already falling faster keeps its speed; a held ship settles at exactly 2326.5 px/s rising, a released one at 2326.5 px/s falling; a ship launched by a red orb at 2783.5 px/s keeps rising faster than 2326.5 px/s, holding doesn't add to it, and gravity slows it down; a cube climbing a steep slope at 4× speed isn't slowed by any cap.
- **Interactive objects:** a portal acts once per attempt (count mode changes while crossing: exactly 1); after acting its bit is set and the sweep never collects it again (count `interactive_act` calls); `sim_reset` clears the bitset; a portal never changes the player's position or speed except the mode's speed limits; a second ship portal while in ship mode moves the corridor to the second portal. Corridor bounds for a **ship** portal at y = 250 (200 px tall) are −150 and 850 (centered at 350, 1000 px tall → −150..850); for one at y = 650 they're −150 and 850 too (250..1250, then pushed above the ground); a **ball** portal (800 px) at y = 250 gives −50..750. While the corridor is active, `cam.pos.y` equals `top - (1080 - height) / 2` on every tick whatever the player does; after a cube portal it follows again.
- **Physics constants:** a jump from flat ground has apex 213.32 ± 0.05 px (GD's 2.1333 blocks, 11.1) and lasts 102 ticks (0.425 s), counting from the tick whose input starts the jump to the first tick with `grounded` true again, and covers 441.4 px. (The legacy lab tool reports 38 frames at 60 Hz for the same jump: it counts from the frame before the input is applied.) Each trigonometric literal in `constants.h` matches its formula computed in double, and `CAM_LERP` matches `TICK_RATE` and `CAM_TAU`.
- **Determinism:** run the same input script twice and compare `sim_state_hash` after every tick. Snapshots: save at tick 500, run 300 ticks, restore, run the same 300 ticks: identical hashes.
- **Fuzzing:** `tests/fuzz/fuzz_parser.c` feeds random bytes to `sim_load_mem` and runs 200 ticks with alternating input. Built with clang's libFuzzer and ASan/UBSan (`make fuzz_parser`), seeded with every file in `levels/` and `tests/levels/`. The parser reads files people download and edit by hand, so it's the most exposed code in the game; a fuzzer finds the crashes unit tests don't think of.

```c
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    sim_t s;

    if (sim_load_mem(&s, (const char *)data, size, "fuzz", NULL) != 0)
        return 0;
    sim_reset(&s);
    for (int t = 0; t < 200; t++)
        sim_tick(&s, (input_t){t % 20 < 10, t % 20 == 0});
    sim_free(&s);
    return 0;
}
```

### 8.2 Engine tests

The collision engine gets its own tests, each a tiny level given as a string, a scripted input, and an expected outcome. These are the cases where engines usually break:

- **Seams:** running across 20 adjacent blocks at the same height, and across a block next to a slope: no death, `grounded` every tick. Same with the blocks written in random order in the file.
- **Exact gap:** level 5's 100 px gap under a wall: passes, grazing both surfaces without overlapping them.
- **Touching isn't a collision:** a spike whose hitbox edge exactly meets the rigid square's edge doesn't kill; a portal only grazed by an edge doesn't activate; a block whose side exactly meets the inner box's side doesn't kill (one more tick of movement into it does); a block exactly on the next tile above or in front of the player doesn't collide.
- **Containment:** a player spawned entirely inside a block dies on the first tick (the block contains the inner box); a spike entirely inside a portal's hitbox still kills; a 10 × 10 block placed entirely inside the inner box's area kills.
- **Tunneling:** at 4× speed (`speed_mult = 4`), a 10 px wide spike and a 10 px wide wall are still hit; a portal 5 px wide is still collected.
- **Slopes:** climbing a 45° slope at 1× and 4× speed keeps `vx` exact and `grounded` every tick; going down it keeps `grounded` every tick, including faster than the fall cap (a UFO at 2×, a ship or ball at 3×, a cube at 4× on a 45° slope: gravity goes one tick past the slope's speed, 3.4); running off a flat ledge onto a surface 2 px lower falls with gravity alone (no snapping down); the top of an uphill slope launches the player with `vy = vx` (45°); a 60° surface is a wall (death).
- **Shapes and roles:** running into a block's side: the square enters it and the player dies exactly when the block reaches the inner box (30 px of penetration, tested at 1× and 4×); clipping a block's corner with the square (the inner box untouched) survives; a spike touching only the square's corner kills; the inner box and the circle ignore spikes and portals.
- **Step-up:** a block 25 px above the feet: the player is lifted onto it on the tick the inner box's leading side passes its edge (not before: at 29 px of penetration it's still inside the square's corner); exactly 30 px above the feet (the face exactly on the inner box's bottom edge, the block in the jump zone): climbed; a block whose bottom face is exactly on that line, above it: death; 35 px: the inner box meets its side, death; climbing a slope into a step, the result only depends on the face's height at the moment the inner box's leading side reaches it (a step that is 29 px high at that moment is climbed even if it was over 30 px at the start of the tick, and the reverse kills); a 20 px step made of a `slope` rotated so its horizontal face is on top is climbed exactly like a block; at the top of a 45° slope running into a flat top, the player reaches the flat top alive, with a lift of at most 14.6 px (tested at 0.5×, 1× and 4× speed); the lift is a leg (a spike sitting on the step's top, touched only during the lift, kills; a block's underside run into by the circle during the lift kills in every mode, while grazing it exactly at the top of the lift doesn't; a lift that would reach a corridor ceiling doesn't happen).
- **Death stops everything:** a cube dying of a head hit mid-tick stays exactly at the contact point, with no step-up, no further legs and no interactive effect in that tick; a spike and a portal on the same leg, the spike first: the portal never acts.
- **Surfaces never kill:** a wave held against the corridor ceiling slides along it; with flipped gravity, released, it slides along the corridor floor; a flipped cube thrown down onto the ground stops and slides along it; a wave on the ground entering a cube portal ends up standing on the ground, alive; no surface is ever crossed (a fuzzed run of random inputs on every test level never triggers the `crossed_surface` safety net).
- **Boundaries vanish instantly:** a ship pressed against its corridor ceiling that takes a cube portal, and a ship that takes a second ship portal whose corridor is lower, are never stopped or bounced by the old boundaries, from the portal's tick on.
- **Face kinds:** a block at 0°/90°/180°/270° has only flat faces; at 30° only tilted ones; a 45° `slope` has one of each kind; a `slope` rotated 180° has a horizontal top on which the player stays level until the square leaves it (like a block), while on a 45° face the circle's center is 50 px (+ skin) from the surface; walking off a block's ledge, the player stays exactly level until the square's trailing edge passes the edge, then falls.
- **Step-up needs downward momentum:** jumping up into a block's corner that's 20 px above the feet (the player still rising) phases through the corner without being snapped on top; falling or running into the same corner steps up.
- **No jump buffer:** pressing and releasing 1 to 20 ticks before landing never jumps; holding from 20 ticks before landing jumps on the first tick `can_jump` is true; a press on exactly that tick jumps.
- **Double precision:** the same slope run placed at x = 500 and at x = 1 000 000 behaves identically (same contacts, same `vy` sequence, positions equal up to the offset).
- **Jump zone:** the cube can jump on flat ground, on a slope while climbing it (`vy == surface_rise`), and off a block's corner (its vertical side or its horizontal top) touched by the square's light-green corner while falling; it can't while rising through a block's edge after a jump, can't off a tilted face touched only by the square's corner outside the circle, and can't twice near an uphill slope; with flipped gravity the zone is the top strip; landing after any air time and any icon spin, it's at `pos.y = 800` and can jump on that tick.
- **Icon:** the icon's rotation never changes the physics: the same run with the spin disabled gives identical `sim_state_hash` values every tick.
- **Squeezed:** a player driven into a V-shaped notch narrower than itself dies (the inner box) instead of looping, and `distance` still advances by exactly `vx` on every tick before that.
- **Corners:** a box reaching a block's top-left corner exactly at the same time horizontally and vertically lands (the tie goes to the vertical axis).

### 8.3 The bot### 8.3 The bot (the most valuable test)

The bot answers "is every level still beatable?" for any physics settings. It explores inputs depth-first:

- A **decision** happens when the cube can jump (jump or not), and every 12 ticks in ship mode (hold or not; 12 ticks = 50 ms, `bot_decision_ticks` in FEATURES 6.1). A decision is the button state for the following ticks, so it can be a **release** as well as a press: because of the hold rule (3.4), releasing and pressing again is sometimes the only way to make a hold fresh. FEATURES adds decisions for its modes and around live orbs (FEATURES 10.2).
- It tries "don't press" first. When the player dies, it goes back to the most recent decision that hasn't tried "press" yet, flips it, and continues from there.
- **Snapshots instead of replays.** At each decision it saves a run-state snapshot (3.3). Backtracking restores the snapshot instead of replaying the level from tick 0, so the cost of a retry is proportional to the distance from the decision, not to the level length. On long levels this is the difference between seconds and minutes.
- A **dead-state memo** makes it fast: when both choices at a decision have failed, it stores `sim_physics_hash` (3.3) as dead, and later runs that reach the same state stop immediately. Hashing every field that decides the future (corridor, spent objects, hold state, and every FEATURES field) means two states are only merged when they really are the same, so the memo can't hide a path; leaving out the camera, which decides nothing, lets it merge states that only differ in how the camera got there. (The lab's version keyed on tick, y rounded to 0.5 px, vy and mode only, which can in theory merge states that differ in camera height or used orbs.)

The lab's `core_prototype/nsolve.c` is a complete working version (about 80 lines) to adapt to your final `sim_t`. On your 7 levels it finishes in under 0.1 s total. It lives in `src/sim/bot.c`, because both the tests and `--check` (7.4) use it.

**The bot never blocks anything.** It's a search with a cap on attempts, and its decisions are coarser than a human's (every 3 ticks in a ship), so "not completable" can be wrong. Its results are **warnings**:

- `make test` runs it for every level and every rule combination you care about (at least: all legacy, all new) and prints a table (`level6  legacy: OK  new: no path found, furthest 83%`), but the test run's exit status only depends on the real tests (unit, replay, determinism).
- `--check` prints the same line and exits 0 (7.4).
- The editor's Verify button shows it as information (FEATURES 11.10).

You look at the warning, play the section yourself, and decide.

### 8.4 Sanitizers and a smoke test of the real game

- `make debug` then play; ASan/UBSan catch leaks and overflows immediately.
- In CI, the game itself can be started headless, to catch crashes at startup and in menus:

  ```sh
  ALSOFT_DRIVERS=null xvfb-run -a timeout 5 ./my_gd; test $? -eq 124   # 124 = killed by timeout = still running
  ```

### 8.5 GitHub Actions

`.github/workflows/ci.yml`:

```yaml
name: ci
on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - name: Dependencies
        run: sudo apt-get update && sudo apt-get install -y libcsfml-dev xvfb
      - name: Build with warnings as errors
        run: make CFLAGS="-Wall -Wextra -Werror -Iinclude -MMD -MP -ffp-contract=off -O2"
      - name: Unit, replay and determinism tests (bot results are printed, never fail)
        run: make test
      - name: Every level loads without invalid lines
        run: for f in levels/*; do ./my_gd --check "$f" || exit 1; done
      - name: Fuzz the parser for 60 s
        run: sudo apt-get install -y clang && make fuzz_parser && ./fuzz_parser -max_total_time=60 levels tests/levels
      - name: Smoke test
        run: ALSOFT_DRIVERS=null xvfb-run -a timeout 5 ./my_gd; test $? -eq 124
```

---

## Phase 9: Rendering

### 9.0 Performance rules

CSFML adds a C-to-C++ call on top of SFML, and every `sfRenderWindow_drawSprite` / `drawText` is a separate OpenGL draw call with its own state setup. Drawing each object with its own sprite call is the slowest way to use SFML; a level with a few thousand objects on screen would spend most of the frame there. The rendering is designed around one goal: **a handful of draw calls per frame, whatever the level size, and no work per frame for things that don't change.**

1. **Static geometry, built once.** Objects never move (Principle 1), so their vertices are computed once per level load, never per frame.
2. **One texture for all level objects** (an atlas), so every object can share a draw call.
3. **Chunks** of the level along x: only chunks overlapping the view are drawn.
4. **GPU-resident vertices**: `sfVertexBuffer` (SFML 2.5+) keeps the static vertices on the GPU; drawing them costs almost nothing on the CPU.
5. **No per-frame allocation or text layout**: shapes, texts and arrays are created once; a text's string is only set when its content changes.
6. **Measure**: the debug overlay (9.6) shows the frame time and the number of draw calls, counted by small wrappers around the `sfRenderWindow_draw*` calls.

With this, a frame is about ten draw calls: background, ground, up to 3 layers × 3 visible chunks of objects, corridor strips, player, HUD texts.

### 9.1 Views

- `gd->ui_view`: a fixed 1920×1080 view for menus and the HUD (`sfView_createFromRect((sfFloatRect){0, 0, VIEW_W, VIEW_H})`). Don't use the window's default view: it changes when the window is resized.
- `gd->level_view`: same size, centered each frame on the camera (below). It lives in `gd_t`, next to `ui_view`, so window recreation (FEATURES 5.4) can re-apply the letterbox to both.
- `VIEW_W`/`VIEW_H` (1920/1080) are presentation constants: put them in a game-side header (`include/view.h`), not in the sim.

```c
void level_render(gd_t *gd, level_t *lv)
{
    const run_state_t *st = &lv->sim.st;
    vec2_t cam = snap_camera(gd, st->player.pos.x - PLAYER_SCREEN_X, st->cam.pos.y);

    sfRenderWindow_setView(gd->w, gd->ui_view);
    if (lv->state == LEVEL_COMPLETE) {
        render_end_sequence(gd, lv);                     /* 9.8, then the end screen */
        return;
    }
    render_background(gd, cam);                          /* screen space, parallax */
    sfView_setCenter(gd->level_view, (sfVector2f){cam.x + VIEW_W / 2, cam.y + VIEW_H / 2});
    sfRenderWindow_setView(gd->w, gd->level_view);
    render_ground(gd, cam.x);
    render_ship_bounds(gd, &lv->sim, cam.x);
    render_objects(gd, lv, cam.x);
    render_player_or_explosion(gd, lv);
    if (gd->debug_overlay)
        render_hitboxes(gd, lv);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    render_hud(gd, lv);
}
```

The sim's camera y goes straight into the view; the horizontal part is derived from the player.

**Pixel snapping.** The camera moves 4.3275 px per tick at 240 Hz (GD's normal speed), so its position is usually fractional. A view centered on a fractional position samples textures between texels: tiles shimmer and thin seams appear between adjacent blocks. Round the camera to a whole **screen** pixel before building the view:

```c
static vec2_t snap_camera(gd_t *gd, float x, float y)
{
    float scale = gd->viewport_px_w / VIEW_W;          /* screen pixels per logical pixel */

    return (vec2_t){roundf(x * scale) / scale, roundf(y * scale) / scale};
}
```

`viewport_px_w` is the letterboxed viewport's width in window pixels (9.7). Also call `sfTexture_setSmooth(tex, sfFalse)` on every game texture, and leave 2 px of padding around each atlas entry, filled by repeating its border pixels, so rounding never samples a neighbor.

### 9.2 Objects: atlas, chunks, static vertex buffers

**Atlas.** At startup, load every object image (`block.png`, `spike.png`, `cube_portal.png`, later pads and orbs) as an `sfImage`, copy them side by side into one atlas image with `sfImage_copyImage`, and create one `sfTexture` from it. Keep each image's rectangle in the atlas (`gd->atlas_rect[type]`).

**Vertices.** Each object is two triangles (6 vertices, `sfTriangles`): the rect's four corners, rotated around its center by `rotation` (the same function as the hitbox, 4.4), with texture coordinates from the atlas and a vertex color (white, or the portal tint: cyan for cube, pink for ship).

```c
static void append_object(sfVertex *v, const object_t *o, sfFloatRect tex, sfColor col)
{
    vec2_t c = {o->rect.x + o->rect.w / 2.0f, o->rect.y + o->rect.h / 2.0f};
    vec2_t p[4] = {{o->rect.x, o->rect.y}, {o->rect.x + o->rect.w, o->rect.y},
        {o->rect.x + o->rect.w, o->rect.y + o->rect.h}, {o->rect.x, o->rect.y + o->rect.h}};
    sfVector2f t[4] = {{tex.left, tex.top}, {tex.left + tex.width, tex.top},
        {tex.left + tex.width, tex.top + tex.height}, {tex.left, tex.top + tex.height}};
    static const int order[6] = {0, 1, 2, 0, 2, 3};

    for (int i = 0; i < 4; i++)
        p[i] = rotate_point(p[i], c, o->rotation);
    for (int k = 0; k < 6; k++)
        v[k] = (sfVertex){{p[order[k]].x, p[order[k]].y}, col, t[order[k]]};
}
```

**Chunks and layers.** Split the level into chunks of 1024 px along x, by the left edge of each object's drawn (rotated) bounds. Inside each chunk, keep one vertex buffer **per layer**: blocks, then hazards, then interactive objects (the same draw order in the game and the editor, FEATURES 11.5). Each chunk remembers the leftmost and rightmost drawn x of its objects, since an object can stick out of its chunk.

```c
typedef struct render_chunk {
    float left;                          /* min drawn x of its objects */
    float right;                         /* max drawn x                */
    sfVertexBuffer *layer[LAYER_COUNT];  /* NULL if the layer is empty */
} render_chunk_t;

void render_objects(gd_t *gd, level_t *lv, float cam_x)
{
    sfRenderStates rs = {sfBlendAlpha, sfTransform_Identity, gd->atlas, NULL};

    for (int layer = 0; layer < LAYER_COUNT; layer++)
        for (size_t c = 0; c < lv->nb_chunks; c++) {
            render_chunk_t *ch = &lv->chunks[c];

            if (ch->right < cam_x || ch->left > cam_x + VIEW_W || ch->layer[layer] == NULL)
                continue;
            sfRenderWindow_drawVertexBuffer(gd->w, ch->layer[layer], &rs);
        }
}
```

Build the buffers once in `level_start`: `sfVertexBuffer_create(n, sfTriangles, sfVertexBufferStatic)` then `sfVertexBuffer_update(buf, vertices, n, 0)`. If `sfVertexBuffer_isAvailable()` is false (very old GPUs), keep the same vertices in `sfVertexArray`s instead: same code, drawn with `sfRenderWindow_drawVertexArray`. The chunk loop is over a few hundred chunks at most; for very long levels, keep a first-visible chunk cursor like the collision cursor (4.1).

**Spent objects** are drawn like live ones: they lose their hitbox, not their sprite. The feedback flash when an interactive object becomes spent is the only per-frame object drawing: the game layer compares the `spent` bitset with the previous frame's and, for a few frames, draws that one object again with a single sprite (white, fading out). Pulsing orbs later work the same way: a small dynamic vertex array for animated objects, the static buffers for everything else.

This also fixes two mismatches you have today: `block.png` and `spike.png` are 100×100 and never scaled, so `size 1` objects are drawn twice as big as their hitbox (levels 1 and 2 have some); and portals are drawn with the player/ship icons at 100×100 over a 100×200 hitbox. Vertices use each object's real rect, and portals use `cube_portal.png` (100×200, currently unused), tinted by mode.

**Texts.** `sfText` re-lays out its glyphs whenever its string changes, and each text is its own draw call. Set the percentage text only when the value shown changes (keep the last string and compare), build the attempt text once per attempt, and never create texts during a frame.

### 9.3 Ground and background

`ground.png` is currently a 1920×1080 copy of the level background, drawn at a fixed position: the ground doesn't scroll. Use a small tileable texture with `sfTexture_setRepeated(tex, sfTrue)`:

```c
void render_ground(gd_t *gd, float cam_x)
{
    sfVector2u ts = sfTexture_getSize(gd->res.ground);      /* small tile, not 1920x1080 */
    float x = floorf(cam_x / ts.x) * ts.x;

    sfSprite_setTextureRect(gd->ground_sprite,
        (sfIntRect){0, 0, (int)VIEW_W + 2 * (int)ts.x, 1200});
    sfSprite_setPosition(gd->ground_sprite, (sfVector2f){x, GROUND_Y});
    sfRenderWindow_drawSprite(gd->w, gd->ground_sprite, NULL);
}
```

(1200 px deep, so the ground never ends inside the view, whatever the camera does in a corridor.) One draw call. Background: repeated texture in the UI view, texture rect offset by `cam.x * 0.1` and `cam.y * 0.1` for parallax, also one draw call.

### 9.4 Player

The icon is the "dynamic hitbox" of 4.3: it has no physics, it's only drawn, at `pos`. Its rotation is still advanced in the tick (`player_update_rotation`, cosmetic), so it doesn't depend on the frame rate and replays look the same:

```c
void player_update_rotation(player_t *p)                 /* cosmetic, 2.2 rule 4 */
{
    float surface = atan2f(p->support_normal.x, -p->support_normal.y * (float)p->gravity_dir)
        * 180.0f / (float)M_PI;               /* 0 on flat ground, the slope's angle on a slope */

    if (p->mode == MODE_SHIP) {
        p->rotation = -atan2f(p->vy, p->vx) * 180.0f / (float)M_PI * (float)p->gravity_dir;
        return;
    }
    if (!p->grounded) {
        p->rotation += PER_TICK(CUBE_SPIN) * (float)p->gravity_dir;   /* spins in the air */
        return;
    }
    p->rotation = surface + roundf((p->rotation - surface) / 90.0f) * 90.0f;   /* lies along it */
}
```

- In the air the cube spins; when it lands it snaps to the nearest quarter turn **relative to the surface**, so it lies flat on flat ground and along a slope on a slope. That snap is purely visual: the player's position was decided by the square or the circle (4.4).
- On a slope, the circle's center is exactly 50 px from the surface, so the tilted icon (100 × 100, drawn around `pos`) sits flush on it with no correction.
- `atan2f` is fine here: rotation never feeds back into the physics.
- FEATURES' ball rolls: the game layer adds `distance / 50` radians to its drawn angle (a circle of radius 50 rolling without sliding).

Today the cube only spins while its **screen** y is above 750 and snaps to 0° on landing. So it stops spinning during the last 50 px of a fall to the ground, and it snaps back from any angle (a visible jolt). Now it spins for the whole jump and snaps onto whatever it lands on, like GD. The ship tilts with its motion.

### 9.5 HUD

- Progress: the percentage, and a GD-style bar at the top (two rectangles, created once; only the fill width changes).
- "Attempt N": GD draws it **in the world** near the spawn, so it scrolls away naturally. Draw it in the world view at `(PLAYER_SPAWN_X + 400, 400)`: no timer needed.

### 9.6 Debug overlay (F3 key)

Build the outlines each frame into one `sfVertexArray` of `sfLines` (created once, cleared each frame): the player's rigid square (white), circle (green), inner box (red), and the jump zone (its two parts shaded: inside the circle, and the square's corners; brighter when `can_jump` is true), neutral hitboxes (blue), harm hitboxes (red), interactive hitboxes (yellow; spent ones in grey), every polygon drawn through its vertices, ship bounds (cyan), kill ceiling (red line, 4.7), the tick's path legs, and the **contacts of the last tick** as short arrows along their normals (white: landing, orange: head hit, grey: passed into a wall, green: stepped up, red: death). One draw call. Print tick, `vx`, `vy`, `surface_rise`, `grounded`, `can_jump`, mode, **frame time in ms and draw calls this frame** (9.0). Thirty minutes of work; it turns every collision question into something you can see.

### 9.7 Resolution independence

On `sfEvtResized`, apply a letterbox viewport to both views, so the 16:9 image keeps its proportions:

```c
void apply_letterbox(sfView *view, unsigned int w, unsigned int h)
{
    float win = (float)w / (float)h;
    float want = VIEW_W / VIEW_H;
    sfFloatRect vp = {0.0f, 0.0f, 1.0f, 1.0f};

    if (win > want) {
        vp.width = want / win;
        vp.left = (1.0f - vp.width) / 2.0f;
    } else {
        vp.height = win / want;
        vp.top = (1.0f - vp.height) / 2.0f;
    }
    sfView_setViewport(view, vp);
}
```

Apply it to `gd->ui_view` and `gd->level_view`, and store the viewport's width in pixels (`vp.width * w`) as `gd->viewport_px_w` for pixel snapping (9.1).

Mouse positions (clicks, custom cursor) then go through `sfRenderWindow_mapPixelToCoords(gd->w, pixel, gd->ui_view)`.

**Window size.** A fixed 1920×1080 window doesn't fit on a 1366×768 laptop screen. Create the window at 1280×720 by default (clamped to the desktop size from `sfVideoMode_getDesktopMode()`), resizable, with the letterbox doing the rest. FEATURES 5 adds the options (fullscreen, sizes) on top of this.

### 9.8 End of level

Today the end screen replaces the level instantly on the completion tick. GD makes the finish a moment: the camera stops, the player flies on into the end, a flash, then the results.

When `complete` becomes true, the game layer (not the sim) plays a 1-second sequence in `LEVEL_COMPLETE` before showing the end screen:

1. The camera x freezes at its value on the completion tick.
2. The player sprite keeps moving right at `SCROLL_SPEED` (a presentation-only position, `end_x + t * SCROLL_SPEED`), spinning, toward an end wall drawn at `PLAYER_SPAWN_X + end_shift + 400`.
3. When it reaches the wall: a white flash (a full-screen rectangle fading out over 0.3 s), the completion sound, and "Level Complete!".
4. Then the end screen, with the cursor. The music keeps playing (FEATURES 4.8).

Clicks and keys during the sequence are ignored except Escape, which skips to the end screen.

---

## Phase 10: Scenes, menus, input, audio

### 10.1 Scenes

Replace `gd->menu` (`'m'`, `'o'`, `'e'`, `'l'`, `'P'`) with an enum and a **deferred** switch:

```c
typedef enum scene {
    SCENE_MAIN, SCENE_OPTIONS, SCENE_EDITOR, SCENE_LEVEL_LIST, SCENE_PLAYING, SCENE_QUIT
} scene_t;
```

Handlers set `gd->next_scene`. At the end of the frame, the main loop frees the old scene and creates the new one. Today each handler frees its own menu in the middle of event handling and returns immediately; it's correct now, but one missing `return` away from a use-after-free.

Naming: the main menu's "online" button opens the editor scene. Rename the button or the scene so they match.

### 10.2 Input

Jump inputs are **bindings**, not hard-coded keys. The defaults are Space, Up and the left mouse button, plus gamepad button 0 (A on most controllers); all of them are rebindable in the options (FEATURES 2.2, 5.5).

```c
typedef enum binding_kind { BIND_KEY, BIND_MOUSE, BIND_JOY } binding_kind_t;

typedef struct binding {
    binding_kind_t kind;
    int code;                   /* sfKeyCode, sfMouseButton, or joystick button */
} binding_t;

static bool binding_is_down(const binding_t *b)
{
    switch (b->kind) {
    case BIND_KEY:
        return sfKeyboard_isKeyPressed((sfKeyCode)b->code);
    case BIND_MOUSE:
        return sfMouse_isButtonPressed((sfMouseButton)b->code);
    case BIND_JOY:
        return sfJoystick_isConnected(0) && sfJoystick_isButtonPressed(0, (unsigned)b->code);
    }
    return false;
}

bool input_held(gd_t *gd)
{
    if (!gd->has_focus)
        return false;
    for (size_t i = 0; i < gd->jump_bindings.count; i++)
        if (binding_is_down(&gd->jump_bindings.items[i]))
            return true;
    return false;
}
```

Call `sfJoystick_update()` once per frame before polling if the window doesn't process events that frame (SFML updates joysticks while polling events).

- **Focus matters:** `sfKeyboard_isKeyPressed` reads the global keyboard state, ignoring focus. Today, pressing Space in another window makes the cube jump. Track `sfEvtLostFocus` / `sfEvtGainedFocus`, and pause the level while unfocused.
- The mouse today only reacts to the **press event**, so holding it does nothing: no auto-jump, and the ship can't be flown with the mouse (each click adds a single 3.5 px/frame of thrust). Polling `sfMouse_isButtonPressed` fixes both.
- Today **any** mouse button jumps (right and middle included). Only the bound ones should (left by default).
- Event loops `return` after the first handled event, leaving the rest queued for later frames (a click and a close request in the same frame get handled a frame apart). Process all events each frame; switch scenes at the end.
- After clicking Retry on the end screen, ignore input until the button is released, or the new attempt starts with a jump.

### 10.3 Audio

- Load sound buffers once (`sfx_t` in `gd_t`: death, level start, level quit), one `sfSound` each.
- `back_mus.ogg` is opened three times as three `sfMusic` (editor, options, level). Open it once.
- Menu music: the main menu restarts it every time it's created, and the level list stops it. GD keeps the menu loop playing across all menus and only stops it for a level. Start it once; stop it on entering a level; resume on leaving.
- Add volume settings to the options screen (it's an empty screen today), saved in the settings file (FEATURES 2), not with progress.

### 10.4 Pause

Escape currently quits straight to the level list. Make it open a pause overlay (Resume, Restart, Practice mode on/off, Quit), stop ticking, `sfMusic_pause` the music. Losing window focus opens the same overlay. Restart follows 6.5. Quit counts as neither death nor completion: it doesn't update the best, and leaving the level is what writes the session's attempts to `save/progress.txt` (6.2). Resuming restarts the level clock, so the pause doesn't turn into a burst of ticks (3.6).

### 10.5 Menu polish

`button_t.pressed` exists but is never used. Use it for feedback: scale the button to 0.9 while pressed, activate on **release** over the button (like GD), and play the click sound. Add hover scaling (1.05) with the mapped mouse position.

---

## Phase 11: Feel tuning

Start once Phase 8's engine tests pass and the bot finds a path on the 7 levels. Then change **one constant at a time**, and run the bot after each change (its results are warnings: read them, play the flagged spots, decide). The debug overlay (9.6) shows the contacts, which makes most tuning questions visible.

### 11.1 The jump at 240 Hz

GD simulates at 240 Hz, and so does this engine (3.2). At 60 Hz with a 60 FPS display, the accumulator sometimes runs 0 ticks one frame and 2 the next (micro-stutter); at 240 Hz it's invisible, and collisions are 4× finer.

Changing the tick rate changes the jump, because the discrete integration differs. With today's per-second values the jump gets higher at 240 Hz. These constants reproduce today's jump exactly (found numerically, verified in the lab's prototype, which integrates in the same order as this engine: impulse, gravity, move):

Gravity is GD's 0.876 acceleration units, untouched: every other speed in the game (pads, orbs, the fall cap, the modes' own values) is expressed against it, so it's the one constant that must not move. The jump velocity is the one we tune, because what has to match is the **height GD publishes**, 2.1333 blocks, and a discrete integration doesn't reach the continuous height of a given velocity. In px:

| Setting | Apex | Airtime | Distance |
|---|---|---|---|
| Legacy game: 60 Hz, jump 1560 px/s, gravity 5040 px/s², speed 750 | 228.6 px | 0.617 s | 462 px |
| GD's 1.94 units at 240 Hz, our order (impulse, gravity, move) | 210.63 px (2.106 blocks) | 0.425 s | 441 px |
| **`CUBE_JUMP_V` = 1.9522 units (2027.6 px/s), same gravity** | **213.32 px (GD's 2.1333 blocks)** | **0.425 s** | **441 px** |

So the constant is 0.6% above GD's published 1.94: the difference is the integration, not a change of feel. The exact solution of "measured apex = 213.333 px" is 1.952227 units; 1.9522 lands 0.01 px under it and keeps the airtime at 102 ticks (1.9524 would tip it to 103). A unit test pins the last line, and any change to the tick rate or the integration order means solving that value again.

The chained-jump height GD also publishes (2.233 blocks) comes out of the same constants: holding the button re-jumps on the first tick the player is allowed to, a little above the ground, so the apex is higher. Measure it rather than tuning for it.

The ship, UFO and ball values GD doesn't publish are ours, kept from the previous tuning and converted into GD units so they scale with the speed. Their published heights do match: the UFO's hop is GD's 1.5666 blocks (FEATURES 7).

### 11.2 Contact tuning

Each of these has a default chosen from GD's feel; adjust with the overlay and the bot:

| Constant | Default | What it changes |
|---|---|---|
| `FLOOR_MIN_DOT` / `FLOOR_MAX_TAN` | 50° | steepest surface you can land and slide on; steeper is a wall. GD's slopes are 45° and 26.6°, so 50° accepts both with a margin |
| `PLAYER_INNER_HALF` | 20 px (40 × 40) | the fatal core. Smaller: more forgiving (deeper corner clips survive). It also sets the jump zone's top line and the step-up height (`half - inner_half`, 30 px by default) |
| `BOUNCE_RESTITUTION_SHIP` (and FEATURES' UFO/ball values) | 0.3 | how much of the rise speed a ceiling hit sends back |
| `BOUNCE_MIN_SPEED` | 60 px/s | below this, a ceiling hit just stops: sliding instead of vibrating |
| Spike hitbox | middle 40% × bottom 80% | the forgiveness of spikes (4.2) |
| `CAM_TAU` | 0.08 s | camera smoothing (3.5) |
| `CUBE_SPIN` | 324°/s | how fast the icon spins in the air (cosmetic) |
| `KILL_CEILING_MARGIN` | 600 px | flipped gravity only: must stay above what a flipped player can legitimately reach, corridors included (4.7) |

Keep the trigonometric literals and `CAM_LERP` in sync with their formulas: the unit tests (8.1) fail otherwise.

### 11.3 Interpolation

Render the player at `prev_pos + (pos - prev_pos) * alpha`, with `alpha = lv->accumulator / 1000000.0` (the fraction of a tick left in the accumulator, 3.6). At 240 Hz the difference is small, but it's free and removes any stepping when the display rate isn't a divisor of the tick rate. The camera uses the interpolated position too. It's presentation only: the sim never sees it.

---

## Phase 12: Assets, licensing, privacy, repository

### 12.1 Photos of people

`res/cecilya.png` (editor background) and `res/noe_background.jpeg` (options background) are photos of real people, published in a public repository. If they're friends and it's a joke, make sure they're fine with it being public and permanent. Either way, replace them with real menu art before sharing the project widely.

Deleting a file doesn't remove it from git history: anyone can still check out an old commit. If they need to disappear, rewrite history with `git filter-repo --path res/cecilya.png --invert-paths` (and the same for the other), then force-push. Everyone with a clone must re-clone.

### 12.2 Assets from Geometry Dash

`explode_11.ogg`, `playSound_01.ogg`, `quitSound_01.ogg` and `menuLoop.mp3` match Geometry Dash's own resource file names, and the menu art looks like the game's. If they come from the game, they're RobTop's copyrighted material, and a public repository with them can receive a DMCA takedown (which on GitHub takes the whole repository down). The same applies to `GDfont.ttf` if it's the font shipped with the game.

Safe replacements:

- Graphics and sounds: Kenney (kenney.nl, CC0), OpenGameArt (check each asset's license), Freesound (filter by CC0).
- Fonts: Google Fonts (SIL Open Font License).
- Or make your own: the icons and spike in `res/` already look home-made.

Keep a `res/CREDITS.md` listing each asset, its author and license. Add a line to the README: "Fan project, not affiliated with RobTop Games." Recreating game **mechanics** is fine; copying its **assets** is the risk.

### 12.3 Placeholder cleanup

Byte-identical files: `ground.png` = `level_background.png` = `end_level_background.png`; `main_background.png` = `menu.png`; `b.png` = `editor_background.png` = `level_list_background.png` (1920×1079, one pixel short); `cursor.png` = `play_button.png` = `param_button.png` = `param_buton.png` = `online_button.png` = `retry_button.png` = `opt_background.png` (so every menu button currently shows the cursor arrow); `player_icon.png` = `quit_button.png`.

Never loaded: `b.png`, `cube_portal.png`, `editor_background.png`, `explode_11.ogg`, `explos.png`, `menu.png`, `monster.png`, `monster2.png`, `opt_background.png`, `param_buton.png`, `playSound_01.ogg`, `quitSound_01.ogg`, `spike_hitbox.png`.

`noe_background.jpeg` is 2688×1512 (much bigger than the screen) and gets replaced anyway.

### 12.4 README

The repository has no README. Include: what it is (one line + screenshot/GIF), dependencies (`libcsfml-dev` on Ubuntu, `csfml` on Arch, or CSFML 2.6 built from source), build (`make`, `make debug`), controls, how to add a level (the format from 7.2), how to run tests, credits and the not-affiliated line. A LICENSE file for **your** code (MIT is common), separate from asset licenses.

---

## Phase 13: Further features

All of these are small once the sim is pure and deterministic and the run state is a snapshot (3.3).

### 13.1 Practice mode

Toggled from the pause menu (10.4). While it's on:

- **Checkpoints** are run-state snapshots (`sim_snapshot_save`). Place one with the checkpoint key (default `Z`), remove the last one with `X` (both rebindable, FEATURES 2.2). A checkpoint can only be placed while alive. They're drawn as green diamonds at the saved player position, and as ticks on the progress bar.
- **Death**: the usual delay and explosion, then `sim_snapshot_restore` of the last checkpoint (or `sim_reset` if there is none). Every respawn counts an attempt.
- **Restart key**: restores the last checkpoint immediately and counts an attempt (6.5).
- **Best**: practice has its own `practice_best` in the progress store. It's only a statistic, shown in the level list next to the normal best: no "New best!" popup, and it never changes `best` (6.3). Completing the level in practice sets `practice_best` to 100 and the end screen says "Practice complete".
- **Music** restarts from the checkpoint's time: `offset + tick / TICK_RATE` (FEATURES 4.8).
- Leaving practice mode (or the level) discards the checkpoints.

Because a snapshot is the whole run state (player, camera, corridor, tick, cursor, spent objects, and every FEATURES field), a restored checkpoint is exactly the moment it was taken: no special cases per mode or object.

### 13.2 Replays and ghost

Record the ticks where `held` changes and the ticks with `pressed` (a few dozen integers per run). Replaying = feeding them back through `sim_tick`. Save the best run per level in `save/replays/<id>.txt`:

```text
my_gd-replay 1
level 10280 hash=9f2c41d07ab35e11 tick_rate=240 rules=0
held 104 131 190 212 ...
pressed 104 190 ...
```

The header makes a replay refuse to play on a different version of the level, tick rate or rule set, instead of desyncing. Draw the replayed run as a translucent ghost: a second `sim_t` whose `lvl` is a shallow copy of the level's (objects shared, never freed by the ghost), with its own run state.

### 13.3 Level editor

The editor scene is empty today. FEATURES 11 specifies it: world coordinates via `sfRenderWindow_mapPixelToCoords(w, pixel, level_view)`, grid snapping, placing, rotating and resizing objects, undo, playtest, the bot as information, and saving in the 7.2 format.

### 13.4 More modes and objects

UFO, wave, ball, robot, swing, spider, pads, orbs, gravity portals, speed portals (with a double-precision `distance` instead of `tick * speed`) and mini portals: FEATURES 6–10.

### 13.5 Level music

A `music <file> [offset]` header line, the song library and synchronization: FEATURES 4.

### 13.6 Triggers (out of scope)

GD-style triggers act on groups of objects (5.1); objects join a group with `group=` on their own line (7.2). The mechanism will be designed separately.

---

## Appendix A: Bug list with evidence

"Measured" = reproduced with the lab tools (Appendix E). "Code" = found by reading.

| # | Bug | Where | Evidence | Fixed in |
|---|---|---|---|---|
| 1 | First attempt spawns 50 px right: obstacles 4 frames early | `create_player` vs `player_dead` | Measured (F2) | 3.4 |
| 2 | Ship portal teleports the player 250 px | `portal_shift` | Measured (F3) | 5.2 |
| 3 | Ship portal re-fires and leaks boundary lists (14 per crossing once #2 is fixed) | `check_portal`, `remove_create_portal_boundaries` | Measured (F3) | 5.1 (interactive rule) |
| 4 | No jumping for ~6 frames inside a cube portal | `load_new_player_texture` | Measured (F4) | 5.1 |
| 5 | Blocks wider than ~210 px stop colliding | `check_collisions` culling | Measured (F5) | 4.1 |
| 6 | Best % loses decimals | `my_str_to_word_array`, `manage_first_line`, `read_level_info` | Measured (F6) | 6.4 |
| 7 | Level list order arbitrary | `fill_names_list` | Measured (F7) | 7.5 |
| 8 | Level shown by file name, loaded by header number | `start_level` | Measured (F7) | 7.5 |
| 9 | Portals never freed | `free_objects` | Measured (F8) | 3.3 |
| 10 | Collision loops keep running after death on reset positions | `player_dead` | Code | 4.4 |
| 11 | Physics frame-rate dependent, runs after rendering | `print_level` | Code | 3.6 |
| 12 | Camera stays offset after a ship section (ground at y 600 in level 7) | `move_objects` | Measured | 3.5 |
| 13 | Vertical camera can overshoot below the ground view | `move_objects` | Code | 3.5 |
| 14 | `write` overreads in `main`, wrong program name | `gd.c` | gcc warning | 1.4 |
| 15 | Empty or short level lines crash | `load_object` | Code | 7.3 |
| 16 | Word-array functions allocate one pointer too few | `my_str_to_wordarray.c` | Code | 7.3 |
| 17 | Unknown portal mode leaves gamemode uninitialized | `load_portal` | Code | 5.5 |
| 18 | Missing level file → uninitialized objects → crash | `load_level_data`, `start_level` | Code | 7.3 |
| 19 | Escape mid-run saves current % as best | `rewrite_level` | Code | 6.4, 10.4 |
| 20 | Progress written into tracked level files | `rewrite_level` | Code | 6.4 |
| 21 | Temp path overflow; `remove` before `rename` | `rewrite_level` | gcc warning | 6.4 |
| 22 | Directory read twice; possible uninitialized button pointers | `create_level_list` | Code | 7.5 |
| 23 | Level list overflows the screen past 9 levels | `create_level_button` | Code | 7.5 |
| 24 | Button hit areas are squares, textures aren't | `keyboard_events.c` | Code | 7.5, 10.5 |
| 25 | Sprites not scaled to object size | `load_block`, `load_spike` | Code | 9.2 |
| 26 | Portals drawn with player/ship icons, wrong size | `load_portal` | Code | 9.2 |
| 27 | Ground doesn't scroll | `print_objects` | Code | 9.3 |
| 28 | Cube spin depends on screen y and frame rate; snaps to 0° | `print_player` | Code | 9.4 |
| 29 | Space pressed in another window makes the cube jump | `keyboard_events_playing` | Code (SFML semantics) | 10.2 |
| 30 | Mouse can't hold (no auto-jump, can't fly the ship) | `keyboard_events_playing` | Code | 10.2 |
| 31 | Any mouse button jumps | `keyboard_events_playing` | Code | 10.2 |
| 32 | Event loops stop after the first handled event | all `keyboard_events_*` | Code | 10.1 |
| 33 | Menu music restarts on every return, stops in level list | `create_main_menu`, `create_level_list` | Code | 10.3 |
| 34 | Same music file opened three times | `load_musics` | Code | 10.3 |
| 35 | `play_sound` returns uninitialized data; `play_background_music` leaks | `music.c` (unused) | gcc warning | 1.4 |
| 36 | `my_put_nbr(INT_MIN)` overflows | `my_put_nbr.c` | Code | 1.4 |
| 37 | Circular include `struct.h` ↔ `mygd.h` | headers | Code | 1.3 |
| 38 | No header dependency tracking | `Makefile` | Code | 1.1 |
| 39 | `.gitignore` pattern `*.o/*` matches nothing | `.gitignore` | Code | 1.2 |
| 40 | Photos of people and likely game assets in a public repo | `res/` | Inspected | 12.1, 12.2 |

## Appendix B: Compiler warnings

`gcc -Wall -Wextra`, CSFML 2.6, commit `3f625f2`:

```text
gd.c:269            'write' reading 31 bytes from a region of size 27
gd.c:274            'write' reading 54 bytes from a region of size 46
get_level.c:143     '.temp' directive writing 5 bytes into a region of size between 1 and 256
music.c:44          'sound_buffer' may be used uninitialized
my_put_nbr.c:36     unused variable 'nb_c'
physics.c:218       variable 'top_kill_y' set but not used
unused parameters:  get_level.c:68, keyboard_events.c:221, level.c:112, level.c:151,
                    physics.c:80, physics.c:215, physics.c:330
```

The first four are real bugs; `top_kill_y` points at bug-adjacent code (the ceiling ignores `portal_blocks[0]`). The rest disappear with the rewrite.

## Appendix C: Final file layout

All headers are in `include/`.

```text
include/
    sim/constants.h  sim/sim_types.h  sim/sim.h  sim/progress.h  sim/bot.h
    view.h           (VIEW_W, VIEW_H, render layers, render prototypes)
    input.h          (bindings, input_t sampling)
    mygd.h  struct.h (game-side includes, structs and prototypes)
src/sim/             level_load.c  hitbox.c  sweep.c  sim.c  collision.c  camera.c  player.c  modes.c
                     progress.c  bot.c
src/                 main.c  gd.c  scene.c  input.c  audio.c  level.c  level_render.c
                     atlas.c  end_screen.c  debug_overlay.c  main_menu.c  option_menu.c
                     editor_menu.c  level_list_menu.c  button.c  cursor.c  (my_* helpers still in use)
tests/               test_main.c  test_loader.c  test_progress.c  test_collision.c
                     test_rotation.c  test_replay.c  test_determinism.c  test_bot.c
                     replays/level1.txt ...  levels/ (Appendix D)
tests/fuzz/          fuzz_parser.c
levels/              level1 ... (no progress headers after migration)
res/                 assets + CREDITS.md
build/               release/ and debug/ objects (ignored by git)
.github/workflows/ci.yml
```

Deleted: `physics.c`, `get_level.c`, `portal.c`, `music.c`, `utilitary.c` (replaced by `snprintf`), unused `my_*` helpers.

If you follow a coding style with function length limits, the snippets here are written for readability; split them as needed without changing the structure.

## Appendix D: Test levels

Put these in `tests/levels/`. Each isolates one behavior; expected results assume the new core.

`wide_block` (bug 5): lands, runs the full 800 px.

```text
name TEST wide block
block 1200 650 2 w=16 h=4
```

(An 800 × 200 platform standing on the ground. The earlier square version, `block 1200 650 8` + `block 1600 650 8`, reached 200 px below the ground; the prototype bot completes it.)

`ship_portal` (bugs 2, 3, same-mode portal): one mode change per crossing; the second ship portal moves the corridor to itself; no leak under `make debug`.

```text
name TEST ship portal
portal 1500 650 2 ship
portal 2500 650 2 ship
portal 3500 650 2 cube
```

`cube_portal_jump` (bug 4): holding the button, the cube jumps as soon as it lands inside the portal.

```text
name TEST cube portal jump
portal 1500 650 2 cube
```

`steps`: each step climbable; running into a side kills.

```text
name TEST steps
block 1500 750 2
block 1900 650 2
block 2300 550 2
```

`gap` (level 5's trick): the player fits exactly under the wall.

```text
name TEST exact gap
block 1700 650 2
block 1700 550 2
```

`camera`: view follows up, returns to the ground view.

```text
name TEST camera
block 1500 750 2
block 1700 550 2
block 1900 350 2
block 2100 150 2
block 2300 -50 2
```

`ceiling_spike` (rotation): the spike hangs from y = 600 pointing down; running under it on the ground is safe, jumping into it kills.

```text
name TEST ceiling spike
spike 1500 600 2 rot=180
spike 2500 650 2 rot=90
```

`rotated_gap` (4.2): level 5's exact gap, with the wall blocks written at quarter turns; must behave exactly like `gap`.

```text
name TEST rotated gap
block 1700 650 2 rot=90
block 1700 550 2 rot=270
```

`diagonal_spike`: a 45° spike; the player jumps over its bounding box corner without touching the spike itself.

```text
name TEST diagonal spike
spike 1500 750 2 rot=45
```

`slopes` (4.4): up a 45° slope onto a platform, along it, down a 26.6° slope back to the ground; no input needed, `grounded` on every tick except the short lift-off at the top of the first slope.

```text
name TEST slopes
slope 1500 750 2
block 1600 750 2 w=8 h=2
slope 2000 750 2 w=4 h=2 rot=270
```

(`rot=270` turns the rising triangle into a falling one: its right angle is now at the bottom left. Check it in the debug overlay and adjust the angle if your rotation convention differs.)

`step_up` (4.4): a slope up to a first platform; the second platform is 25 px higher: stepped onto (the step stays under the inner box). The third is 35 px higher than the second: it reaches the inner box, the player dies there.

```text
name TEST step up
slope 1400 750 2
block 1500 750 2 w=4
block 1700 725 2 w=4
block 1900 690 2 w=4
```

`head_hit`: holding the button, the cube jumps into the block's underside and dies; after the ship portal, the ship thrusts into a second block's underside and bounces.

```text
name TEST head hit
block 1650 600 2
portal 2500 650 2 ship
block 3000 350 2 w=8 h=1
portal 4000 650 2 cube
```

`seams`: a slope up, then 10 blocks in a row written in random order; the cube runs across them without dying, with no input.

```text
name TEST seams
slope 1400 750 2
block 2300 750 2
block 1600 750 2
block 2100 750 2
block 1500 750 2
block 1900 750 2
block 1700 750 2
block 2200 750 2
block 1800 750 2
block 2000 750 2
block 2400 750 2
```

(The unit test generates 20, and also checks a slope meeting a block.)

`parser`: warnings for the bad lines, no crash, spike at 2000 present. Save it with Windows line endings and without a final newline as extra variants.

```text
name TEST parser

spik 1000 750 2
spike 1200 750
portal 1500 650 2 shp
x

spike 2000 750 2
spike 2100 750 2.5
spike 2200 750 2 nan
block 2300 nan 2
block 2400 750 -1
block 2450 750 0
block 2500 750 2 rot=abc
block 2600 750 2 speed=3   # unknown field: warning, block kept
```

Expected: the lines from `spik` to `rot=abc` are rejected with a warning each; the last block loads.

The prototype bot (Appendix E) completes `wide_block` (square version), `ship_portal`, `steps`, `gap` and `camera` both with the legacy rules and with the rules closest to the new engine at 240 Hz. Re-run it on every test level once the real sim exists.

## Appendix E: The verification lab

`my_gd_lab.zip` contains:

- `legacy/`: tools linking the **real legacy sources** (built from your checkout, `gd.c`'s `main` renamed with `-Dmain=game_main`): `exp1`..`exp6` (the Part I measurements), `solver` (bot on the legacy physics, writes `solN.txt`), `diff` (legacy vs prototype on 400 perturbed scripts per level).
- `core_prototype/`: `sim.h`/`sim.c` (≈200 lines, pure C, the design of Phases 3–5 with every legacy quirk as a switch), `nsolve.c` (headless bot), `apex.c` (jump arcs).
- `Makefile`, `README.md`.

```sh
sudo apt install libcsfml-dev xvfb              # Ubuntu / Debian
sudo pacman -S csfml xorg-server-xvfb           # Arch
make LEGACY=../My_GD-legacy run-legacy
make LEGACY=../My_GD-legacy run-core
```

The legacy tools need a display (loading a level creates sprites, which need an OpenGL context) and an audio device; `xvfb-run` and `ALSOFT_DRIVERS=null` provide fake ones. Without Xvfb, delete `xvfb-run -a` from the lab's Makefile: the tools then briefly open windows on your real display and produce the same numbers (that's how the baseline was taken on your machine). The prototype needs neither, which is the point of Phase 2.

The prototype is a reference, not the final code: its structs are simplified, it uses legacy per-frame units internally, and its collisions are the older overlap rules, not the swept engine of Phase 4. Use it for the feel numbers (jump arcs at any tick rate), as a second opinion on level beatability, and as the starting point for the bot in `src/sim/bot.c`.

## Appendix F: Coordinate spaces and units cheat sheet

| Thing | Space | Unit | Notes |
|---|---|---|---|
| Object `rect` | world | px | from the level file, unrotated, never changes |
| Object `rotation` | — | degrees | clockwise, around the rect's center, any angle |
| Object `hitbox` | world | px | full area (containment counts); polygon (rotation included) or circle; `aabb` for the broadphase |
| Object category | — | — | neutral (contact response), harm (kills), interactive (acts once) |
| `reach` | — | px | widest hitbox in the level, for culling |
| `kill_y` | world | px | kill ceiling (flipped gravity only), 600 px above the highest hitbox |
| `spent` bitset | — | bits | in the run state; set when an interactive object acts, cleared by `sim_reset` |
| `player.pos` | world | px | shared center of the rigid square, the circle, the inner box and the icon |
| Jump zone | relative to `pos` | px | the rigid square below the inner box: `y` from +20 to +50 (mirrored when flipped); inside the circle any neutral counts, the corners only flat (horizontal or vertical) faces |
| `surface_rise` | — | px/tick | the rise speed the supporting surface imposes (0 on flat ground); "downward momentum" means `vy <= surface_rise` |
| `player.vx` | — | px/tick | to the right; base speed × `speed_mult` |
| `player.vy` | — | px/tick | rise speed, away from the floor; world displacement `-vy * gravity_dir` |
| `distance` | — | px (double) | scrolled since spawn; `pos.x = 350 + distance`; percentage |
| Contact normal | world | unit vector | out of the obstacle toward the player; floor if it points up within 50° |
| `cam.pos` | world | px | top-left of the screen in the world |
| Screen position | screen | px | `world - cam`; player always at x 350 |
| Ship bounds | world | px | surfaces, not block positions |
| `tick` | — | 1/TICK_RATE s | integer, the only clock the sim knows |
| Constants in `constants.h` | — | px/s, px/s² | converted with `PER_TICK`, `PER_TICK2` |
| Views (`VIEW_W`×`VIEW_H`) | — | logical px | 1920×1080 regardless of window size |
| Accumulator | — | µs × TICK_RATE | integer; one tick per 1 000 000 |

---

## Appendix G: The geometry, formula by formula

Everything Phases 3–5 use but only describe in words. Write these functions first and test them alone (8.1): the rest of the engine is bookkeeping on top of them. All of it is `double`, all of it is deterministic (`+ - * /` and `sqrt` only).

### G.1 Conventions

- x grows right, y grows **down**. A polygon's vertices are stored **clockwise on screen**.
- Dot product `dot(a, b) = a.x * b.x + a.y * b.y`. Cross product (a scalar here) `cross(a, b) = a.x * b.y - a.y * b.x`.
- For an edge from `A` to `B` of a clockwise polygon, `e = B - A` and the **outward** normal is

  ```text
  n = (e.y, -e.x) / |e|
  ```

  Check on a square `(0,0) -> (1,0) -> (1,1) -> (0,1)`: the top edge gives `n = (0, -1)`, pointing up, out of the square. Right.
- A face's **line** is `dot(n, p) = offset` with `offset = dot(n, A)`. A point `p` is outside that face when `dot(n, p) > offset`.
- The player's "up" is `(0, -gravity_dir)`. Multiplying a y difference by `gravity_dir` turns it into "along gravity", which is how every mirrored test is written (`(a - b) * g > 0` means "a is below b for this player").

### G.2 Building a hitbox (load time, 4.2)

1. Vertices from the type's local shape, rotated around the rect's center, each coordinate rounded to the 1/1024 px grid.
2. **Orientation.** Signed area `A = 0.5 * sum(cross(V[i], V[i+1]))`. With y down, clockwise on screen gives `A > 0`; if `A < 0`, reverse the vertex order. (`A == 0` is a degenerate object: reject it at load.)
3. **AABB**: min and max of the vertices.
4. **Face kinds**, per edge, on the rounded coordinates: `A.y == B.y` horizontal, `A.x == B.x` vertical, otherwise tilted.
5. **Axes**: for each edge, `n = (e.y, -e.x) / |e|`. Drop it if `n.x == 0 || n.y == 0` (the x and y axes are always tested anyway). Drop it if an axis already kept is parallel: `fabs(dot(n, a)) >= 1 - 1e-12` (an axis and its opposite are the same separating axis). A rectangle keeps 2, a right triangle 1 to 3.
6. **Projections**: for each kept axis `a`, `axis_lo = min(dot(V[i], a))`, `axis_hi = max(dot(V[i], a))`.
   Also store, **per edge** (not deduplicated, one per face): `face_n[i] = (e.y, -e.x) / |e|` and `face_off[i] = dot(face_n[i], V[i])`. The circle's sweep and the step-up work face by face, so they need these; the axes are only for the square's SAT.
7. The two implicit axes: `hitbox_axis(h, 0) = (0, 1)` with `lo = aabb.y`, `hi = aabb.y + aabb.h`; `hitbox_axis(h, 1) = (1, 0)` with `lo = aabb.x`, `hi = aabb.x + aabb.w`. Index `k >= 2` reads `h->axes[k - 2]`.

### G.3 Square against a polygon

The swept version is 4.3's `sweep_box_poly`. The static one, used by the touch tests:

```c
/* Do the areas of the square (center c, half h) and the polygon overlap?
   Touching is not overlapping (3.0). */
bool overlap_box_poly(vec2_t c, double h, const hitbox_t *hb)
{
    for (int k = 0; k < hb->naxes + 2; k++) {
        vec2_t a = hitbox_axis(hb, k);
        double r = h * (fabs(a.x) + fabs(a.y));
        double p = dot(c, a);

        if (p + r <= hitbox_lo(hb, k) || p - r >= hitbox_hi(hb, k))
            return false;                      /* separated, or exactly touching */
    }
    return true;
}
```

`sweep_box_touch(c, h, d, hb)` is `sweep_box_poly` without the "moving into" test and without the normal: keep `t_in = max(entry times)` and `t_out = min(exit times)`, and return `max(t_in, 0)` when `t_in < t_out`, `t_in <= 1` and `t_out > 0`; otherwise "no touch" (`INFINITY`). The square's projection radius is `r = h * (|a.x| + |a.y|)` on every axis, which is exact for an axis-aligned square whatever the axis.

### G.4 Circle against a polygon

The circle has center `c`, radius `r`, and moves by `d`. The disc touches the polygon exactly when its center reaches the polygon **inflated by r**: every face pushed out by `r` along its normal, plus a disc of radius `r` around every vertex. So the first contact is the earliest of the face candidates and the vertex candidates.

**Faces.** For face `i` with normal `n` and line offset `off`:

```text
den = dot(n, d)
if (den >= 0) skip                     /* not moving toward this face */
t = (off + r - dot(n, c)) / den
if (t < 0 || t > 1) skip
p = c + t * d - r * n                  /* the touch point, on the face's line */
if (dot(p - A, e) < 0 || dot(p - B, e) > 0) skip   /* outside the face's extent */
candidate: t, normal n
```

**Vertices.** For vertex `V`, with `f = c - V`:

```text
a = dot(d, d)                          /* 0 if the player doesn't move: skip */
b = dot(f, d)
cc = dot(f, f) - r * r
disc = b * b - a * cc
if (disc < 0) skip                     /* the path misses this vertex's disc */
t = (-b - sqrt(disc)) / a              /* the entering root */
if (t < 0 || t > 1) skip
candidate: t, normal (c + t * d - V) / r
```

Take the candidate with the smallest `t`; on a tie, the face (a face contact is a real surface, a vertex contact is a corner). A vertex's full disc sticks out of the inflated polygon nowhere, so no spurious early hit is possible, and only the smaller root can be an entry.

The sweep takes the two predicates of G.9 (`face_ok`, `vertex_ok`) and skips the faces and vertices they reject; passing none means "all of them", which is what the touch tests and the step-up's ceiling check use.

**Already touching** (`cc <= 0` for some vertex, or the center is inside the inflated shape): contact at `t = 0`. Its normal is the direction from the closest point of the polygon to the center, or, if the center is inside the polygon itself, the normal of the face with the smallest `off - dot(n, c)`. The neutral sweep never needs this case (`first_event` skips shapes the shape already overlaps, 4.4); the touch tests do.

**Distance to a convex polygon** (used by `overlap_circle_poly`, and by the jump zone):

```text
inside = true
best = INFINITY
for each face i:
    if (dot(n_i, c) > off_i) inside = false
    q = closest point of segment [A_i, B_i] to c:
        u = clamp(dot(c - A_i, e_i) / dot(e_i, e_i), 0, 1);  q = A_i + u * e_i
    best = min(best, |c - q|)
distance = inside ? 0 : best
```

`overlap_circle_poly` is `distance < r` (strict: touching isn't a collision). The jump zone's support test is the same with `<= r + 2 * CONTACT_SKIN` (4.3: contact counts there, and the skin covers the gap the circle keeps from slopes).

`sweep_circle_touch` is the same candidate list without the "moving toward" filter, plus the already-touching case at `t = 0`.

### G.5 The ground and the corridor boundaries

They're half-planes, so one axis is enough. With `sy` the surface's y and `side = +1` for "solid below" (the ground, the corridor floor) or `-1` for "solid above" (the corridor ceiling):

```text
normal        = (0, -side)
square touch  : the square's edge crosses sy when (c.y + side * half - sy) * side >= 0
square sweep  : t = (sy - side * half - c.y) / d.y      (skip if d.y * side <= 0)
circle sweep  : the same with the radius: t = (sy - side * r - c.y) / d.y
```

Clamp `t` to 0 when it's negative and the player is already past the line (that's the "put back on the surface" case of 4.3), and skip when `t > 1`. Surfaces are never skipped and never kill (4.3).

### G.6 The jump zone (4.3)

With `g = gravity_dir`, `half` and `inner_half` from the mode, and the player's center `c`:

- the zone's **line** is `y_line = c.y + inner_half * g`;
- the zone **rectangle** is x from `c.x - half` to `c.x + half`, y from `y_line` to `c.y + half * g` (swap the two when `g < 0`), grown by `2 * CONTACT_SKIN` on every side;
- the **dark** part is the disc of radius `half` around `c` on the zone's side of the line.

Dark test, per neutral candidate: clip the polygon to the half-plane `(y - y_line) * g >= 0`, then measure the distance from `c` to the clipped polygon (G.4) and compare with `half + 2 * CONTACT_SKIN`. Clipping one convex polygon by one half-plane (Sutherland–Hodgman, at most `n + 1` vertices out):

```c
int clip_half_plane(const vec2_t *in, int n, double y_line, double g, vec2_t *out)
{
    int m = 0;

    for (int i = 0; i < n; i++) {
        vec2_t a = in[i];
        vec2_t b = in[(i + 1) % n];
        double da = (a.y - y_line) * g;      /* >= 0: inside */
        double db = (b.y - y_line) * g;

        if (da >= 0.0)
            out[m++] = a;
        if ((da > 0.0 && db < 0.0) || (da < 0.0 && db > 0.0)) {
            double u = da / (da - db);       /* da - db != 0 here */

            out[m++] = (vec2_t){a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u};
        }
    }
    return m;                                 /* 0: nothing on that side */
}
```

Light test, per neutral candidate: for each **flat** face (`FACE_HORIZONTAL` or `FACE_VERTICAL`), does the segment meet the zone rectangle? For a horizontal face at `y = Y` between `x0` and `x1`: `Y >= ry0 && Y <= ry1 && x1 >= rx0 && x0 <= rx1`. For a vertical one, the same with the axes swapped. (Comparisons are `<=`/`>=`: the zone is a support test.)

`can_jump = (dark || light) && vy <= surface_rise + RISE_EPSILON`, at the end of the tick.

### G.7 Steps (4.4)

`up_facing_horizontal_face(hb, i, g, &f)`: edge `i` qualifies when `face_kind[i] == FACE_HORIZONTAL` and its outward normal points against gravity (`n.y * g < 0`). Then `f.y = V[i].y`, `f.x0 = min(V[i].x, V[i+1].x)`, `f.x1 = max(...)`.

The step's moment inside a leg, with `lead = pos.x + inner_half` and `d.x > 0` (always: `vx > 0`):

```text
t = lead >= f.x0 ? 0 : (f.x0 - lead) / d.x
```

and the three conditions of 4.4's `step_event` are evaluated **at that t**, on `y(t) = pos.y + d.y * t`.

`lift_hits_surface(s, y, lift)`: the center after the lift is `y' = y - lift * g`, and the circle's point on the ceiling side is `y' - half * g`. With `ceil_y` the surface on that side (the corridor's ceiling when `g > 0`, its floor when `g < 0`, the ground when flipped and outside a corridor), the lift is refused when `(y' - half * g - ceil_y) * g < 0`.

### G.8 The tick's bounds and the path

`tick_sweep_bounds(p)` is the AABB of everything the tick can reach: the rigid square at the start and at the end of the unobstructed move, plus the step-up room on both sides:

```text
ys = pos.y,  ye = pos.y - vy * gravity_dir
room = half - inner_half                        /* 30 px for the 100 px modes */
x0 = pos.x - half - CONTACT_SKIN
x1 = pos.x + half + vx + CONTACT_SKIN
y0 = min(ys, ye) - half - room - CONTACT_SKIN
y1 = max(ys, ye) + half + room + CONTACT_SKIN
```

`advance(s, d, t)`, the only place the player moves:

```c
s->tick_left *= 1.0 - t;                       /* the fraction of the tick left  */
st->distance = s->tick_x0 + p->vx * (1.0 - s->tick_left);
p->pos.x = PLAYER_SPAWN_X + st->distance;      /* never accumulated (3.2)        */
p->pos.y += d.y * t;
s->legs += 1;                                  /* path position = leg index + fraction */
```

`tick_x0` and `tick_left` are set by `move_and_collide` at the start of the tick (`distance` and `1.0`). Horizontal distance is **not** accumulated leg by leg: `vx * t + vx * (1 - t)` isn't `vx` in floating point, so a tick that landed on something would cover a hair less ground than one that didn't, and two runs that differ only in where they land would drift apart. Rebuilding it from the fraction travelled makes every tick cover exactly `vx`, split or not: when the last leg ends, `tick_left` is exactly 0.

`crossed_surface(s)`: `pos.y > GROUND_Y`, or `pos.y > bounds.bottom`, or `pos.y < bounds.top` while the corridor is active. It can't happen (G.5); it's the safety net of 4.3.

### G.9 `first_event`: which shape tests which face

The shapes don't each test the whole hitbox: the square owns horizontal and vertical faces, the circle owns tilted ones and every head hit (4.3). Because `sweep_box_poly` and `sweep_circle_poly` report one contact per shape, the filtering happens on what they're allowed to return.

**The square** (`sweep_box_poly` with `half`, against neutral objects, the ground and the corridor floor) keeps a contact only when:

- it came through the **y axis** (`c.flat`) **and** the face is on the player's floor side, `c.normal.y * g < 0`: landing on the shape's top, whether that top is a horizontal face or a single vertex. A y-axis contact from below (`c.normal.y * g > 0`) is dropped: head hits belong to the circle;
- or it came through the **x axis**: a wall, which `respond` turns into `pass_into`.

A contact through any other axis is a tilted face: dropped, the circle's business.

**The circle** (`sweep_circle_poly` with `half` as radius) is given, per object, the faces and vertices it's allowed to hit, with `g = gravity_dir` and `up = (0, -g)`:

```c
static bool circle_face_ok(const hitbox_t *h, int i, double g)
{
    return h->face_kind[i] == FACE_TILTED       /* support: slopes            */
        || h->face_n[i].y * g > 0.0;            /* head hit: faces the ceiling */
}

static bool circle_vertex_ok(const hitbox_t *h, int i, double g)
{
    int prev = (i + h->nverts - 1) % h->nverts;   /* the two faces meeting at vertex i */

    if (h->face_kind[prev] == FACE_TILTED && h->face_kind[i] == FACE_TILTED)
        return true;                            /* rolling over a corner between slopes */
    return h->face_n[prev].y * g > 0.0 || h->face_n[i].y * g > 0.0;   /* a ceiling corner */
}
```

So the corner between a tilted face and a flat one is **not** a support candidate for the circle: there the square takes over (the 14.6 px case of 4.4), and the step-up finishes the climb. Head hits have no such restriction: every face and corner that faces the player's ceiling side counts, corners of blocks included.

**The surfaces** are added after the objects: the ground and the corridor floor for the square (G.5), the corridor ceiling for the circle. They're never skipped.

**The order**, which is also the tie-break, since a candidate only replaces the current best when it's **strictly** earlier:

1. the neutral candidates in `s->cand` order (which is the level's x-sorted object order, 4.1), square first then circle for each object;
2. the ground, then the corridor floor, then the corridor ceiling;
3. the step events (G.7), so a contact at the same `t` wins over a step.

**Skipping.** Before testing an object with a shape, skip it when

- it's in `s->passed` (the player already went into it as a wall this tick), or
- that **shape** already overlaps it (`overlap_box_poly` / `overlap_circle_poly`, G.3 and G.4): the player is inside it, so the inner box and the step-up decide. It's per shape: a square clipped into a block's side doesn't stop the circle from taking a head hit on that same block.

Resting on a face is touching, not overlapping (3.0), so a supported player is never skipped by its own floor.

```c
static void keep(event_t *out, const contact_t *c)      /* strictly earlier wins */
{
    if (c->t < out->t || out->kind == EV_NONE) {
        out->kind = EV_CONTACT;
        out->t = c->t;
        out->contact = *c;
    }
}

static void try_square(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    contact_t c;

    if (overlap_box_poly(p->pos, MODES[p->mode].half, h))
        return;                                         /* already inside it: skip */
    if (!sweep_box_poly(p->pos, MODES[p->mode].half, d, h, &c) || c.t > out->t)
        return;
    if (c.flat && c.normal.y * p->gravity_dir > 0.0)
        return;                                         /* met from below: the circle's */
    if (!c.flat && !contact_through_x_axis(&c))
        return;                                         /* a tilted face: the circle's  */
    c.surface = (long)i;
    keep(out, &c);
}

static void try_circle(sim_t *s, size_t i, vec2_t d, event_t *out)
{
    const player_t *p = &s->st.player;
    const hitbox_t *h = &s->lvl.objects[i].hitbox;
    double g = p->gravity_dir;
    contact_t c;

    if (overlap_circle_poly(p->pos, MODES[p->mode].half, h))
        return;
    if (!sweep_circle_poly(p->pos, MODES[p->mode].half, d, h, circle_face_ok,
            circle_vertex_ok, g, &c) || c.t > out->t)
        return;
    c.surface = (long)i;
    keep(out, &c);
}

static bool first_event(sim_t *s, vec2_t d, vec2_t vel, event_t *out)
{
    out->kind = EV_NONE;
    out->t = 1.0;
    for (size_t k = 0; k < s->nb_cand; k++) {
        size_t i = s->cand[k];

        if (OBJ_CATEGORY[s->lvl.objects[i].type] != CAT_NEUTRAL || is_passed(s, i))
            continue;
        try_square(s, i, d, out);              /* keeps it if strictly earlier than out->t */
        try_circle(s, i, d, out);
    }
    try_surfaces(s, d, out);
    try_steps(s, d, vel, out);                 /* G.7; only if it starts strictly earlier */
    return out->kind != EV_NONE;
}
```

### G.10 Square against a circle (saws, FEATURES 10.6)

A saw's hitbox is a disc (center `C`, radius `r`), and only the rigid square ever tests it, for a touch. The square (center `c`, half `h`) moving by `d` touches it when `C` enters the square grown by `r`: a rectangle with rounded corners. Work in the square's frame, `q(t) = C - c - t * d`, and take the earliest of:

```text
straight parts (two axis-aligned rectangles, an ordinary slab sweep):
    |q.x| < h + r and |q.y| < h        /* left and right of the square */
    |q.x| < h     and |q.y| < h + r    /* above and below it           */

corners (the four discs of radius r at the square's corners K = (±h, ±h)):
    f = C - c - K
    a = dot(d, d)                      /* 0: no movement, skip */
    b = dot(f, d)
    cc = dot(f, f) - r * r
    disc = b * b - a * cc
    t = (b - sqrt(disc)) / a           /* the entering root, if disc >= 0 */
```

Keep the candidates with `0 <= t <= 1`, take the smallest, and `t = 0` when the disc is already inside the grown rectangle at the start. Touching exactly (`= r`, or `= h + r` on a slab) isn't a collision (3.0), which is why the slab tests are strict.

### G.11 Hashing (3.3)

FNV-1a over the bytes of each field, in a fixed order, never over the raw struct (padding is uninitialized):

```c
static void hash_bytes(uint64_t *h, const void *p, size_t n)
{
    const uint8_t *b = p;

    for (size_t i = 0; i < n; i++) {
        *h ^= b[i];
        *h *= 1099511628211ULL;
    }
}

static void hash_double(uint64_t *h, double v)
{
    if (v == 0.0)
        v = 0.0;            /* -0.0 and 0.0 are the same state */
    hash_bytes(h, &v, sizeof v);
}
```

Start from `14695981039346656037ULL`. The sim never produces a NaN (G.10), so there's no NaN case to normalize (G.12).

### G.12 Numerical rules

- **Divisions** are guarded at their source: `den < 0` for faces, `a != 0` for the vertex quadratic, `d.x > 0` for the step, `n.y != 0` in `land` and `head_hit` (guaranteed: those run only when `|n.y|` is at least `FLOOR_MIN_DOT`), `da - db != 0` in the clipper. No other division by a computed value exists.
- **`t` is always clamped** to `[0, 1]` before use, and a contact at `t = 0` is legal (already touching and moving in).
- **The only tolerances in the engine** are `CONTACT_SKIN` (1/1024 px: how far the circle is placed off a tilted face, and the `2 ×` growth of the jump zone), `RISE_EPSILON` (1/4096 px/tick: the momentum comparisons of the jump zone and the step), the 1/1024 px grid at load, and the `1e-12` axis-deduplication test at load. Nothing else compares with an epsilon; everywhere else, exact comparisons are the specification.
- **Build with `-ffp-contract=off`** (Phase 1) so no `a * b + c` is fused: with contraction, two compilers can disagree in the last bit and the determinism tests fail.

---

## Appendix H: Build order, file by file

The phases say what the engine does; this says what to write, in the order to write it. Every function here is fully specified somewhere in the plan, and the **Spec** column says where. Each step ends with tests that pass before the next one starts.

Files under `src/sim/` never include an SFML header (Principle: 2.2). Their prototypes live in `include/sim/`, one header per module, except the engine-internal ones (`move`, `zone`, `interact`, `player`, `camera`), which share `include/sim/internal.h` because only the sim calls them.

---

### Step 1: headers, the mode table, the test runner

**`include/sim/constants.h`** — 3.2, as written there. **`include/sim/sim_types.h`** — 3.3: `vec2_t`, `rect_t`, `input_t`, the enums, `hitbox_t`, `object_t`, `level_data_t`, `player_t`, `camera_t`, `ship_bounds_t`, `run_state_t`, `sim_snapshot_t`, `touch_t`, `face_t`, `contact_t`, `event_t`, `sim_t`, and the two `spent` helpers. **`include/sim/sim.h`** — 2.3 (already written).

**`include/sim/modes.h` + `src/sim/modes.c`**

| Function | Job | Spec |
|---|---|---|
| `const mode_ops_t MODES[MODE_COUNT]` | the table, designated initializers | 3.3, FEATURES 6.1 |
| `int mode_from_name(const char *name)` | `"cube"` → `MODE_CUBE`, `-1` if unknown | 5.5 |

**`include/sim/alloc.h` + `src/sim/alloc.c`**

| Function | Job | Spec |
|---|---|---|
| `void *sim_xcalloc(size_t n, size_t size)` | calloc or exit 84; the sim's own copy, so it doesn't depend on the game layer | 3.3 |

**`tests/main.c`** — the `CHECK` macro and the list of test functions (8.1). **`tests/test_constants.c`**

Done when `make` still builds the game, `make test` runs, and these pass: each trigonometric literal equals its `double` recomputation, `CAM_LERP == 1 - exp(-1 / (TICK_RATE * CAM_TAU))` to 1e-9, `PER_TICK(SCROLL_SPEED) == 4.3275`, and a ship held from `vy = 0` settles at exactly `PER_TICK(SHIP_MAX_VY)`.

---

### Step 2: the geometry

Pure functions, no player, no tick. This is the step to be slow on: everything later assumes it's right.

**`include/sim/geom.h`** (static inline): `dot`, `cross`, `vadd`, `vsub`, `vscale`, `vlen`, `rect_overlap`. G.1.

**`include/sim/hitbox.h` + `src/sim/hitbox.c`**

| Function | Job | Spec |
|---|---|---|
| `static double grid(double v)` | round to 1/1024 px | 4.2 |
| `static vec2_t rotate_point(vec2_t p, vec2_t c, double deg)` | rotate around the rect's center | 4.2 |
| `static rect_t bounds_of(const vec2_t *v, int n)` | AABB | G.2 |
| `static void ensure_clockwise(vec2_t *v, int n)` | signed area, reverse if negative | G.2 |
| `static void build_faces(hitbox_t *h)` | `face_kind`, `face_n`, `face_off` per edge | G.2 |
| `static void build_axes(hitbox_t *h)` | keep non-axis-parallel normals, deduplicate, fill `axis_lo/hi` | G.2 |
| `void hitbox_build_poly(hitbox_t *h, const vec2_t *local, int n, rect_t rect, double deg)` | the whole build | 4.2, G.2 |
| `void hitbox_build_circle(hitbox_t *h, rect_t rect, double r)` | saws; a `SHAPE_CIRCLE` hitbox | FEATURES 10.6 |
| `void hitbox_for_object(object_t *o)` | local shape from the type, then `hitbox_build_*` | 4.2 table |
| `vec2_t hitbox_axis(const hitbox_t *h, int k)` | `k = 0` → `(0,1)`, `k = 1` → `(1,0)`, else `axes[k-2]` | G.2 |
| `double hitbox_lo(const hitbox_t *h, int k)`, `hitbox_hi` | the matching projections | G.2 |
| `bool up_facing_horizontal_face(const hitbox_t *h, int i, double g, face_t *out)` | edge `i` if horizontal and facing the player's up | G.7 |

**`include/sim/sweep.h` + `src/sim/sweep.c`**

| Function | Job | Spec |
|---|---|---|
| `bool sweep_box_poly(vec2_t c, double h, vec2_t d, const hitbox_t *hb, contact_t *out)` | the square's contact sweep | 4.3 |
| `double sweep_box_touch(vec2_t c, double h, vec2_t d, const hitbox_t *hb)` | first touch time, `INFINITY` if none | G.3 |
| `bool overlap_box_poly(vec2_t c, double h, const hitbox_t *hb)` | static overlap, touching excluded | G.3 |
| `bool sweep_circle_poly(vec2_t c, double r, vec2_t d, const hitbox_t *hb, face_ok_fn fok, vertex_ok_fn vok, double g, contact_t *out)` | the circle's contact sweep, filtered | G.4, G.9 |
| `double sweep_circle_touch(vec2_t c, double r, vec2_t d, const hitbox_t *hb)` | same, unfiltered, time only | G.4 |
| `double poly_distance(vec2_t c, const hitbox_t *hb)` | distance from a point to a convex polygon, 0 inside | G.4 |
| `bool overlap_circle_poly(vec2_t c, double r, const hitbox_t *hb)` | `poly_distance < r` | G.4 |
| `int clip_half_plane(const vec2_t *in, int n, double y_line, double g, vec2_t *out)` | Sutherland–Hodgman, one plane | G.6 |
| `bool sweep_box_plane(vec2_t c, double h, vec2_t d, double sy, double side, contact_t *out)` | the square against the ground or a corridor boundary | G.5 |
| `bool sweep_circle_plane(vec2_t c, double r, vec2_t d, double sy, double side, contact_t *out)` | the same for the circle | G.5 |
| `double sweep_box_disc(vec2_t c, double h, vec2_t d, vec2_t C, double r)` | saws: slabs and corner discs | G.10 |

**`tests/test_hitbox.c`, `tests/test_sweep.c`** — done when: a block at 0/90/180/270° has exactly axis-aligned vertices and only flat faces; at 30° only tilted ones; a 45° `slope` has one face of each kind and a `slope` at 180° a horizontal top; a square resting exactly on a face doesn't overlap it and a square 1/1024 px into it does; a square fully inside a block overlaps; a square sliding along a top face reports no contact, and the same square moving down into it reports `t` with `flat` true and the face's exact `offset`; a circle falling on a 45° face stops with its center 50 px from the surface; the 100 px gap of 8.2 passes; a 10 px spike is hit at 8 px/tick.

---

### Step 3: the loader

**`include/sim/level.h` + `src/sim/level_parse.c`**

| Function | Job | Spec |
|---|---|---|
| `int level_parse_mem(const char *buf, size_t len, object_t **objs, size_t *count, level_header_t *hdr, sim_log_fn log)` | text → objects, one warning per bad line, never fatal | 7.3 |
| `static int parse_object_line(char *line, int lineno, object_t *out, sim_log_fn log)` | `type x y size [word] [key=value...]` | 7.2 |
| `static int parse_kv(const char *tok, object_t *o, sim_log_fn log)` | `rot=`, `w=`, `h=`, `group=` | 7.2 |
| `static bool parse_double(const char *s, double *out)`, `parse_int` | reject `2.5` where an int is expected, `nan`, `inf`, trailing junk | 7.3 |

**`src/sim/level_build.c`**

| Function | Job | Spec |
|---|---|---|
| `static int cmp_object(const void *a, const void *b)` | by `hitbox.aabb.x`, then `line` | 4.1 |
| `void level_finalize(level_data_t *lvl)` | hitboxes, sort, `reach`, `end_shift`, `kill_y` | 4.1, 3.4, 4.7 |
| `int sim_load_mem(sim_t *s, const char *buf, size_t len, const char *name, sim_log_fn log)` | parse, finalize, allocate `spent`, `sim_reset` | 2.3 |
| `int sim_load(sim_t *s, const char *path, sim_log_fn log)` | read the file, then `sim_load_mem` | 2.3 |
| `void sim_free(sim_t *s)` | objects and `spent` | 2.3 |

**`tests/test_parser.c`** — done when the 8.1 parser cases pass: comments, blank lines, `\r\n`, missing fields, unknown type, unknown portal mode, `size 0` and negatives rejected with file and line, a legacy header line ignored, no trailing newline, and a 10 000-line level loading with the objects sorted.

---

### Step 4: the tick, cube on flat ground

The first version moves a cube on the ground and on block tops. No circle, no slopes, no steps, no orbs.

**`include/sim/internal.h` + `src/sim/player.c`**

| Function | Job | Spec |
|---|---|---|
| `void player_update_hold(player_t *p, input_t in)` | none / fresh / used | 3.4 |
| `void player_apply_input(player_t *p, input_t in)` | the cube's jump, the ship's thrust | 3.4 |
| `void player_apply_gravity(player_t *p)` | gravity, the fall cap, the grounded exception | 3.4 |
| `void player_flip_gravity(player_t *p)` | flip and negate `vy` | 3.4 |
| `void player_update_rotation(player_t *p)` | cosmetic; a stub returning immediately is fine here | 9.4 |

**`src/sim/move.c`** (the heart)

| Function | Job | Spec |
|---|---|---|
| `static void broadphase(sim_t *s, rect_t sweep)` | fill `cand`, advance `first_active` | 4.1 |
| `static rect_t tick_sweep_bounds(const player_t *p)` | the tick's AABB | G.8 |
| `static void advance(sim_t *s, vec2_t d, double t)` | the only place the player moves; rebuilds `distance` from the tick's fraction | G.8 |
| `static void keep(event_t *out, const contact_t *c)` | strictly-earlier wins | G.9 |
| `static void try_square(sim_t *s, size_t i, vec2_t d, event_t *out)` | the square's filter | G.9 |
| `static void try_surfaces(sim_t *s, vec2_t d, event_t *out)` | ground, corridor floor and ceiling | G.5, G.9 |
| `static bool first_event(sim_t *s, vec2_t d, vec2_t vel, event_t *out)` | the earliest event | G.9 |
| `static void settle(player_t *p, const contact_t *c, double half)` | exact placement | 4.4 |
| `static void land(player_t *p, const contact_t *c, vec2_t *vel)` | support, `surface_rise` | 4.4 |
| `static void head_hit(player_t *p, const contact_t *c, vec2_t *vel)` | die, bounce, or stop on a surface | 4.4 |
| `static void pass_into(sim_t *s, const contact_t *c)` | the passed list | 4.4 |
| `static void respond(sim_t *s, const contact_t *c, vec2_t *vel)` | floor / ceiling / wall | 4.4 |
| `static bool leg_deaths(sim_t *s, vec2_t d, double t_end)` | inner box and spikes along the leg | 4.4, 4.5 |
| `static bool crossed_surface(const sim_t *s)` | the safety net | G.8 |
| `void move_and_collide(sim_t *s)` | the leg loop | 4.4 |

**`src/sim/camera.c`**: `void camera_follow(camera_t *c, const player_t *p, const ship_bounds_t *b)` — 3.5.

**`src/sim/hash.c`**: `hash_bytes`, `hash_double`, `sim_state_hash`, `sim_physics_hash` — G.11.

**`src/sim/sim.c`**: `sim_reset`, `sim_tick`, `sim_percent`, `sim_snapshot_init/save/restore/free` — 3.4, 3.3.

**`tests/test_tick.c`** — done when: a jump from flat ground has apex 213.32 ± 0.05 px, 102 ticks and 441.4 px; `distance` grows by exactly `vx` every tick; running across 20 adjacent blocks stays grounded with no death; a player spawned inside a block dies on tick 1; running into a tall block's side dies exactly 30 px in; the same input script twice gives the same `sim_state_hash` every tick; a snapshot saved at tick 500, restored and replayed gives the same hashes.

---

### Step 5: the circle, slopes, steps and the jump zone

**`src/sim/move.c`** (added to)

| Function | Job | Spec |
|---|---|---|
| `static bool circle_face_ok(const hitbox_t *h, int i, double g)` | tilted, or facing the ceiling | G.9 |
| `static bool circle_vertex_ok(const hitbox_t *h, int i, double g)` | between two tilted faces, or a ceiling corner | G.9 |
| `static void try_circle(sim_t *s, size_t i, vec2_t d, event_t *out)` | the circle's candidates | G.9 |
| `static bool step_event(const sim_t *s, vec2_t d, vec2_t vel, face_t f, double *t)` | when the step happens inside the leg | 4.4, G.7 |
| `static void try_steps(sim_t *s, vec2_t d, vec2_t vel, event_t *out)` | the highest step, over every candidate's up-facing faces | 4.4 |
| `static bool lift_hits_surface(const sim_t *s, double y, double lift)` | refuse a lift into a boundary | G.7 |
| `static bool circle_hits_neutral(sim_t *s, vec2_t lift)` | the lift's ceiling check | 4.4 |
| `static void step_up(sim_t *s, const face_t *f, vec2_t *vel)` | the lift and the landing | 4.4 |

**`src/sim/zone.c`**

| Function | Job | Spec |
|---|---|---|
| `static bool zone_dark(const sim_t *s, const hitbox_t *h)` | clip, then distance to the circle | G.6 |
| `static bool zone_light(const sim_t *s, const hitbox_t *h)` | flat faces against the zone rectangle | G.6 |
| `static bool zone_surface(const sim_t *s)` | the ground and corridor boundaries in the zone | G.6 |
| `void update_can_jump(sim_t *s)` | the zone plus the momentum test | 4.3 |

**`tests/test_slopes.c`, `tests/test_steps.c`, `tests/test_zone.c`** — done when the 8.2 bullets for slopes, seams, ledges, step-up, "step-up needs downward momentum", face kinds, the jump zone and "no jump buffer" pass, at 0.5×, 1× and 4× speed.

---

### Step 6: harm, interactive objects, portals, corridors

**`src/sim/interact.c`**

| Function | Job | Spec |
|---|---|---|
| `void leg_touches(sim_t *s, vec2_t d, double t_end)` | collect live interactive objects with their path position | 4.6 |
| `static int touch_cmp(const void *a, const void *b)` | `(at, index)` | 4.6 |
| `static bool interactive_wants_activation(const sim_t *s, const object_t *o)` | portal: always; orb: a fresh hold | 5.1 |
| `static void interactive_act(sim_t *s, const object_t *o)` | the effect | 5.1 |
| `static void touch_interactive(sim_t *s, size_t i)` | act once, then spend | 5.1 |
| `void apply_interactive(sim_t *s)` | sort and apply, tick step 5 | 4.6 |
| `static void center_corridor(sim_t *s, const object_t *portal)` | the corridor from the portal and the camera lock | 5.2 |
| `static void enter_portal(sim_t *s, const object_t *o)` | the mode change | 5.2 |

Spikes need nothing new: `leg_deaths` already sweeps the rigid square against `CAT_HARM` (step 4).

**`tests/test_interact.c`** — done when: a portal acts exactly once per crossing and is skipped afterwards; the corridor bounds match the examples of 5.2 and the camera stays locked; a same-mode portal switches corridors; the old boundaries stop existing on that tick; `vy`, position and gravity are untouched by a portal; a spike touched only by the square's corner kills, and one exactly grazing it doesn't.

---

### Step 7: the game layer

Now the game can run the new engine: `handle_playing` with the integer accumulator (3.6), `input_for_tick` (FEATURES 1), the views and letterboxing (9.1, 9.7), the level's vertex buffers (9.2), the player (9.4) and the corridor strips (5.3). Then death, respawn, attempts and progress (Phase 6).

**Build the F3 overlay (9.6) as soon as a cube moves on screen**, before anything else in Phase 9 is polished: the hitboxes, the contacts of the last tick, the jump zone and the numbers are what make every later question answerable by looking.

---

### Step 8 and after

The bot (8.3) and CI (8.5), then the modes and objects of FEATURES in their own order (6.7's checklist per mode), then the editor. By then the engine is fixed and everything else is a table row or a scene.

---

## Footnote: a future fork with another graphics library

**This project uses CSFML, and nothing in this plan changes because of this note.** It's only a reminder that a later fork may replace CSFML with a lower-level, faster library (SDL3 with its GPU API is the leading candidate: C, modern GPU backends, shaders and compute for effects, window, input and audio included). The goal is that such a fork rewrites a few modules instead of the whole game.

The simulation already makes most of that true: `src/sim/` never includes an SFML header (Phase 2), so physics, levels, progress, the bot and the tests would carry over unchanged. The only extra habit, at no cost here:

- **Keep `sf*` calls inside a few thin modules**: window and views (`window.c`), drawing (`level_render.c`, `atlas.c`, `debug_overlay.c`, the UI widgets' draw functions), input (`input.c`), audio (`audio.c`). Scenes, menus, the level's game rules (death, respawn, progress) and the editor's logic call those modules, not CSFML directly.
- **Use the game's own types outside those modules**: `vec2_t`, `rect_t`, colors as `uint8_t` RGBA, textures and sounds referred to by ids (`TEX_BLOCK`, `SFX_DEATH`), key and button bindings as the game's own enum (PLAN 10.2 already stores them by name). A widget or a scene holds data, and asks the drawing module to draw it.
- **Render from data, not from long-lived library objects scattered everywhere**: the static vertex buffers of 9.2 are built from `object_t`s; a fork builds its own GPU buffers from the same data.

When unsure, prefer the simpler code: this is a guideline for where CSFML calls live, not a reason to add abstraction layers now.

