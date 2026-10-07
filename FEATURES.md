# My_GD — Feature Plan (v2, precise)

Companion to **PLAN.md**. Covers music selection, the options screen, new gamemodes (UFO, wave, ball and more), and the level editor.

## How this version differs

v1 described each feature. This version specifies each one precisely enough to implement without guessing:

- **Exact behavior**: what happens on every input, in every state, at every edge case.
- **Exact data**: file grammars, struct fields, value ranges, defaults.
- **Step-by-step algorithms** with the order of operations, and why that order.
- **Numbers that were measured**, not estimated. The gamemode physics, direction-aware collision rules and the test levels were run in a 240 Hz prototype (`core_prototype/modes_proto.c`, added to `my_gd_lab.zip`). Every "measured" value below comes from it.
- **Acceptance criteria** you can check, for each feature.

References like "PLAN 3.4" point to PLAN.md.

**v3** (matching PLAN v3, the semi-physics engine): modes only change the velocity (`apply_input`, `apply_forces`), the engine moves the player and resolves contacts (6.1–6.5); head hits bounce the ship, UFO and ball (30%) and kill the cube and wave, except against the ground and corridor boundaries: surfaces the player can't cross, which never kill anyone; corridors are snapped to the grid and lock the camera; pads and orbs set `vy`, speed portals set `vx`; saws are circle harm objects (10.6); `slope` and `saw` join the grammar. Input follows GD's hold rule (PLAN 3.4): no press memory or time buffer; a hold is fresh until it jumps, flips, hops or activates an orb; used holds keep jumping off surfaces but ignore orbs; orbs win over surface jumps. Gravity is a constant acceleration whose sign is `gravity_dir`; gravity portals **only** flip it, through PLAN 3.4's `player_flip_gravity`, which keeps the player's motion (no speed reset); a ball click flips it the same way and adds a small push off the surface it leaves (9.2); while blue pads and blue orbs flip it and then set a speed toward the new floor, and green orbs flip it and then jump away from it (10.1, 10.2). Physics constants are GD's own, written in GD's velocity units (6.9).

**v2.2** (review pass, matching PLAN v2.2): the sim is level data + a snapshot-able run state (`s->st.*`); every object can be rotated by any angle and sized with `w`/`h`; a fixed kill ceiling replaces the camera-relative out-of-bounds rule; all jump inputs (including the left mouse button and a gamepad) are rebindable; same-mode portals switch to the new corridor; pads have thin hitboxes; orbs have an explicit tick step; the mini portal is `mini`; start positions carry any mode, gravity and speed; the bot's results are information, never a blocker; an audio offset setting compensates output latency.

---

## Table of contents

- [0. Conventions and roadmap](#0-conventions-and-roadmap)
- [1. Input: held, pressed, and the tick boundary](#1-input-held-pressed-and-the-tick-boundary)
- [2. Settings store](#2-settings-store)
- [3. UI toolkit](#3-ui-toolkit)
- [4. Music selection](#4-music-selection)
- [5. Options screen](#5-options-screen)
- [6. Gamemode framework](#6-gamemode-framework)
- [7. UFO](#7-ufo)
- [8. Wave](#8-wave)
- [9. Ball and gravity direction](#9-ball-and-gravity-direction)
- [10. More modes and objects](#10-more-modes-and-objects)
- [11. Level editor](#11-level-editor)
- [12. Level format v2 grammar](#12-level-format-v2-grammar)
- [13. Test plan with expected values](#13-test-plan-with-expected-values)
- [14. Milestones and acceptance criteria](#14-milestones-and-acceptance-criteria)

---

## 0. Conventions and roadmap

### 0.1 Conventions used everywhere

| Term | Meaning |
|---|---|
| Tick | One simulation step. `TICK_RATE = 240` (PLAN 11.1), so 1 tick = 4.17 ms |
| Frame | One rendered image. Its length depends on the monitor; 0 to many ticks run per frame |
| `PER_TICK(v)` | px/s → px/tick: `v / 240` |
| `PER_TICK2(a)` | px/s² → px/tick²: `a / 57600` |
| World y | Grows downward. Ground surface at `GROUND_Y = 850` |
| `vy` | px/tick, **away from the floor in the player's own gravity** (up when gravity is normal) |
| `gravity_dir` | `+1` normal (falls toward +y), `-1` flipped |
| Scroll speed | GD's normal speed, 1038.6 px/s = 4.3275 px/tick (6.9) |
| Block | 100 px (a `size 2` object); one grid unit is 50 px |

All constants are written in px/s or px/s² and converted with the macros, so they read the same whatever the tick rate.

Code conventions: simulation state is `s->lvl` (immutable level data) and `s->st` (run state), PLAN 3.3. Every header is in `include/` (simulation headers in `include/sim/`).

### 0.2 Prerequisites from PLAN.md

| Needed | PLAN | Why it's a prerequisite |
|---|---|---|
| Pure simulation in `src/sim/` | 2 | Modes, the editor's playtest and the bot run the sim without a window |
| Integer `tick` counter | 3.2 | Music position is computed from it |
| Run-state snapshots | 3.3 | Practice checkpoints, editor start positions, the bot |
| Rotation and `w`/`h` | 4.2, 7.2 | Ceiling spikes, pads, the editor's rotate tool |
| Deferred scene switching | 10.1 | Options and editor are scenes; switching mid-event would free live data |
| Progress store | 6.4 | Per-level song choice is stored there |
| Header lines in levels | 7.2 | `music`, `bpm`, `version` |
| Bot | 8.3 | Proves test levels and edited levels are beatable |
| 240 Hz with the matched cube arc | 3.2, 11.1 | Mode constants below are tuned at 240 Hz |
| The semi-physics engine | 3–4 | Modes only provide impulses and forces; the engine's sweep and contact rules (already gravity-aware) do the rest (6.4) |

### 0.3 Order

```text
1  Input edges          small    needed by UFO, ball, orbs, editor shortcuts
2  Settings store       small    needed by options and music volume
3  UI toolkit           medium   needed by options, song picker, editor
4  Music selection      medium
5  Options screen       medium
6  Gamemode framework   medium   includes gravity direction
7  UFO, wave            small each
8  Ball                 small    (framework did the hard part)
9  Pads, orbs           small each   (10.1, 10.2: done before the editor)
10 Level editor         large, in 7 usable stages
11 Saws, speed portals, robot, swing, spider, mini   small each
```

Pads and orbs were moved ahead of the editor: they are simulation work like the modes, and the editor's palette is built from the object table, so what exists before it starts is in it for free.

Why the modes come before the editor: the editor's palette, properties and playtest are all generated from the mode table (6.2). Build the table first, and the editor supports every mode for free. New modes don't need the editor to be tested: their test levels are five lines of text, and the bot checks them.

---

## 1. Input: held, pressed, and the tick boundary

### 1.1 The problem, precisely

The cube only cares whether the button is **down** ("held"). The UFO, ball, orbs and editor shortcuts care about **presses**: the moment the button goes from up to down. The game only lives in 240 Hz ticks; frames are a rendering detail. But the loop runs ticks in bursts: it reads the window once per frame, then runs the 4 ticks that frame owes (at 60 FPS) one after the other, in a fraction of a millisecond. Anything read during the burst is the same for all 4 ticks, so a click can't be placed in the tick it happened in:

1. **Several ticks per frame.** If "pressed" is read per frame and given to all 4 ticks, a UFO jumps 4 times from one click; given to the first only, a click at the end of the frame is moved up to 3 ticks early.
2. **Zero ticks in a frame.** On a fast monitor (e.g. 500 FPS), some frames run no tick: what a frame sees isn't any tick's.
3. **Events have no time.** SFML's events say what happened, not when: everything is "this frame".

### 1.2 Specification

- Every tick covers its own 1/240 s of real time: tick *k* of an attempt is `[start + k/240 s, start + (k+1)/240 s)`.
- `pressed` for a tick = a jump input went **down inside that tick's 1/240 s**. Several presses inside one tick are one `pressed`, and nothing is carried over to the next tick. A press that falls in no tick's time (a pause, a hitch the loop skipped, before the attempt) presses nothing.
- `held` for a tick = a jump input is down at the end of that tick's time. A press and release inside one tick gives `pressed` without `held`.
- Frames don't matter: the same presses at the same times give the same ticks at 30, 60 or 500 FPS.
- OS key repeat must not create extra presses.

### 1.3 Implementation

A thread polls the jump inputs every millisecond and stamps each **change** with the monotonic clock; the loop gives each tick the changes stamped inside its own time.

```c
typedef struct input_change {
    int64_t at_us;                 /* when the poll saw it */
    unsigned int down;             /* bit i: jump input i is down */
} input_change_t;
```

The queue between them is a single-writer, single-reader ring (`src/sim/input_ticks.c`, pure C, so the tests drive it with made-up stamps): no lock, 256 changes. The thread also publishes `polled_us`, the time up to which every change is already in the queue:

```c
for (;;) {                                  /* the polling thread, in src/input.c */
    int64_t now = input_clock_us();         /* the stamp, taken before the poll */
    unsigned int down = buttons_down();     /* sfKeyboard / sfMouse, every jump input */

    if (down != queued && input_queue_push(&q, (input_change_t){now, down}))
        queued = down;                      /* full: pushed again next poll */
    input_queue_publish(&q, now);
    sleep_until_next_ms();
}
```

The loop (PLAN 3.6) runs ticks up to `polled_us` rather than to the clock, so a change can never arrive after its tick has run. What's left in the accumulator says how far `now` is past the last tick's end, so each tick knows its own window, in microseconds x `TICK_RATE`:

```c
while (lv->accumulator >= 1000000) {
    int64_t end = now * TICK_RATE - lv->accumulator + 1000000;

    lv->accumulator -= 1000000;
    level_step(lv, gd, input_for_tick(gd, end - 1000000, end));
}
```

The reader takes every change stamped before `end`: one stamped inside the window that puts a new input down is the press; one stamped before the window only updates what's down.

```c
input_t input_read_tick(input_reader_t *r, input_queue_t *q, int64_t start, int64_t end)
{
    input_change_t c;
    bool pressed = false;

    while (next_change(q, end, &c)) {       /* stamped before `end` */
        if (c.at_us * TICK_RATE >= start && (c.down & ~r->down & ~r->latched))
            pressed = true;
        r->down = c.down;
        r->latched &= c.down;
    }
    return (input_t){(r->down & ~r->latched) != 0, pressed};
}
```

- **Key repeat** doesn't exist for polling: a key held for 2 s is one change. `sfRenderWindow_setKeyRepeatEnabled(w, sfFalse)` is still set once after creating the window, for the UI (5).
- **Another input** going down while one is held is a new press; a release never is.
- **The click that starts a level** (`latched`): Play and Retry are left clicks, usually still down on tick 0. The mouse inputs start latched and count for nothing until they're seen up; the keyboard never is (PLAN 3.6).
- **Threads and X.** SFML 2.6 polls through the X display its window uses and never makes Xlib thread safe, so `main` calls `XInitThreads()` before anything else (as SDL does). Measured: a poll of Space, Up and the left button takes about 0.28 ms, and the thread holds 1 kHz, so a stamp is at most about 1.3 ms off, against a 4.17 ms tick.
- The thread runs while a level is open (`input_start` in `level_start`, `input_stop` in `level_free`); menus read events as before.

The same table of jump inputs is polled for every bit, so `held` and `pressed` can never disagree about which inputs jump. PLAN 10.2's bindings (FEATURES 2.2) replace the table.

### 1.4 How each mode reads input

| Mode | Uses | Rule |
|---|---|---|
| Cube | `held` or `pressed` | Jump if `(held || pressed) && can_jump`. Including `pressed` makes a tap inside one tick on the ground still jump. Uses the hold |
| Ship | `held` or `pressed` | Thrust while down; a tap inside one tick still gives one tick of thrust. Never uses the hold (always fresh) |
| UFO | `pressed` | One hop per press, anywhere. Uses the hold |
| Wave | `held` or `pressed` | Up while down; a tap inside one tick still gives one tick up. Never uses the hold (always fresh) |
| Ball | `held` or `pressed` | Exactly like the cube, but it flips its gravity instead of jumping (9). Uses the hold |
| Orbs | a **fresh** hold | Activate when touched with a fresh hold, even one started before the contact; takes priority over the surface jump (10.2). Uses the hold |

**Holds (PLAN 3.4), the rule behind this table.** A hold is fresh from its press (a hold carried into an attempt is fresh too). A *buffered click* is a hold started before a contact and still down at the contact: it acts at the contact, and a press released before it does nothing (no time window, no forgiveness). Jumping off a surface, flipping the ball, a UFO hop or an orb **uses** the hold: while it stays down, the player keeps jumping (or flipping) on every surface it lands on, but it activates no orb until it's released and pressed again. Ship and wave holds never get used.

### 1.5 Clicks that belong to the UI

A left click on the pause button or the end screen must not also count as a jump. Polling can't tell a UI click from a jump, but it doesn't have to: the UI reads the frame's events **before** the ticks that cover the click run, and every UI click in a level stops those ticks. Pausing stops ticking, and resuming restarts the tick windows at the resume time, so a press stamped before it presses nothing (1.2). The end screen only exists once the sim has stopped. Retry and Play start a new level, whose mouse inputs start latched until released (1.3). A future widget that's clicked while ticks keep running would need the same treatment: drop the presses stamped before its click was handled. The UI is always clicked with the left mouse button, whatever the jump bindings are.

### 1.6 Tests

In `tests/test_input.c`, on the queue and the reader alone, with made-up stamps:

- A press inside tick 1 → `pressed` on tick 1 only, `held` from tick 1 on.
- Two clicks inside one tick → one `pressed`, nothing on the next tick. Clicks in two ticks in a row → two.
- Press + release inside one tick → that tick has `pressed = true, held = false`; the next has neither.
- Held for 2 s → exactly one press, 480 ticks held.
- The same changes queued all up front or just in time → the same ticks (frames don't matter).
- A change exactly on a tick's end → the next tick's.
- A press stamped before the tick's window → held, not pressed.
- The latched click: neither held nor pressed until released; a key meanwhile still is.
- Another input going down while one is held → a press; one going up → not.
- A full queue refuses a change until the reader frees a slot.

---

## 2. Settings store

### 2.1 File format

`save/settings.txt`. Grammar:

```text
file    = { line }
line    = blank | comment | pair
comment = "#" { any char } newline
pair    = key "=" value newline         (spaces around key and value are trimmed)
key     = [a-z_]+
value   = any chars up to end of line
```

### 2.2 Keys

| Key | Type | Range | Default | Used by |
|---|---|---|---|---|
| `music_volume` | int | 0–100 | 80 | music manager |
| `sfx_volume` | int | 0–100 | 100 | sound bank |
| `menu_song` | file name in `music/` | a plain name (no folder, not hidden); whether it exists is checked when it's played (4) | `menu_loop.ogg` (4.2) | menus |
| `fullscreen` | bool (0/1) | | 0 | window |
| `window_width` | int | 640–7680; the window is then capped to the desktop | 1280 | window |
| `window_height` | int | 360–4320; capped to the desktop | 720 | window |
| `vsync` | bool | | 1 | window |
| `fps_limit` | int | 0, 60, 120, 144, 240 | 0 | window (ignored while vsync is on) |
| `show_percent` | bool | | 1 | HUD |
| `show_progress_bar` | bool | | 1 | HUD |
| `show_attempts` | bool | | 1 | HUD |
| `jump_bindings` | comma list of binding names (keys, `MouseLeft`/`MouseRight`/`MouseMiddle`, `Joy0`..`Joy15`) | 1–6 bindings, each once, never `Escape` | `Space,Up,MouseLeft,Joy0` | input (1.3) |
| `restart_key` | binding name, or empty for unbound | not a jump binding, not Escape | `R` | level (PLAN 6.5) |
| `checkpoint_key` | binding name, or empty | not a jump binding, not Escape, not `restart_key` | `Z` | practice (PLAN 13.1) |
| `remove_checkpoint_key` | binding name, or empty | not a jump binding, not Escape, not an earlier action key | `X` | practice |
| `audio_offset_ms` | int | −300–300 | 0 | music sync (4.8) |

### 2.3 Loading rules

1. Start from defaults (`settings_defaults`).
2. Read line by line; spaces around the key and the value are trimmed, `#` lines and blank lines skipped. For each `pair`, find the key in a table; parse; validate against the range; if invalid, keep the default and log `save/settings.txt:7: fps_limit=75 invalid, using 0`. A key given twice: the last one wins. A line that isn't `key=value` with a `[a-z_]+` key is logged and dropped.
3. Unknown keys: **keep them** in a list of raw lines and write them back unchanged on save. A newer version's settings survive being opened by an older build.
4. Missing file: defaults, no warning. Unreadable file (no read permission, a read error, not a regular file): defaults, one warning, and the file is **never overwritten** (`unreadable`), so a permission problem can't turn into lost settings.
5. Then the action keys, in table order (`restart_key`, `checkpoint_key`, `remove_checkpoint_key`): one that is also a jump binding, or the same as an earlier action key, falls back to its default, or is left unbound if the default is taken too; logged. This runs after the whole file, so the order of the lines doesn't matter.

Binding names are SFML's, any case: `A`–`Z`, `Num0`–`Num9`, `Space`, `Up`, `F1`–`F15`, `Numpad0`–`Numpad9` and every other `sfKeyCode` name; `MouseLeft`, `MouseRight`, `MouseMiddle`; `Joy0`–`Joy15`. They live in `include/sim/binding.h` as an X-macro list in `sfKeyCode`'s order, and `input.c` checks each one against `sfKey<name>` with a `_Static_assert`, so the stored codes are SFML's without a translation table. Gamepad buttons are read and saved, but not polled yet: SFML refreshes joysticks while the main thread reads its events, which the input thread (1.3) can't share.

A table keeps parsing and saving in one place, so a new setting is one line (`src/sim/settings.c`):

```c
typedef enum setting_type { ST_INT, ST_BOOL, ST_FILE, ST_BINDING, ST_BINDINGS } setting_type_t;

typedef struct setting_def {
    const char *key;
    setting_type_t type;
    size_t offset;              /* offsetof(settings_t, field) */
    int min;
    int max;
    const int *choices;         /* ST_INT: the only values allowed (fps_limit), or NULL */
} setting_def_t;

static const setting_def_t DEFS[] = {
    {"music_volume", ST_INT, offsetof(settings_t, music_volume), 0, 100, NULL},
    {"sfx_volume", ST_INT, offsetof(settings_t, sfx_volume), 0, 100, NULL},
    {"fullscreen", ST_BOOL, offsetof(settings_t, fullscreen), 0, 1, NULL},
    /* ... */
};
```

`offsetof` gives the field's position inside the struct, so one generic function reads and writes every setting. A value is only stored once it's valid: a rejected one never touches the field.

What uses them today: the window (size, fullscreen, vsync, frame limit) at startup, the music volume, the three HUD toggles, the jump bindings (read by the input thread when a level starts) and the restart key. The others wait for their features: `sfx_volume` (there are no sounds yet), `menu_song` (4), the checkpoint keys (practice) and `audio_offset_ms` (4.8).

### 2.4 Saving rules

- Save when leaving the options screen (not on every slider step: dragging a slider would write the file 100 times).
- Save on quit. Until the options screen exists, that's what writes the file with its defaults the first time, so the player has something to edit.
- Atomic: write `settings.txt.tmp`, `fsync`, `fclose` returns 0, `rename`, the folder made if needed. `src/sim/save_file.c` does it for both stores, the progress and the settings.
- Written in the table's order, then the unknown keys; comments aren't kept.

### 2.5 What never goes in settings

Anything the **simulation** reads. Settings differ between players; if they changed physics, a level beatable for one player might not be for another, and replays would desync. The sim gets its inputs from `input_t` and the level file only.

---

## 3. UI toolkit

### 3.1 Coordinate space

All widgets live in the UI view: 1920×1080 logical pixels, letterboxed (PLAN 9.7). Every mouse position is converted before use:

```c
sfVector2f ui_mouse(gd_t *gd)
{
    sfVector2i px = sfMouse_getPositionRenderWindow(gd->w);

    return sfRenderWindow_mapPixelToCoords(gd->w, px, gd->ui_view);
}
```

Forgetting this is the classic bug: widgets work at 1920×1080 and break when the window is resized.

### 3.2 Widget states

Every widget has one visual state, computed each frame:

| State | Condition | Look |
|---|---|---|
| Idle | none of the below | normal |
| Hover | mouse inside bounds, no button down | scale 1.05 |
| Focused | keyboard focus is on it | outline |
| Pressed | left button went down inside it and is still down | scale 0.92 |
| Disabled | `enabled == false` | 50% alpha, ignores input |

### 3.3 Activation rule

A button activates on **release**, and only if the release happens inside the same widget the press started in. This lets the player cancel a click by dragging away, and it matches GD. Keyboard: Enter or Space on the focused widget.

### 3.4 Event routing

For each event, in order:

1. If a modal dialog is open (confirmation, key capture), it gets the event; stop.
2. If a widget has **mouse capture** (a slider being dragged), it gets mouse moves and the release, even outside its bounds; stop.
3. Keyboard navigation: Up/Down move focus (skipping disabled widgets, wrapping around); Left/Right go to the focused widget (sliders and cyclers change value; on a button they move the focus like Up/Down, so a dialog's side-by-side buttons are reachable); Enter/Space activate; Escape goes to the screen's "back" action.
4. Mouse: find the widget under the mouse; hover moves focus to it; press starts capture or pressed state.
5. The jump never goes through events: its own thread polls it (1.3). A UI click in a level stops the ticks that would cover it (1.5).

### 3.5 The widgets

The toolkit has two halves, like the input (1.3) and the settings (2):

- **The core** (`include/ui/ui.h`, `src/ui/ui.c`): pure C, no SFML. Focus, hover, presses, capture, activation, the slider's grid, the cycler, the list, key repeat and the key capture modal. The tests drive it with made-up events (`tests/test_ui.c`).
- **The window side** (`src/ui_sfml.c`): `ui_from_sf` turns an `sfEvent` into a `ui_event_t`, with the mouse already in the UI view's pixels (3.1); `ui_draw` draws a screen with one shape and one text, created once (`ui_gfx_create`).

The core's events are bindings (FEATURES 2), so a key capture gets keys, mouse buttons and gamepad buttons the same way:

```c
typedef enum ui_event_kind { UI_MOVE, UI_PRESS, UI_RELEASE, UI_WHEEL } ui_event_kind_t;

typedef struct ui_event {
    ui_event_kind_t kind;
    binding_t input;               /* a key, mouse button or gamepad button */
    float x;                       /* the mouse, UI pixels */
    float y;
    int wheel;                     /* notches, up positive */
    bool shift;
} ui_event_t;

typedef enum widget_kind { W_BUTTON, W_TOGGLE, W_SLIDER, W_CYCLER, W_KEYBIND, W_LIST } widget_kind_t;

struct widget {
    widget_kind_t kind;
    const char *label;
    ui_rect_t bounds;              /* UI coordinates */
    bool enabled;
    int *value;                    /* toggle 0/1, slider min..max, cycler and list index */
    binding_t *binding;            /* keybind */
    int min;
    int max;
    int step;
    const char *const *choices;    /* cycler labels and list rows, `max + 1` entries */
    int scroll;                    /* list: the first row shown */
    widget_fn on_change;           /* value changed: (ctx, widget) */
    widget_fn on_activate;         /* a button, or a list's chosen row */
    capture_fn on_capture;         /* a keybind's result; NULL: assigned as is */
};
```

Callbacks get the screen's `ctx`, not `gd_t`, and **must never free the screen they're called from**: a scene that changes sets a flag and switches after its events (PLAN 10.1).

Slider: pressing it captures the mouse, and the value follows it anywhere until the release, on the step grid and capped at `max` (a step that doesn't divide the range still reaches `max`). Left/Right change it by `step`.

```c
static void slider_follow(ui_screen_t *ui, widget_t *w, float x)
{
    float t = (x - w->bounds.x) / w->bounds.w;
    int step = w->step > 0 ? w->step : 1;
    int v;

    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    v = w->min + (int)roundf(t * (float)(w->max - w->min) / (float)step) * step;
    set_value(ui, w, v > w->max ? w->max : v);    /* calls on_change only if it changed */
}
```

`on_change` is only called when the value actually changes, so dragging doesn't spam volume updates with the same value.

**Held arrows repeat** every 80 ms after a 300 ms delay, on `ui_update`'s clock, since OS key repeat is off (1.3): Up/Down move the focus (or a list's selection), Left/Right change a slider or a cycler, or move the focus from a button. A hitch replays at most 4 repeats.

Cycler: `< label >`. Left/Right move through `choices`, wrapping around; a click on its left third goes back, anywhere else forward, and Enter goes forward.

Key capture: activating it starts the modal ("Press a key, mouse button or gamepad button for Jump 2", "Esc to cancel, Backspace to clear"), drawn over a dimmed screen. Only the next **press** counts, so the click or Enter that opened it doesn't. Escape cancels, Backspace gives `BIND_NONE` (clear), anything else is the result. With `on_capture` set, the screen decides (5.5: swaps, the last jump binding); without it, the binding is assigned.

List: rows of `UI_ROW_H` (60 px), as many as fit in its bounds, from `scroll`. The mouse wheel scrolls 3 rows, clamped so the last row stays at the bottom. Up/Down move the selection and scroll to keep it visible; past either end, the focus moves on to the next widget. A click selects a row; a click on the selected row, or Enter, activates it (`on_activate`). Used by the song picker and, later, the level list and editor file dialog.

Disabled widgets are skipped by Up/Down, ignore the mouse, and never take the focus.

### 3.6 Screen structure

```c
struct ui_screen {
    widget_t *widgets;
    int count;
    int focused;                   /* -1 when nothing can be focused */
    int hovered;
    int pressed;                   /* where the current left press started, -1 if none */
    int captured;                  /* the slider being dragged, -1 if none */
    int capturing;                 /* the keybind waiting for an input, -1 if none */
    /* the mouse, the held arrow and its next repeat */
    ui_screen_t *modal;            /* a dialog on top: it gets every event, it's drawn last */
    ui_rect_t panel;               /* a dialog's own background, behind its widgets and text */
    void *ctx;
    float label_x;                 /* rows' labels start here (5.1); 0: right-aligned against the widget */
    void (*on_back)(void *ctx);                             /* Escape */
    void (*on_key)(void *ctx, binding_t key, bool shift);   /* other keys: Tab for the sections (5.1) */
};

void ui_init(ui_screen_t *ui, widget_t *widgets, int count, void *ctx);   /* focus on the first enabled */
void ui_event(ui_screen_t *ui, const ui_event_t *ev, int64_t now_ms);
void ui_update(ui_screen_t *ui, int64_t now_ms);                          /* key repeat */
ui_look_t ui_look(const ui_screen_t *ui, int i);                          /* 3.2, for the renderer */
```

`ui_look` gives the state of 3.2 with what to draw: scale 1.05 hovered (under the mouse with no button down), 0.92 pressed (the left press started here and the mouse is still on it, or it's the slider being dragged), 50% alpha disabled, an outline when focused.

Widgets are laid out once when the screen is created; nothing is allocated per frame. No scene uses the toolkit yet: the options screen (5) is the first.

---

## 4. Music selection

### 4.1 Scope

1. Each level plays its **own song**, chosen by the level's author.
2. The player can **override** a level's song.
3. The **menu song** is chosen in options.
4. Songs stay **in sync** with the level on every attempt.

### 4.2 The library

Folder `music/`, scanned once at startup.

Scan algorithm:

**Moving today's files.** `res/back_mus.ogg` moves to `music/back_mus.ogg` and becomes the default level song. `res/menuLoop.mp3` is Geometry Dash's own menu track (PLAN 12.2), so it doesn't move: replace it with a track you're allowed to ship, saved as `music/menu_loop.ogg` (the `menu_song` default). Until that file exists, a missing `menu_song` falls back to the first song of the library with one warning, and to silence if the library is empty.

1. List `music/` (skip names starting with `.`).
2. Keep files ending in `.ogg`, `.wav`, `.flac`, and `.mp3` (see below).
3. For each, open with `sfMusic_createFromFile`. If it returns `NULL`, log "can't decode" and skip. Otherwise read `sfMusic_getDuration`, then destroy. `sfMusic` streams from disk, so opening only reads headers; this is fast.
4. Read `music/songs.txt` if present and merge its metadata (4.3).
5. Sort by title (case-insensitive).

MP3: SFML added MP3 decoding in 2.6. With CSFML 2.5, `.mp3` files fail at step 3 and are skipped with a log line, which is the correct behavior. Recommend `.ogg` in the README.

### 4.3 `songs.txt` grammar

```text
# file | title | artist | license | default_offset_seconds
stereo_sunrise.ogg | Stereo Sunrise | Some Artist | CC BY 4.0 | 0.00
base_after.ogg     | Base After     | Other Artist | CC0      | 1.25
```

- Fields separated by `|`, trimmed. Lines starting with `#` and blank lines ignored.
- Missing trailing fields get defaults: title = file name without extension, artist = "Unknown", license = "?", offset = 0.
- A line naming a file that wasn't found in step 2: warning, ignored.
- A file found in step 2 with no line: included with defaults.
- `default_offset` must be ≥ 0 and < duration; otherwise 0 with a warning.

```c
typedef struct song {
    char file[128];
    char title[64];
    char artist[64];
    char license[32];
    float default_offset;
    float duration;
} song_t;
```

The `license` column exists so you always know you're allowed to ship a song (the same issue as PLAN 12.2). Many game-music creators publish under Creative Commons or grant permission for games; keep the proof, and credit title and artist in-game, which CC BY requires.

### 4.4 Which song plays, and from where

For a level, in this order:

1. The player's **override** for this level, if set and the file still exists.
2. The level's `music` field, if the file exists.
3. The **default level song** (`back_mus.ogg` today, or a `default_level_song` setting).

Start offset, in this order:

1. The level's `music_offset` value, **only if** the song being played is the level's `music` file (an override uses its own default offset: the level's offset was chosen for the level's song).
2. The song's `default_offset` from `songs.txt`.
3. 0.

If the chosen file fails to open at level start, fall back to the next rule and show a small "song missing" notice.

### 4.5 Level header fields

```text
music stereo_sunrise.ogg
music_offset 1.25
```

Two header fields of PLAN 7.2, one value each. `music` names a file as `songs.txt` does, so it can't contain spaces or a `#`; a missing file falls back to 4.4's next rule. `music_offset` is in seconds. The parser stores both in `level_header_t`, which is level data like the name: the sim carries them and never reads them, and the game layer's music manager is what acts on them. That keeps PLAN Phase 2's rule that the sim knows nothing about audio.

### 4.6 Player override

UI: in the level list, each level shows `♪ Title — Artist`. A small button opens the **song picker**: a list widget with title, artist, duration and license; "Default (level's song)" at the top; the currently used one marked. Moving the selection plays a 10-second preview from the song's default offset, at music volume. Enter confirms; Escape cancels and restores the menu music.

Storage: the `song=` field of the progress file (PLAN 6.4, already `key=value`):

```text
level3 attempts=33 best=47.83 practice_best=81.20 hash=9f2c41d07ab35e11 song=base_after.ogg
```

Choosing "Default" removes the `song=` field.

### 4.7 The music manager

One module owns the playing `sfMusic`:

```c
typedef struct music_manager {
    sfMusic *current;
    char current_file[128];
    float volume;                 /* 0..100 */
    float level_offset;           /* start offset of the current level song */
    float audio_offset;           /* seconds, from settings (4.8) */
    sfClock *sync_clock;          /* throttles drift checks */
} music_manager_t;

int music_load(music_manager_t *m, const char *file);      /* no-op if already loaded */
void music_play_from(music_manager_t *m, float seconds);
void music_pause(music_manager_t *m);
void music_resume(music_manager_t *m);
void music_stop(music_manager_t *m);
void music_set_volume(music_manager_t *m, int volume_0_100);
void music_free(music_manager_t *m);
```

```c
int music_load(music_manager_t *m, const char *file)
{
    char path[256];
    sfMusic *next;

    if (m->current != NULL && strcmp(m->current_file, file) == 0)
        return 0;
    snprintf(path, sizeof(path), "music/%s", file);
    next = sfMusic_createFromFile(path);
    if (next == NULL)
        return -1;
    if (m->current != NULL)
        sfMusic_destroy(m->current);
    m->current = next;
    snprintf(m->current_file, sizeof(m->current_file), "%s", file);
    sfMusic_setVolume(m->current, m->volume);
    return 0;
}

void music_play_from(music_manager_t *m, float seconds)
{
    if (m->current == NULL)
        return;
    sfMusic_stop(m->current);
    sfMusic_setPlayingOffset(m->current, sfSeconds(seconds));
    sfMusic_play(m->current);
    sfClock_restart(m->sync_clock);
}
```

Only one `sfMusic` exists at a time. Switching songs destroys the old one after the new one opened, so a missing file never leaves you with silence *and* a dangling pointer.

Volume: `sfMusic_setVolume` is linear (0–100), but loudness perception is roughly logarithmic, so the bottom half of a linear slider sounds almost the same. Map the slider through a curve: `sfMusic_setVolume(m, 100.0f * powf(v / 100.0f, 2.0f))`. At slider 50 that gives 25, which *sounds* like half. Apply the same curve to sound effects.

### 4.8 Synchronization

**Rule:** while a level is playing, `song_position = level_offset + tick / TICK_RATE + audio_offset`.

`audio_offset` (setting `audio_offset_ms`, in seconds here) compensates output latency: sound leaves the speakers some milliseconds after `sfMusic` is told to play it (driver buffers, Bluetooth headphones: up to 200 ms). A positive offset starts the music that much further into the song, so what you hear lines up with what you see. It applies everywhere a song position is computed (`music_play_from` calls, the drift check, the editor's "play from here").

Every event that changes the sim's time must apply the rule:

| Event | Sim | Music |
|---|---|---|
| Level start | `sim_reset` (tick 0) | `music_play_from(offset)` |
| Death | stops ticking (DYING) | `music_stop` (GD stops the music on death) |
| Respawn | `sim_reset` | `music_play_from(offset)` |
| Checkpoint respawn (practice) | restore snapshot at tick `t` | `music_play_from(offset + t / TICK_RATE)` |
| Pause | stops ticking | `music_pause` |
| Resume | ticking again | `music_resume`, then a drift check |
| Window loses focus | pause (PLAN 10.2) | `music_pause` |
| Completion | stops | let the song play on under the end screen |

Start sequence, in this order (so the first tick and the first sound happen together):

1. Parse the level, `sim_init`.
2. `music_load` (can take a few ms on a slow disk).
3. `sim_reset`.
4. `music_play_from(offset)`.
5. Restart the level clock and zero the accumulator (PLAN 3.6).

Drift: the fixed-timestep loop clamps a frame to 0.25 s (PLAN 3.6). After a freeze longer than that, the sim falls behind real time; the music doesn't. Check once per second while playing:

```c
void music_check_sync(music_manager_t *m, long tick)
{
    float expected;
    float actual;

    if (m->current == NULL || sfTime_asSeconds(sfClock_getElapsedTime(m->sync_clock)) < 1.0f)
        return;
    sfClock_restart(m->sync_clock);
    expected = m->level_offset + (float)tick / TICK_RATE + m->audio_offset;
    actual = sfTime_asSeconds(sfMusic_getPlayingOffset(m->current));
    if (fabsf(actual - expected) > 0.05f)
        sfMusic_setPlayingOffset(m->current, sfSeconds(expected));
}
```

Why 50 ms: below that, a mismatch isn't noticeable; seeking a compressed stream can cause a small audio glitch, so you don't want to seek for nothing. Why once per second: `sfMusic_getPlayingOffset` is cheap, but checking every frame would react to normal scheduling jitter.

### 4.9 Level length vs song length

Level duration = `end_shift / SCROLL_SPEED` seconds (with speed portals, the sum of segments, 10.3). If `offset + level_duration > song duration`:

- `--check` (PLAN 7.4) and the editor warn: "level is 12.4 s longer than its song".
- At runtime, `sfMusic_setLoop(m, sfTrue)`: better a loop than silence.

### 4.10 Menu music

The menu song (setting `menu_song`) starts when the game starts, keeps playing across main menu, level list, options and editor, and is replaced by the level song on level start. When returning to menus, remember where the menu song was (`sfMusic_getPlayingOffset` before switching) and resume from there, so the menus don't restart the song every time you finish a level.

### 4.11 Credits

The end-of-level screen shows `♪ Title — Artist (License)`. A Credits screen (from the main menu) lists every song in the library with its metadata, plus the assets from `res/CREDITS.md` (PLAN 12.2).

### 4.12 Acceptance

- Each of the 7 levels can be given a different song with one header line.
- On 20 consecutive deaths and respawns, the song restarts from the same offset each time (listen for the same first note).
- After forcing a 1 s freeze (e.g. dragging the window on some systems, or a debug key that sleeps), the music is back in sync within 1 s.
- A song file deleted while the game is closed: the level plays the fallback song and shows the notice; no crash.
- Override set from the level list survives a restart; "Default" clears it.

### 4.13 As built

- **Where:** the pure part is `src/music/library.c` (`include/music/library.h`): the scan, `songs.txt`, the choice of song and offset, the sync rule, the volume curve, the level's duration. The scan takes the decoder as a function, so `tests/test_music.c` feeds it a fake one; the game passes `music_probe` (`sfMusic`). The one `sfMusic` lives in `src/music_manager.c`, the level list's song line and picker in `src/song_picker.c`.
- **File names:** a song's name can't have spaces or any of `#|=`, and needs one of the four extensions: the level header, `songs.txt` and the space-separated progress file all have to hold it. Others are skipped with a log line. At most 63 characters (the header's `music` field).
- **Offsets:** a `music_offset` of 0 counts as unset, so the song's `default_offset` applies.
- **What plays today:** `res/back_mus.ogg` moved to `music/back_mus.ogg` (`music/songs.txt` describes it, license still `?`). There's no `music/menu_loop.ogg`, so the menus play the library's first song with one warning; `res/menuLoop.mp3` isn't played any more (PLAN 12.2).
- **Sync (4.8):** level start, death, respawn and completion follow the table; the drift check runs once a second while playing. Pause, focus loss and checkpoint respawns wait for pause (PLAN 10.4) and practice mode.
- **Level vs song (4.9):** `--check` prints the song, its offset and both lengths, and warns when the level is longer; in game, the song loops then.
- **Menu vs level (4.10):** the manager tracks whether the menu owns the playback (`menu_playing`), not which file is loaded: a level that plays the menu's own song (`back_mus.ogg` today) still saves the menu's position on the way in and gives it back on the way out.
- **Picker (4.6):** the level list shows "Song: Title — Artist" under each level's Play button ("(yours)" for an override); clicking it opens the picker over the dimmed list. Moving the selection plays 10 s; Enter saves `song=` (the "Default" row removes it); Escape cancels; both resume the menu song where it was.
- **Credits (4.11):** the end screen shows "Song: Title — Artist (License)". The Credits screen isn't there yet: it needs a main menu entry and `res/CREDITS.md`.
- **Text:** SFML reads `char *` in the locale's encoding, so song titles and the em dash go through `utf8_to_utf32`. The game's font has no ♪, so the lines start with "Song:".

---

## 5. Options screen

### 5.1 Layout

UI view, 1920×1080. Left column: section tabs (Audio, Gameplay, Controls, Display, Data) at x = 120, one per 110 px. Right area: rows starting at x = 560, y = 200, 100 px apart; label left-aligned, widget right-aligned at x = 1400–1780. Back button bottom-left at (120, 940).

Keyboard: Up/Down move through rows, Tab / Shift+Tab switch sections, Left/Right change values, Enter activates, Escape = Back.

### 5.2 Every setting, precisely

| Section | Row | Widget | Setting key (2.2) | Applied |
|---|---|---|---|---|
| Audio | Music volume | slider 0–100, step 5 | `music_volume` | on change: `music_set_volume` |
| Audio | Effects volume | slider 0–100, step 5 | `sfx_volume` | on change: set every `sfSound` volume, play the click sound at the new volume |
| Audio | Menu music | cycler over library titles | `menu_song` | on change: switch the playing menu song |
| Audio | Audio offset | slider −300–300 ms, step 5, plus a "Calibrate" button | `audio_offset_ms` | on change; Calibrate plays a click track with a flashing square: adjust until the flash and the click coincide |
| Gameplay | Show percentage | toggle | `show_percent` | next frame |
| Gameplay | Show progress bar | toggle | `show_progress_bar` | next frame |
| Gameplay | Show attempts | toggle | `show_attempts` | next frame |
| Controls | Jump 1–6 | 6 capture rows ("—" when empty); keys, mouse buttons and gamepad buttons | `jump_bindings` | on capture |
| Controls | Restart | capture | `restart_key` | on capture |
| Controls | Place / remove checkpoint | 2 capture rows | `checkpoint_key`, `remove_checkpoint_key` | on capture |
| Display | Fullscreen | toggle | `fullscreen` | 5.4, with confirmation |
| Display | Window size | cycler: 1280×720, 1600×900, 1920×1080 (only sizes that fit the desktop) | `window_width/height` | 5.4, disabled while fullscreen |
| Display | VSync | toggle | `vsync` | on change |
| Display | FPS limit | cycler: Off, 60, 120, 144, 240 | `fps_limit` | on change; row disabled while VSync is on |
| Data | Reset progress | button | — | 5.6 |
| Data | Open save folder | button | — | opens it with `xdg-open`, and prints the path |

No physics settings, deliberately (2.5).

### 5.3 VSync and frame limit

- VSync on: `sfRenderWindow_setVerticalSyncEnabled(w, sfTrue)` and `sfRenderWindow_setFramerateLimit(w, 0)`.
- VSync off: `setVerticalSyncEnabled(w, sfFalse)` and `setFramerateLimit(w, fps_limit)`.
- Never both. SFML's frame limit sleeps, VSync waits for the monitor; combined they fight and cause uneven frames.

With PLAN 3.6's fixed timestep, these settings affect smoothness and power use only, never game speed. That's worth saying in the UI (a small hint line under the rows).

### 5.4 Display changes with automatic revert

Changing fullscreen or window size requires recreating the window:

```c
int window_apply(gd_t *gd)
{
    const settings_t *s = &gd->settings;
    sfVideoMode desktop = sfVideoMode_getDesktopMode();
    sfVideoMode mode = s->fullscreen ? desktop
        : (sfVideoMode){(unsigned)s->window_width, (unsigned)s->window_height, 32};
    sfUint32 style = s->fullscreen ? sfFullscreen : (sfTitlebar | sfClose | sfResize);

    sfRenderWindow *w = sfRenderWindow_create(mode, "my_gd", style, NULL);

    if (w == NULL)
        return -1;                     /* keep the old window; caller reverts the settings */
    if (gd->w != NULL)
        sfRenderWindow_destroy(gd->w);
    gd->w = w;
    sfRenderWindow_setKeyRepeatEnabled(gd->w, sfFalse);               /* 1.3 */
    sfRenderWindow_setMouseCursorVisible(gd->w, sfFalse);             /* custom cursor */
    sfRenderWindow_setVerticalSyncEnabled(gd->w, s->vsync);
    sfRenderWindow_setFramerateLimit(gd->w, s->vsync ? 0 : (unsigned)s->fps_limit);
    apply_letterbox(gd->ui_view, mode.width, mode.height);             /* PLAN 9.7 */
    apply_letterbox(gd->level_view, mode.width, mode.height);
    return 0;
}
```

The new window is created **before** the old one is destroyed: an unsupported mode makes `sfRenderWindow_create` return `NULL`, and then the old window is still there, the settings are restored, and a "Display mode not supported" notice is shown instead of the revert dialog.

Every window setting must be re-applied after recreation (key repeat, cursor, vsync, limit): they belong to the window, not to the program. Textures, fonts and sounds survive, because SFML keeps one shared OpenGL context for all windows.

Revert flow, a small state machine:

```text
[options] --change display--> save previous values, window_apply, open dialog
[dialog "Keep these display settings? Reverting in 10"]
    Keep (click/Enter)        -> close dialog, mark settings dirty
    Revert (click/Escape)     -> restore previous values, window_apply
    10 s elapse (sfClock)     -> same as Revert
```

Default focus on **Revert**, so a player who can't see the screen (unsupported mode) gets their old setup back by waiting or pressing Enter.

### 5.5 Key capture rules

1. Activating a row opens the modal "Press a key, mouse button or gamepad button for Jump 2 (Esc to cancel, Backspace to clear)".
2. A mouse button press or gamepad button press is captured like a key (the click that opened the modal doesn't count: capture starts on the next press).
3. `sfEvtKeyPressed`:
   - Escape: cancel, keep the old key. (Escape is reserved: it opens pause/back everywhere.)
   - Backspace: clear this jump slot (not allowed if it's the last jump binding left: at least one must exist).
   - Any other key already bound to **another action**: swap the two bindings, and show "R moved to Restart ↔ Jump 2" for 2 s. Swapping beats refusing: the player never gets stuck.
   - Otherwise: assign.
4. Bindings are stored by name. Keep a table `{BIND_KEY, sfKeySpace, "Space"}` for the inputs you allow (letters, digits, arrows, Space, Enter, Shift, Ctrl, Tab, `MouseLeft`/`MouseRight`/`MouseMiddle`, `Joy0`..`Joy15`); unknown names in the file fall back to the default bindings.

The left mouse button is a jump binding by default like Space and Up, and can be removed or moved like any other. Clicking the UI always uses the left button, independently of the bindings (1.5).

### 5.6 Reset progress

1. Button opens a modal: "Delete all attempts and best percentages? This cannot be undone." Buttons: **Cancel** (focused by default) and Delete.
2. Delete: copy `save/progress.txt` to `save/progress.bak` (overwrite), clear the store, save, show "Progress reset" for 2 s.
3. Song overrides live in the same file: say so in the dialog ("…and per-level song choices"), or keep them (only reset `attempts` and `best`). Pick one and write it in the dialog text.

### 5.7 Saving

Mark settings dirty on every change. On leaving the options screen: save if dirty. On quitting the game from anywhere: save if dirty.

### 5.8 Acceptance

- Every row works with keyboard only, and with mouse only.
- Settings survive a restart; deleting `settings.txt` gives defaults; writing garbage into it gives defaults plus warnings, no crash.
- Switching to an unsupported window size and waiting 10 s restores the previous size.
- After rebinding Jump to W and removing Space, Space no longer jumps and W does, in cube, ship and UFO.
- After removing `MouseLeft` from the jump bindings, clicking in a level doesn't jump, and menus still work with the mouse.

### 5.9 As built

- **Files:** the scene is `src/option_menu.c` (sections, routing, leaving), `src/options_rows.c` (each section's rows and what they apply) and `src/options_dialogs.c` (the revert and reset dialogs, the save folder), with `include/options.h`. The decisions that need no window are pure and tested (`src/ui/options_rules.c`, `tests/test_options.c`): rebinding (5.5) and the window sizes (5.2). `progress_reset` (5.6) is in `src/sim/progress.c`.
- **Routing:** the section tabs and Back are their own toolkit screen and only get the mouse; the rows get every event, so the keyboard never lands on a tab. Every callback only sets a `pending_*` field: switching sections, opening a dialog, recreating the window and leaving all happen after the frame's events (3.5).
- **Applied at once:** music volume, the menu song (it plays from its start), the audio offset, the three HUD toggles, VSync and the frame limit (never both, 5.3), the bindings. Effects volume is stored for the sounds to come.
- **Rebinding (5.5):** `rebind` assigns, clears on Backspace, swaps with the row that had the input (a notice "R moved: Jump 1 <-> Restart" for 2 s; the font has no ↔), and refuses any change that would leave no jump input, a swap included. The jump rows are packed back into `jump_bindings` in their order. A level started afterwards polls the new bindings (1.3).
- **Display (5.4):** `window_apply` makes the new window before destroying the old one; on failure the settings go back and "Display mode not supported" shows. Otherwise the dialog counts down from 10 with Revert focused; Escape and the timeout revert.
- **Reset (5.6):** the dialog says it deletes song choices too (they live in the same file), with Cancel focused. Nothing is cleared if the backup can't be written.
- **Saving (5.7):** every change marks the settings dirty; leaving the screen saves if dirty; quitting saves if dirty, or if `settings.txt` doesn't exist yet (so a first run still writes one to edit).
- **Not yet:** the audio offset's Calibrate button (it needs a click sound and a flashing square) and joystick polling for jumps (1.3).

---

## 6. Gamemode framework

### 6.1 The mode table

Adding a mode should touch one file and one table row, not every function with an `if (mode == ...)`. This extends PLAN 3.3's `mode_ops_t` (same fields, plus the ones below).

In the semi-physics engine (PLAN 3–4), a mode **never moves the player**: its functions only change the velocity (input impulses, forces). The engine then sweeps the player and handles every contact with the same rules for all modes, parameterized by the table. That's why a new mode is small.

```c
typedef enum gamemode {
    MODE_CUBE, MODE_SHIP, MODE_UFO, MODE_WAVE, MODE_BALL, MODE_COUNT
} gamemode_t;

typedef struct mode_ops {
    const char *name;           /* name in level files                                 */
    double half;                /* rigid square half size and circle radius (PLAN 4.3) */
    double inner_half;          /* inner box half size: a neutral touch kills          */
    double gravity;             /* px/tick^2, 0 for the wave                           */
    double max_fall;            /* px/tick, fall speed cap                             */
    double head_restitution;    /* ceiling hit: < 0 dies (never on the ground or a corridor
                                   boundary: it stops there), else share of vy bounced back */
    double corridor_height;     /* the corridor its portal opens, px; 0: none (6.9)    */
    bool neutral_kills;         /* any contact with a neutral object kills (wave);
                                   the ground and corridor surfaces don't            */
    bool keep_vy_on_surface;    /* sliding on a surface doesn't reset vy (wave)        */
    int bot_decision_ticks;     /* 0 = decide when grounded; N = every N ticks         */
    void (*apply_input)(player_t *p, input_t in);   /* impulses, input-driven speed   */
    void (*apply_forces)(player_t *p);              /* gravity and caps; never moves   */
    void (*update_rotation)(player_t *p);           /* the icon, cosmetic (PLAN 9.4)   */
} mode_ops_t;

extern const mode_ops_t MODES[MODE_COUNT];   /* include/sim/modes.h */
```

Defined once in `src/sim/modes.c`, with **designated initializers**: a dozen fields, several of them `bool`s, are too easy to shift by one without any compiler warning.

```c
const mode_ops_t MODES[MODE_COUNT] = {
    [MODE_CUBE] = {.name = "cube", .half = 50, .inner_half = 20,
        .gravity = PER_TICK2(CUBE_GRAVITY),
        .max_fall = PER_TICK(CUBE_MAX_FALL), .head_restitution = -1.0,
        .bot_decision_ticks = 0,
        .apply_input = cube_input, .apply_forces = default_forces, .update_rotation = cube_rotation},
    [MODE_SHIP] = {.name = "ship", .half = 50, .inner_half = 20,
        .gravity = PER_TICK2(SHIP_GRAVITY),
        .max_fall = PER_TICK(SHIP_MAX_VY), .head_restitution = 0.3,
        .corridor_height = 1000, .bot_decision_ticks = 12,
        .apply_input = ship_input, .apply_forces = default_forces, .update_rotation = ship_rotation},
    [MODE_UFO] = {.name = "ufo", .half = 50, .inner_half = 20,
        .gravity = PER_TICK2(UFO_GRAVITY),
        .max_fall = PER_TICK(UFO_MAX_FALL), .head_restitution = 0.3f,
        .corridor_height = 1000, .bot_decision_ticks = 12,
        .apply_input = ufo_input, .apply_forces = default_forces, .update_rotation = ufo_rotation},
    [MODE_WAVE] = {.name = "wave", .half = 15, .inner_half = 15,
        .head_restitution = -1.0,
        .corridor_height = 1000, .neutral_kills = true, .keep_vy_on_surface = true,
        .bot_decision_ticks = 12,
        .apply_input = wave_input, .apply_forces = no_forces, .update_rotation = wave_rotation},
    [MODE_BALL] = {.name = "ball", .half = 50, .inner_half = 20,
        .gravity = PER_TICK2(BALL_GRAVITY),
        .max_fall = PER_TICK(BALL_MAX_FALL), .head_restitution = 0.3f,
        .corridor_height = 800, .bot_decision_ticks = 0,
        .apply_input = ball_input, .apply_forces = default_forces, .update_rotation = ball_rotation},
};
```

`default_forces` is PLAN 3.4's `player_apply_gravity`: gravity accelerates up to the mode's `max_fall` and no further. The caps bind the player's own motion only (PLAN 3.4): gravity up to `max_fall`, and input up to the mode's limit (the ship's thrust up to `SHIP_MAX_VY`; the cube's jump and the UFO's hop set speeds below their limits). Speed from orbs, pads, slopes or a previous mode can exceed both caps and is never cut. Omitted fields are `false`/`0`.

Head hits, from the table: the cube and the wave die; the ship, UFO and ball bounce back with 30% of their rise speed (under `BOUNCE_MIN_SPEED` they just stop and slide). **The ground and corridor boundaries never kill anyone** (PLAN 4.3): they're surfaces the player can't cross, nothing more. A cube or wave that runs into one as a ceiling (the corridor ceiling, or the ground or corridor floor when flipped) stops and slides along it; the wave only dies on neutral objects, with its rigid square (8.4). When a portal removes or replaces a corridor, the old boundaries' collision disappears instantly (PLAN 5.2); only their drawing fades out.

`bot_decision_ticks = 12` means the bot may change its input every 12 ticks (50 ms). Smaller values find more solutions but search longer.

Everything that used to switch on the mode reads the table: the loader (portal mode names), the tick, the contact responses, the bot, the renderer (sprite array indexed by mode), the editor palette.

### 6.2 The player

PLAN 3.3's `player_t` already has everything the modes need, including the hold state (`hold`: none, fresh or used, PLAN 3.4). There is no press memory: an input counts only at the tick it's there.

`vy` is the rise speed (away from the floor in the player's own gravity) and `gravity_dir` is `+1` or `-1`; both are already in PLAN, and the engine's contact rules already use them (PLAN 4.4).

### 6.3 The tick with modes

PLAN 3.4's tick, with the mode-specific steps going through the table:

```c
void sim_tick(sim_t *s, input_t in)
{
    run_state_t *st = &s->st;
    player_t *p = &st->player;

    if (!p->alive || st->complete)
        return;
    p->prev_pos = p->pos;
    player_update_hold(p, in);                    /* 0: fresh / used / none (PLAN 3.4)  */
    in = activate_orbs(s, in);                    /*    a fresh hold on an orb (10.2)   */
    MODES[p->mode].apply_input(p, in);            /* 1: impulses                        */
    MODES[p->mode].apply_forces(p);               /* 2: gravity, caps                   */
    move_and_collide(s);                          /* 3: PLAN 4.4, mode-aware responses  */
    if (p->alive)
        collide_kill_ceiling(p, &s->lvl);         /* 4: PLAN 4.7                        */
    if (p->alive)
        apply_interactive(s);                     /* 5: portals, pads, orbs touched this
                                                        tick with a fresh hold, speed portals */
    if (p->alive)
        update_can_jump(s);                       /*    jump zone, last (PLAN 3.4)      */
    camera_follow(&st->cam, p, &st->bounds);      /* 6                                  */
    st->tick += 1;
    if (p->alive && st->distance >= s->lvl.end_shift)
        st->complete = true;                      /* 7                                  */
    if (p->alive)
        MODES[p->mode].update_rotation(p);        /* 8: the icon, cosmetic              */
}
```

`MODES[p->mode]` is read fresh at each step: step 5 can change the mode (a portal), and that must only affect the next tick's physics.

### 6.4 Surfaces, floors and ceilings with gravity

There's no separate surface code: the ground and the corridor's two surfaces are neutral half-planes in the sweep (PLAN 4.3), and PLAN 4.4 classifies **every** contact by how its normal faces the player's "up", `(0, -gravity_dir)`. So with flipped gravity, a block's underside is a floor (land and slide), its top is a ceiling (head hit), the corridor's ceiling is the floor, and the ground becomes a ceiling. Nothing mode-specific or gravity-specific has to be written for surfaces.

Mode-specific parts of the contact rules, all from the table:

- `head_restitution`: die, or bounce (6.1). Against a surface (the ground, a corridor boundary) nobody dies: a fatal head hit becomes a stop.
- `inner_half`: the size of the fatal inner box (PLAN 4.3), and with it the step-up height (`half - inner_half`, 30 px for the 100 px modes).
- `neutral_kills` (wave): any contact between the player's shapes and a neutral **object** kills, before the floor/ceiling/wall classification (for the wave, the rigid square and the inner box are the same 30 × 30 square, and the circle has radius 15). The ground and corridor surfaces (`SURF_*`) are exempt: the wave slides along them.
- `keep_vy_on_surface` (wave): landing on a surface stops the vertical motion for the rest of the tick (`vel.y = 0`) without resetting `p->vy`, so the tick after the input changes, the wave leaves the surface immediately (8.3).

**Outside a corridor with flipped gravity**, there is no floor surface at all: the player falls upward until it lands on a block or dies. Without a limit it would rise forever, and since the camera follows it upward, a limit measured from the camera would never trigger: the run would end by **completing the level**. The limit is PLAN 4.7's **kill ceiling**, fixed in the world at 600 px above the highest object's hitbox and computed at load. It exists **only while gravity is flipped**: a flipped cube that misses every block dies when its top crosses that line. With normal gravity there's no such line; whatever a pad or an orb sends up comes back down. The ground is always solid, so no lower limit is needed.

The margin must stay larger than the highest legitimate rise above the highest object (PLAN 4.7). When you add a stronger launcher, re-check it; the unit test in 13 pins it.

### 6.5 Why the engine already handles every direction

PLAN 4.4's `respond` computes `up_dot = -normal.y * gravity_dir`, `land` computes the slope-following rise speed with `gravity_dir` (the square or the circle standing on a block's underside when flipped), the step-up's lift moves along `(0, ∓gravity_dir)`, and the jump zone is mirrored to the top strip of the rigid square (PLAN 4.3). The square, the circle and the inner box don't rotate, so nothing else depends on gravity. Written once, it all works for both gravities.

Verified in the lab's prototype (older overlap rules, same geometry): a flipped ball starting at y = 500 under a row of blocks whose bottom is at y = 400 lands grounded at exactly **y = 450** (block bottom + 50), alive. In the swept engine the same case is a flat contact, so `settle` puts it at exactly 450 too; it's a unit test (13).

### 6.6 Portals and mode transitions

On the first touch of a mode portal (an interactive object, PLAN 5.1: it acts once, then is `spent` and has no hitbox until the next attempt; it's a ghost). **A mode portal never teleports the player**: it only changes the gamemode. Position, `vx`, `vy`, gravity and hold are all kept; a player entering near the portal's top edge continues from exactly there. The only changes in motion come afterwards from the new mode's own logic (a wave sets its sharp 45° motion from the next tick, 8.2). Step by step:

1. `p->mode = portal mode`.
2. Boundaries: **decided by the gamemode alone, never by gravity.** If the new mode has a `corridor_height` (ship, UFO and wave 1000 px, ball 800 px, 6.9), compute the corridor from the portal (PLAN 5.2: that height, centered on the portal's rect, snapped to the nearest grid line, pushed up so its bottom isn't below the ground; a rotated portal still opens a horizontal corridor), and **lock the camera** on it until the player leaves it; if it doesn't (cube, and later robot and spider), remove them. **Same-mode portals** follow the same rule: a ship portal reached in ship mode (or a UFO portal in UFO mode, and so on) switches to the new corridor, centered on the second portal. Gravity only decides which of the two boundaries is the one the player stands on (6.4): a flipped ship has the same floor and ceiling as a normal ship, it just rests against the ceiling. Nothing else creates or removes boundaries: not gravity portals, not pads, not orbs.
3. `vy`: **kept as is**, never clamped (PLAN 3.4: limits only apply to what gravity and input add). A cube falling at 2700 px/s entering a UFO portal keeps falling at 2700 px/s; the UFO's gravity just won't accelerate it any further than its own 1797 px/s fall limit, which it's already past. Momentum carries across modes: a wave going up at 45° (`vy = +750` px/s) that enters a cube portal becomes a cube rising at 750 px/s, a small hop of about 58 px (`v² / 2g` with the cube's gravity); entering a ship portal, it keeps climbing at 750 px/s and the ship's own gravity and thrust take over from there.
4. The hold state is unchanged: a portal doesn't use or refresh a hold. (A portal has nothing to do with the ground: `grounded` is the engine's business.)
5. `gravity_dir`: **never touched by a mode portal**, including cube portals. A mode portal only changes the gamemode (and therefore the icon and physics). A flipped ball entering a ship portal is a flipped ship; a flipped ball entering a cube portal is a flipped cube. Only gravity portals, ball clicks, blue pads and blue orbs change `gravity_dir`, and they all do it through PLAN 3.4's `player_flip_gravity` (flip the constant gravity, keep the motion).

   Consequence: a flipped cube can leave a corridor with nothing above it. That's the level designer's job to handle (put blocks to land on, or a gravity portal back), exactly as in GD. The simulation only needs a rule for when it isn't handled: the kill ceiling (6.4).
6. Icon rotation: unchanged at the portal; the new mode's `update_rotation` takes over at the end of the tick (cosmetic).

### 6.7 Adding a mode: checklist

1. `src/sim/mode_<name>.c`: `apply_input`, `apply_forces`, `update_rotation` (cosmetic, PLAN 9.4).
2. One row in `MODES`.
3. A sprite (the game layer's `mode_sprites[MODE_COUNT]`).
4. `tests/levels/<name>.lvl`, completable by the bot.
5. A motion unit test with the measured numbers (sections 7–9 give them).
6. Nothing in the editor: its palette reads `MODES`.

### 6.8 Tuning a "hop" mode: the two formulas

For any mode that jumps with an initial speed `v` against gravity `g` (cube, UFO, ball flips, pads, orbs), in continuous terms:

- Rise height: `h = v² / (2g)`
- Time to come back to the same height: `T = 2v / g`

Solve for the constants from the feel you want (a height and a duration):

- `v = 4h / T`
- `g = 8h / T²`

Example: GD's cube rises 2.1333 blocks and lands after about 0.43 s → `v ≈ 1.98 units`, `g ≈ 0.88 units`, close to GD's published 1.94 and 0.876. At 240 Hz the discrete integration lands about 2% lower than the formula, which is why `CUBE_JUMP_V` is 1.9522 units (PLAN 11.1): get close with the formulas, then solve the last percent against a unit test.

### 6.9 Reference: GD's own numbers, in our units

Sources: the wiki's [Portals](https://geometrydash.wiki.gg/wiki/Portals) page (corridors, jump heights, speeds) and the forum post [The Physics of Geometry Dash, Part 1: Cube](https://gdforum.freeforums.net/thread/48749/p1kachu-presents-physics-geometry-dash) (velocities, gravity, pads, orbs).

Both count in **GD blocks**: a block is the player's size, so **1 block = our 100 px** (GD splits a block into 30 of its own internal units; nothing here needs them). Speeds are written relative to the horizontal one: **1 velocity unit = the normal scroll speed** (1038.6 px/s), and **1 acceleration unit = that speed squared per block** (10786.9 px/s²), which is how `constants.h` writes them (PLAN 3.2). Vertical constants stay tied to the *normal* speed, so a speed portal never changes how high the player jumps.

**Corridors** (the "vertical grid" column). "No ceiling" means the mode has no corridor at all:

| Mode | GD | Ours (`corridor_height`) |
|---|---|---|
| Cube, robot | no ceiling | 0: no corridor |
| Ship | 10 blocks | 1000 px |
| UFO | 10 | 1000 px |
| Wave | 10 | 1000 px |
| Swing | 10 | 1000 px (later, 10.5) |
| Spider | 9 | 900 px (later, 10.5) |
| Ball | 8 | 800 px |

**Velocities and gravity** (all in velocity/acceleration units):

| | GD | Ours |
|---|---|---|
| Cube gravity | 0.876 | `CUBE_GRAVITY` |
| Cube jump | 1.94 | `CUBE_JUMP_V` = 1.9522, so the **measured** height is GD's (PLAN 11.1) |
| Fall cap | 2.6 | `CUBE_MAX_FALL` |
| Mini cube jump | 1.41 | 10.4 |
| Yellow / pink / red pad | 2.77 / 1.79 / 3.65 | 10.1 |
| Mini pads | 2.13 / 1.32 / 2.71 | 10.4 |
| Yellow / pink / red orb | 1.91 / 1.37 / 2.68 | 10.2 |
| Mini orbs | 1.43 / 0.94 / 2.05 | 10.4 |
| Blue pad and blue orb | −1.37 | flip, then 1.37 toward the new floor (10.1) |
| Green orb | −1.91 in GD's tables | flip, then a jump of 1.91 away from the new floor (10.2) |
| Black orb | −2.6 | straight toward the floor, at the fall cap |
| Ship, UFO, ball, wave | not published | ours (7–9), except the UFO's hop height |

**The ship, and how its numbers were chosen.** GD publishes nothing here, so guessing a number is guessing twice: once at the value and once at the units. Instead the ship is defined by the **arc it draws**, which is what a player actually feels, and the constants fall out of it:

- The vertical cap is exactly the scroll speed (`SHIP_MAX_VY = 1.0 × V_UNIT`), so the ship never travels steeper than **45 degrees**. That single rule is most of the feel: a ship that can move vertically faster than the level scrolls reads as violent whatever the other numbers are.
- Gravity (`0.36 × A_UNIT`) drops the ship across the full 10-block corridor in **1.00 s**, about 10 blocks of level.
- Thrust (`1.20 × A_UNIT`) lifts it back in **1.02 s**, so climbing and falling are symmetric and the ship answers a press immediately.

The first version of these (0.99, 2.24, 2.24) crossed the corridor in 0.49 s falling and 0.63 s climbing, with a vertical cap of 23.3 blocks/s — more than twice the horizontal speed. Gravity was also **stronger than the cube's** (0.99 against 0.876), which is backwards: GD's ship is the floatier of the two. Measure a change here with the arc, not with the constants.

A negative value is a speed **toward the floor the player is falling to**: for the blue pad and the blue orb it's applied after the flip, so it throws the player at its new floor; the black orb doesn't flip, so it slams the player at the floor it already had. The green orb flips and then sets a **positive** 1.91, a jump in the new gravity (10.2). Blue keeps the same 1.37 for mini, and green the same 1.91.

**Jump and hop heights:**

| Move | GD | Ours |
|---|---|---|
| Cube jump | 2.1333 blocks (2.233 chained) | 213.3 px (223.3), matched exactly: measured apex 213.32 px, airtime 0.425 s, length 441 px (PLAN 11.1) |
| Mini cube jump | 1.3583 (wiki), 1.633 (P1kachu) | jump 1.41 units; with the wiki's height the mini gravity is 0.7318 units, with P1kachu's 0.6087. Re-measure when mini lands (10.4) |
| UFO hop | 1.5666 | 156.7 px: our `UFO_JUMP_V` is set from it (1.416 units, 7.2) |
| Mini UFO hop | 1.2 | 120 px, 0.77× |
| Robot jump (held) | up to 3.5111 | up to 351 px (later, 10.5) |
| Mini robot jump | up to 2.7666 | up to 276.7 px |

**Horizontal speeds** (GD's five speeds, which are now ours, 10.3):

| GD speed | Ratio | GD | Ours |
|---|---|---|---|
| Slow | 0.807× | 8.372 blocks/s | 837.2 px/s |
| Normal | 1× | 10.386 | 1038.6 px/s, our scroll speed |
| Fast | 1.243× | 12.914 | 1291.4 px/s |
| Very fast | 1.502× | 15.6 | 1560 px/s |
| Extremely fast | 1.849× | 19.2 | 1920 px/s |

**Other rules from the same page**, already in this plan: the wave moves at a 45° diagonal up while held and down while released (8), and a **mini wave's vertical speed is double its horizontal speed** (a 2:1 diagonal, not 45°: add it with the mini portal, 10.4); the ball inverts gravity on contact with a surface (9); the spider teleports to the nearest overhead surface and inverts gravity (10.5); the swing inverts gravity gradually (10.5).

### 6.10 As built

- **The table:** `MODES` (`src/sim/modes.c`, `include/sim/modes.h`) has every field of 6.1, with a row for the cube, the ship, the UFO (7), the wave (8) and the ball (9). A mode's own code is one file, `src/sim/mode_<name>.c`: its input and its icon's rotation. `src/sim/player.c` keeps what every mode shares: the hold rule, gravity up to the mode's fall cap (`player_apply_gravity`, the `apply_forces` of every row but the wave's) and `player_flip_gravity`.
- **The tick (6.3)** calls `MODES[p->mode]` at each step and no longer names a mode. Moving the code was checked to change nothing: every tick's state hash over 20000 ticks, on every level and three input patterns, is the same before and after.
- **`neutral_kills` (6.4):** the square that dies on a neutral object is `mode_neutral_kill_half`: the inner box, or the whole rigid square for a mode nothing may touch. Only the wave's row sets it (8).
- **Gravity portals (9.4)** are in, ahead of the ball, because they are what flips gravity in a level: `gravity x y size up|down`, an interactive object with a mode portal's shape. Yellow sends the player up, blue brings it back down.
- **The game layer** reads per-mode arrays for the icon and the portal colour, and draws the icon upside down when gravity is flipped.
- **Tests:** `tests/test_gravity.c` (the table's rows, the parser, a flip keeps position and on-screen speed, a portal acts once, landing exactly 50 px under a block, there and back, the kill ceiling, gravity and gamemode independent) and a gravity level in `tests/test_bot.c`.

---

## 7. UFO

### 7.1 Behavior

- Each **press** gives one hop, on the ground or in the air.
- Holding does nothing.
- Falls with its own (lighter) gravity.
- Lands on block tops, slopes and the corridor floor; bounces off ceilings (30% of its rise speed, PLAN 4.4); dies on spikes and when a block reaches its inner box (running into a wall).
- Enters with a corridor.

### 7.2 Physics

```c
#define UFO_GRAVITY   (0.64 * A_UNIT)    /* ours: 6903.6 px/s^2                        */
#define UFO_JUMP_V    (1.416 * V_UNIT)   /* 1470.7 px/s: GD's 1.5666-block hop (6.9)   */
#define UFO_MAX_FALL  (1.73 * V_UNIT)    /* ours: 1796.8 px/s                          */

void ufo_input(player_t *p, input_t in)
{
    if (in.pressed) {
        p->vy = PER_TICK(UFO_JUMP_V);
        p->hold = HOLD_USED;                 /* holding into an orb afterwards does nothing */
    }
}

void ufo_rotation(player_t *p)
{
    p->rotation = -atan2f(p->vy, p->vx) * 180.0f / (float)M_PI / 3.0f * (float)p->gravity_dir;
}
```

Forces are `default_forces` with the UFO's row of the table (gravity 6903.6 px/s², fall cap 1796.8 px/s): nothing UFO-specific. The engine moves it.

The icon tilts with a third of the ship's angle: enough to show direction, not enough to look like a ship. Cosmetic only (PLAN 9.4).

**Assign, don't add.** A hop works exactly like a cube using an orb: it **sets** the rise speed and discards the current momentum, whatever it was. Like a pink orb clicked right after a yellow pad, which resets the pad's launch to the orb's speed, a hop during a yellow pad's 2876.9 px/s launch brings the UFO to 1470.7 px/s. `vy = UFO_JUMP_V` makes every hop identical whatever the current speed. Adding would let fast clicking accumulate speed without limit, and a hop during a fast fall would barely slow it, which feels unresponsive.

### 7.3 Expected behavior (GD constants, 240 Hz)

| Measurement | Value |
|---|---|
| One press from the ground: rise | **153.6 px** measured, 156.7 continuous: GD's 1.5666 blocks (6.9); a cube jump is 213.3 |
| Time to apex | **0.213 s** |
| Back on the ground | **0.425 s** (102 ticks), the same as a cube jump |
| Holding after the press | no effect (same numbers) |
| Tap rhythm that holds a constant altitude | one press every **0.425 s** (102 ticks) |
| Tapping faster | climbs; slower: sinks |

The hover rhythm equals the hop duration `T = 2v/g`: to hold altitude, press exactly when the previous hop returns to its starting height. Faster tapping climbs, slower sinks. That's the UFO's whole skill, and it's why `T` is the number to tune first. With these values, a comfortable hover is a bit under 2.4 taps per second. (The earlier prototype's numbers, 150.9 px and 0.583 s, were measured with our old guessed constants at 750 px/s.)

### 7.4 Design consequences

- A wall of height `H` needs about `ceil(H / 155)` hops if they're chained near each apex. The test level's 300 px wall takes 2.
- Ceilings: a hop that hits a ceiling bounces back down with 30% of its speed; players feel this as "bonking". Keep gaps at least 250 px tall in early UFO sections.
- The bot solves the test level in 40 attempts (73 in the prototype).

### 7.5 Test level

```text
name TEST ufo
portal 1500 650 2 ufo
block 2300 550 2
block 2300 650 2
block 2300 750 2
spike 2900 750 2
portal 3600 650 2 cube
```

Expected: completable by the bot; impossible for a cube (the wall is 300 px, above the cube's 213.3 px jump).

### 7.6 As built

- **The mode:** `MODE_UFO`, its row in `MODES` and `src/sim/mode_ufo.c` (`ufo_input`, `ufo_rotation`), with the constants of 7.2 in `constants.h`. A press sets the rise speed, on the ground or in the air; a hold alone does nothing, including one carried into the attempt or kept through a landing. The press marks the hold used.
- **Measured in the engine:** 153.6 px, apex on tick 51 (0.2125 s), grounded again after 102 ticks. In the air the rhythm that holds an altitude sits between 101 ticks (climbs 1.6 px a hop) and 102 (sinks 4.5 px): on the ground the floor hides that half tick.
- **The game layer:** an orange portal, and the ship's icon standing in until the UFO has its own.
- **Tests:** `tests/test_ufo.c` (the numbers of 7.3, holding, assign and not add from five speeds, the tap rhythm, the fall cap, the ceiling bounce on a block and on the corridor, landing, walls and spikes, flipped gravity, portals in and out, the icon's lean) and the level of 7.5 in `tests/test_bot.c`, with and without its UFO portal.

---

## 8. Wave

### 8.1 Behavior

- Moves at exactly 45°: toward the ceiling while held, toward the floor while released. A tap inside one tick gives one tick up (`held || pressed`, PLAN 3.4).
- That 45° motion is a **real velocity** in the engine (`vy = ±vx`), not a position rule: while in wave mode it's overwritten every tick by the input (the wave goes sharply, directly up or down, so it never feels inertia), but when the player leaves wave mode through a portal, the new mode inherits it (6.6 step 3): leaving a wave on the way up gives the next mode a small upward push, on the way down a downward one.
- No gravity, no inertia: direction changes on the tick the input changes.
- Hitbox 30×30.
- **Any block or spike contact kills.** The corridor floor, ceiling and the ground are safe: it slides along them, including when it runs into the ceiling (surfaces never kill, PLAN 4.3).
- Leaves a trail.

### 8.2 Physics

```c
void wave_input(player_t *p, input_t in)
{
    p->vy = (in.held || in.pressed) ? p->vx : -p->vx;   /* 45 deg at any horizontal speed:
                                               a real velocity, inherited by the next mode (6.6) */
}

void no_forces(player_t *p)
{
    (void)p;                                /* no gravity, no inertia */
}

void wave_rotation(player_t *p)
{
    p->rotation = (p->vy > 0.0f ? -45.0f : 45.0f) * (float)p->gravity_dir;   /* cosmetic */
}
```

Holding for 40 ticks moves **173.1 px right and 173.1 px up** (slope 1.0000, at the normal speed). Crossing a full 1000 px corridor (6.9) means 970 px of travel for the 30 px hitbox, which takes 970 / 1038.6 = **0.93 s**.

With flipped gravity, "held" moves toward the flipped ceiling, i.e. down the screen, like GD's reverse-gravity wave.

### 8.3 Why `keep_vy_on_surface`

When the wave slides on the floor, the engine's landing response stops its vertical motion for the rest of the tick. If it also zeroed `vy` (as it does for other modes), the next tick would start from 0 and the wave would stay stuck for a tick after the player presses. Keeping `vy` means that on the tick the input changes, the wave leaves the surface immediately. Precise and responsive is the whole point of the wave.

### 8.4 Hitbox choice

`half = inner_half = 15`: the wave's rigid square and inner box are one 30 × 30 square, its circle has radius 15, and its jump zone is empty (it never jumps). The sprite can be 60 px; the small box is what makes tight wave corridors fair. A 100 px gap leaves 70 px of freedom for the wave's center. Show the box in the debug overlay (PLAN 9.6) while designing.

Design decision recorded here: neutral objects always kill the wave (it can't slide on block or slope surfaces), while the ground and corridor surfaces are safe. It's simple and easy to read. If you want sliding on blocks later, set `neutral_kills = false`: the engine's normal floor and ceiling responses then apply, with `keep_vy_on_surface`.

### 8.5 Trail (game layer)

Algorithm:

1. After each tick, if the player is a wave, compare the sign of `vy` (and whether it's touching a surface) with the previous tick's.
2. On any change, append the current position to `trail[]` (world coordinates). Capacity 256; when full, drop the oldest.
3. Each frame, drop old points left of the view, but **keep the last one that's left of the view's left edge**: it's the start of the segment that enters the screen. Dropping it too would make the visible trail start at the first on-screen point instead of at the screen's edge. In code: drop `trail[0]` only while `trail[1].x` is also left of the edge.
4. Draw a polyline through `trail[0..n-1]` plus the current position.
5. Clear the trail on respawn and on leaving wave mode.

Only direction changes are stored, because between two changes the path is a straight line: 256 points cover minutes of play.

Drawing with thickness, as a triangle strip:

```c
void draw_trail(gd_t *gd, const vec2_t *pts, size_t n, vec2_t head, float width)
{
    sfVertexArray *va = gd->trail_va;       /* created once, primitive type sfTriangleStrip */

    sfVertexArray_clear(va);
    for (size_t i = 0; i <= n; i++) {
        vec2_t p = i < n ? pts[i] : head;
        sfUint8 alpha = (sfUint8)(60 + 195 * i / (n + 1));   /* older = more transparent */
        sfColor c = sfColor_fromRGBA(255, 255, 255, alpha);

        sfVertexArray_append(va, (sfVertex){{p.x, p.y - width / 2}, c, {0, 0}});
        sfVertexArray_append(va, (sfVertex){{p.x, p.y + width / 2}, c, {0, 0}});
    }
    sfRenderWindow_drawVertexArray(gd->w, va, NULL);
}
```

Offsetting vertically gives constant *vertical* thickness, so 45° segments look `1/√2` ≈ 71% as thick as flat ones. For uniform thickness, offset each point along the average normal of its two segments (the normalized sum of the two segments' perpendiculars, scaled by `width / 2` divided by the cosine of half the angle between them). It's one draw call either way.

### 8.6 Test level

```text
name TEST wave
portal 1500 350 2 wave
spike 2200 750 2
block 2600 250 2
block 3000 550 2
portal 4000 350 2 cube
```

Expected: completable by the bot (16 attempts in the prototype).

### 8.7 As built

- **The mode:** `MODE_WAVE`, its row in `MODES` and `src/sim/mode_wave.c` (`wave_input`, `wave_forces`, `wave_rotation`), `WAVE_HALF` in `constants.h`. The hold is never used: flying uses nothing (PLAN 3.4).
- **The icon's angle** is read from what the tick really did (`prev_pos` to `pos`) and not from `vy` as in 8.2: `vy` is kept while sliding (8.3), so it would show a wave pointing 45° into the floor it slides on. It is 45° in the air and flat along a surface. The icon is drawn 60 px wide (8.4), so it overlaps a surface it slides on by 15 px.
- **Leaving wave mode on the ground:** a wave sliding there is 35 px lower than a cube standing there. The portal puts the bigger square back on the ground, by exactly that much, on the tick it acts (`stay_above_the_ground`, `src/sim/interact.c`); objects are left to the contact rules. It is the one case where a mode portal moves the player (PLAN 5.2), and only the ground can cause it: a new corridor is placed around the player.
- **The floor spike:** a spike's box ends 32 px above the ground (PLAN 4.2), and the wave is 30 px tall, so a wave sliding on the ground passes under a spike standing on it. Raise the spike or put a block there when a wave must not pass.
- **The trail (8.5)** is two parts. `src/fx/trail.c` (pure, tested) keeps the corners: one where each tick that turned started, which is exact, where "the current position" of 8.5 step 2 would be a tick late; reaching and leaving a surface are corners, sliding is not. It begins where the portal acted (or at the start, for an attempt that begins as a wave) and ends with the mode; a death leaves it on screen until the respawn. `src/trail_draw.c` builds the strip of 8.5 each frame, with constant vertical thickness (14 px). It fades with the distance behind the player down to the screen's left edge, and not with the corner's index: an index fade made the whole trail change brightness each time a corner was added.
- **Measured in the engine:** 40 held ticks are 173.1 px each way; floor to ceiling of the corridor takes 225 ticks (0.94 s).
- **Tests:** `tests/test_wave.c` (the row, 45° at 1× and 2×, a direction change on the tick of the input, a tap, death on a block's top, underside and side, on a slope and a spike, the 30 px square under a block 31 and 29 px above the ground, sliding on the ground and on the corridor's ceiling and leaving each at once, flipped gravity, what the next mode inherits, growing on the ground, the icon's angle), `tests/test_trail.c` (corners, surfaces, portals, attempts, capacity, rounding, dropping what is left of the view) and a wave level in `tests/test_bot.c`.
- **The level of 8.6 is no test of the wave.** Its portal's hitbox ends at y = 540 and a cube on the ground reaches 750, so a cube passes under it, jumps the spike and walks under both blocks: the bot's 12 attempts were a cube's, with no tick spent as a wave. The level in `tests/test_bot.c` has its portal on the ground (`portal 1500 650 2 wave`), a floor block, a ceiling block and a taller floor block, and the test replays the found path and counts the ticks spent as a wave (89 attempts).

---

## 9. Ball and gravity direction

### 9.1 Behavior

- Rolls along the floor surface.
- A click flips its gravity and pushes it gently off the surface it leaves: when the hold is **fresh** (PLAN 3.4) and the ball can jump (`can_jump`, PLAN 4.3), gravity flips and the ball falls to the other surface. The flip uses the hold up, like a UFO's hop: kept down, the ball does **not** flip again on landing. A hold still fresh when the ball lands (pressed in the air and kept down) flips there, once.
- No press memory: a click released before the ball can flip does nothing (PLAN 3.4).
- Lands on block tops (or undersides when flipped) and slopes, bounces off ceilings (30%), dies on spikes and when a block reaches its inner box.
- Enters with a corridor.

### 9.2 Physics

```c
#define BALL_GRAVITY    (0.5 * A_UNIT)   /* ours: 5393.4 px/s^2 */
#define BALL_MAX_FALL   (1.6 * V_UNIT)   /* ours: 1661.8 px/s   */
#define BALL_FLIP_V     (0.4 * V_UNIT)   /* ours: 415.4 px/s, the push off the surface it leaves */
#define BALL_SPIN       360              /* deg/s at 1x, the icon's roll (cosmetic) */

void ball_input(player_t *p, input_t in)
{
    if (p->hold == HOLD_FRESH && p->can_jump) {    /* a used hold flips nothing, PLAN 3.4 */
        player_flip_gravity(p);             /* PLAN 3.4: flip the constant gravity */
        p->vy = fmin(p->vy, -PER_TICK(BALL_FLIP_V));   /* and leave this side: toward the new floor */
        p->hold = HOLD_USED;
    }
}

void ball_rotation(player_t *p)
{
    (void)p;                                 /* the rolling is drawn by the game layer */
}
```

Forces are `default_forces` with the ball's row (gravity 5393.4 px/s², fall cap 1661.8 px/s). The flip only changes `gravity_dir` and `vy`; the engine does the rest, including landing on block undersides once gravity is flipped (6.4).

A click flips the gravity and gives the ball a small speed toward its new floor (`BALL_FLIP_V`, a quarter of the fall cap). A pure flip, starting from rest, was tried first and felt weak: 0.09 px on the first tick, 4 px after 9. With the push the ball leaves its side at once, 1.8 px on the first tick and 10 px after 5, and the flipped gravity does the rest. The push sets a speed and never slows a ball already going faster that way.

Rolling is drawn by the game layer (PLAN 9.4): the icon turns by `distance / 50` radians, a circle of radius 50 rolling without sliding.

### 9.3 Measured behavior (prototype, 240 Hz)

| Measurement | Value |
|---|---|
| Flip across the ball's 800 px corridor (700 px of travel for the 100 px ball), from rest | **0.51 s** (122 ticks, measured in the engine) |
| Flipped ball landing under a block (block bottom y = 400) | grounded at **y = 450**, alive |

At the normal speed (1038.6 px/s), a full flip covers about 528 px horizontally (5.3 blocks). That's the minimum spacing between floor and ceiling hazards the player must dodge with consecutive flips; use it when designing.

### 9.4 Gravity portals

Once `gravity_dir` is honored everywhere, a gravity portal is small:

```text
gravity x y size up|down
```

First touch (interactive rule, PLAN 5.1: acts once, then `spent`): if the player's gravity isn't already the portal's direction (`up` = −1, `down` = +1), `player_flip_gravity` (PLAN 3.4); otherwise nothing. It only flips the gravity: the player keeps its motion, and the flipped gravity takes it from there. The mode doesn't change, and no corridor is created: a gravity portal outside a corridor gives a flipped player nothing above it, so the level must provide something to land on, or the kill ceiling (6.4) kills it. Gravity portals and mode portals are fully independent: one changes gravity, the other changes the gamemode.

Built with the framework (6.10): a portal the player already agrees with does nothing and is used up all the same.

### 9.5 Test level

```text
name TEST ball
portal 1500 350 2 ball
spike 2200 750 2
spike 2800 0 2 rot=180
spike 3400 750 2
portal 4200 350 2 cube
```

The ball's corridor for a portal at y = 350 is 800 px tall: centered at 450, so 50..850, its floor on the ground. Spikes alternate floor, ceiling (rotated 180°, pointing down, PLAN 4.2), floor, 600 px apart: more than the 528 px a flip needs. Expected: completable. (The prototype found it in 31 bot attempts, but it had no rotation, so its ceiling spike used an upward-pointing hitbox; re-measure once PLAN 4.2 exists.)

### 9.6 Mirrored tests

Because every rule is written with `gravity_dir`, mirroring a corridor section vertically around the corridor's center, and starting it with `gravity_dir = -1`, must give exactly the same result as the original. The test harness can generate the mirror automatically and run both through the bot. It mirrors at the **hitbox** level, after `sim_init`: every vertex `y' = 2 × center − y`, vertex order reversed to stay clockwise, axes and bounds recomputed. That works for every shape, including a non-square slope, which no `rot=` value can mirror (a reflection isn't a rotation). Any difference in the bot's result is a direction bug in the contact rules. It doubles collision coverage for one loop in the test runner. It needs a start position inside the section (11.8), since the cube start outside a corridor isn't mirrorable.

### 9.7 As built

- **The mode:** `MODE_BALL`, its row in `MODES` and `src/sim/mode_ball.c` (`ball_input`, `ball_rotation`), the constants of 9.2 in `constants.h`. The flip needs a fresh hold and `can_jump`; a tap inside one tick counts, a click released in the air is lost, and a hold kept down flips once and no more.
- **Tuned by play-testing:** the first version (gravity 0.75, cap 2.67, a flip in 0.425 s) went up and down too hard, flipped again on every landing while held, and rolled 3.3 turns a second. Gravity 0.5 and cap 1.6 alone (0.575 s) were then too soft, so the click also pushes the ball off its surface at 0.4: a flip across the 800 px corridor takes **122 ticks (0.51 s)**, 528 px of level. Flipped, it lands under a block whose underside is at 400 at y = 450.
- **The rolling** is computed in the tick (`ball_rotation`), not in the game layer as 9.2 says, so it never jumps at a flip. It is `BALL_SPIN`, one turn a second at 1×, scaled with the speed and reversed on a ceiling: a circle of this size that never slid would turn 3.3 times a second, too fast to read.
- **The 800 px corridor** needed nothing new. A portal is 280 px tall, so whatever touches it is well inside the corridor it opens, and the ground is the one thing that can cut that corridor short (8.7).
- **The level of 9.5** has the fault of 8.6: its portal is too high for a cube on the ground, which then jumps the two floor spikes and never becomes a ball (23 attempts, no flip). With the portal on the ground (`portal 1500 650 2 ball`) the corridor is the 50..850 that 9.5 describes, and the bot needs the three flips: 65 attempts. `tests/test_bot.c` uses that one and counts the flips of the found path; the UFO's level counts its ticks as a UFO the same way.
- **Mirrored tests (9.6):** `tests/test_mirror.c`. One section (floor and ceiling blocks, both kinds of slope, spikes both ways, a tilted block, gravity portals, a mode portal) is turned upside down at the hitbox level and played with flipped gravity. For the ship, the UFO, the ball and the wave, 200 seeded input patterns each, the two runs are compared **on every tick, bit for bit**, deaths included, and the bot must reach the same verdict in the same number of attempts.
- **The mirror line is y = 0**, not the corridor's center anywhere. Negating a double is exact, so the mirrored run does the same arithmetic with the signs turned. Around another line the coordinates round differently, and a fall that ends exactly on a floor (a ship coming back to the height it left) lands one tick apart in the two runs: a rounding tie, not a direction bug, but enough to make every later tick differ.
- **What the mirror catches:** with a `gravity_dir` removed from the circle's face rule, from a slope's rise speed, from the jump zone or from the step's ceiling test, the mirror fails and no other test does.
- **The game layer:** a red portal, the cube's icon rolling. The level list has four columns instead of three, so twelve levels fit on screen.
- **Tests:** `tests/test_ball.c` (the row, a flip from rest and back, a hold flips once, clicks in the air, landing under a block, the ceiling bounce, spikes and walls, a ship turning into a ball against its ceiling, a wave turning into one on the ground, the rolling).

---

## 10. More modes and objects

Each builds on sections 6–9. Numbers are starting points; tune with the 6.8 formulas and lock with tests.

### 10.1 Jump pads

`pad x y size yellow|pink|red|blue [rot=...]`. GD's values, in velocity units (6.9: 1 unit = the normal scroll speed), set on touch:

| Pad | GD | px/s | Cube height |
|---|---|---|---|
| Yellow | 2.77 | 2876.9 | 4.38 blocks |
| Pink | 1.79 | 1859.1 | 1.83 blocks |
| Red | 3.65 | 3790.9 | 7.60 blocks |
| Blue | −1.37 | 1422.9 | flips the gravity, then launches toward the new floor (below) |

- yellow, pink, red: `vy = PAD_V` (`2.77 * V_UNIT` and so on), whatever the player was doing
- blue: flips the gravity (`player_flip_gravity`, PLAN 3.4), then **sets** `vy = -1.37 * V_UNIT`, a speed **toward the new floor**, like GD. On the ground, the player flips and is thrown at the ceiling at 1422.9 px/s instead of starting from rest: that's the snap a blue pad has in GD. The value is the same for mini (6.9), unlike the other pads

```c
static void pad_act(player_t *p, pad_kind_t kind)
{
    if (kind == PAD_BLUE) {
        player_flip_gravity(p);         /* PLAN 3.4: flip, keep the motion */
        p->vy = -1.37 * PER_TICK(V_UNIT);   /* then set it: toward the new floor */
        return;
    }
    p->vy = PER_TICK(PAD_V[kind]);      /* yellow 2.77, pink 1.79, red 3.65 */
}
```

A flip object's value is negative because it's a speed **toward** the floor the player is about to fall to; everything else sets a positive rise speed (6.9).

and `grounded = false`. Works in every mode (a pad under a UFO launches it). Pads are interactive objects (PLAN 5.1): activation on touch, then `spent` for the rest of the attempt (still drawn, no hitbox).

**Hitbox.** A pad is a thin plate at the bottom of its cell, like GD's, not a full square: `local = {x + 0.1w, y + 0.75h, 0.8w, 0.25h}` (for `size 2`: 80 × 25 px, flush with the cell's bottom), then rotated with the object (PLAN 4.2). A pad at `rot=180` hangs from a ceiling and works for flipped gravity.

**A pad's speed is never cut.** Modes have no rise caps (PLAN 3.4), so a yellow pad launches every mode at 2876.9 px/s, the ship included (its 2326.5 px/s limit only applies to what its thrust can add). How high it goes depends on the mode's gravity, as in GD.

Pads push **vertically** (relative to gravity): they set `vy`, never `vx`, even when rotated. A rotated pad only changes where its hitbox is.

### 10.2 Jump orbs

`orb x y size yellow|pink|red|blue|green|black`. When the player's rigid square touches the orb with a fresh hold, it activates. GD's values (6.9), in velocity units:

| Orb | GD | px/s | Effect |
|---|---|---|---|
| Yellow | 1.91 | 1983.7 | `vy = 1.91 * V_UNIT` (2.08 blocks high) |
| Pink | 1.37 | 1422.9 | `vy = 1.37 * V_UNIT` (1.07 blocks) |
| Red | 2.68 | 2783.5 | `vy = 2.68 * V_UNIT` (4.10 blocks) |
| Blue | −1.37 | 1422.9 | flips the gravity, then `vy = -1.37 * V_UNIT`: launched toward the new floor (like the blue pad, 10.1) |
| Green | 1.91 | 1983.7 | flips the gravity, then `vy = 1.91 * V_UNIT`: a yellow orb in the other gravity, the player jumps away from its new floor and then falls to it |
| Black | −2.6 | 2700.4 | no flip: `vy = -2.6 * V_UNIT`, slammed toward the floor at the fall cap |

GD's dash orbs (green and pink, value 0) are a different mechanic and stay out of scope. Precise rules:

- **When: on exactly the tick of the contact**, never before or after. Two cases:
  - the player **already overlaps** the orb at the start of the tick (it was touching it, and the hold just became fresh, e.g. a press while inside it): step 0 of 6.3, before `apply_input`, so the orb wins over the surface jump (code below);
  - the rigid square **first touches** the orb during the tick's movement, and the hold is still fresh then: the sweep collects it (PLAN 4.6), and step 5 of that same tick activates it, in path order with the other interactive objects. If the player jumped off a surface in step 1 of this tick, that jump used the hold before the contact, and the orb doesn't act (it wasn't touching the orb when it jumped).

  ```c
  static input_t activate_orbs(sim_t *s, input_t in)
  {
      rect_t outer = box(s->st.player.pos, MODES[s->st.player.mode].half);

      if (s->st.player.hold != HOLD_FRESH)
          return in;                           /* no hold, or a used one: orbs ignore it */
      for (size_t i = s->st.first_active; i < s->lvl.nb_objects; i++) {
          const object_t *o = &s->lvl.objects[i];

          if (o->hitbox.aabb.x >= outer.x + outer.w)
              break;
          if (o->type != OBJ_ORB || is_spent(&s->st, i) || !overlap_box_poly(outer, &o->hitbox))
              continue;
          orb_act(s, o);                       /* same effect as the pad of that color */
          set_spent(&s->st, i);
          s->st.player.hold = HOLD_USED;
          return (input_t){false, false};      /* the input is consumed this tick */
      }
      return in;
  }
  ```

- **Activation needs a fresh hold** (PLAN 3.4): the button is down and this hold hasn't jumped, flipped, hopped or activated an orb yet. It doesn't matter when the hold started: a hold started before reaching the orb and still down when the player touches it activates it on that tick (GD's buffered click). A press released before the contact does nothing.
- **The orb uses the hold.** While that hold stays down, the player keeps jumping on surfaces as usual, but touches no other orb; a new press (fresh again) is needed.
- **Orb first:** if the player touches a live orb with a fresh hold while it could also jump off a surface, only the orb acts: the tick continues with `held` **and** `pressed` false, so no surface jump happens. Clearing only `pressed` isn't enough: a grounded cube jumps on `held`, and its jump would overwrite the orb's velocity in step 1. The next tick sees the real `held` again (so holding keeps flying the ship after a ship-mode orb, for instance), and since the hold is now used, it jumps off surfaces but ignores orbs.
- One orb per hold: the loop returns after the first orb that acts, and the hold is used.
- Ship and wave holds are never used by flying (PLAN 3.4), so a ship holding for a while that enters an orb activates it.
- An orb is an interactive object (PLAN 5.1) whose activation condition is touch **and** a fresh hold. It becomes `spent` (no hitbox, still drawn) only when it actually acts; flying through it without pressing leaves it live.
- **Bot:** in addition to each mode's decision points, the bot makes a decision on every tick where the player overlaps a live orb, or is about to reach one: hold, keep holding, or **release**. Releasing matters: a used hold must be released before a new press can activate an orb, so "release now, press on the orb" is a path the bot has to be able to try. The hold state is part of the run state, so the bot's memo (PLAN 8.3) already tells a fresh hold from a used one.

### 10.2b As built (pads and orbs)

- **One table:** `LAUNCHES` (`src/sim/modes.c`) has a row per colour with its speed as a pad and as an orb, in GD's units, and whether it flips; 0 means that object doesn't come in that colour (no green or black pad). `launch` in `src/sim/interact.c` is the effect of both: flip if the colour flips, then **set** the rise speed.
- **Pads:** `pad x y size yellow|pink|red|blue`, the plate of 10.1 along the bottom of the cell, turned with `rot=`. Measured: a cube rises 432, 179 and 752 px from the yellow, pink and red pads, which is GD's 4.38, 1.83 and 7.60 blocks less half a tick of the launch speed. The wave sets its own speed every tick, so only a blue pad's flip shows on it.
- **Orbs:** `orb x y size yellow|pink|red|blue|green|black`. The hitbox is a square of 0.8 of the cell, **80 × 80 px** at size 2, centered. The two cases of 10.2 are `activate_orbs` (step 0, a press while already inside: the orb acts before the mode's input and the tick's input is consumed) and the interactive step (the square reaches the orb during the move with a hold still fresh).
- **The ball and the UFO need no special case:** the orb marks the hold used and consumes the press, so a ball can't flip and a UFO can't hop on the click an orb took.
- **Green is not a stronger blue.** It was first built as 10.2 said then, a flip and 1.91 toward the new floor, which played like a black orb with a gravity switch. It is a flip and then a yellow orb's jump in the new gravity: on screen the player first moves toward the side it is leaving, then falls to the other. Blue (pad and orb) stays a flip and a throw at the new floor. One consequence: clicked while standing on a surface, a green orb jumps *into* that surface, which stops the player there (a bounce for the modes that bounce) before the new gravity takes it away. It is meant to be clicked in the air.
- **The bot** makes a decision on every tick where a live orb overlaps the player horizontally or will within 12 ticks (`near_live_orb`, `src/sim/bot.c`), whatever the mode: a cube in the air has no other moment to choose in, and "release, then press on the orb" has to be a run it can try.
- **The game layer:** neither has an image yet. `src/atlas.c` draws two white shapes at startup, a plate and a ring around a dot, and each object is tinted with its colour and drawn in its hitbox.
- **Tests:** `tests/test_pads.c` (the grammar and the plate, the table, the three heights, set and not add, acts once, a ship and a UFO past their caps, the blue pad, a pad hanging from a ceiling, the wave), `tests/test_orbs.c` (the grammar and the box, every colour pressed inside, a buffered hold on the exact tick, a click released before, a used hold, one orb per hold, the orb before the surface jump, UFO and ball, the ship's hold), a pad level and an orb level in `tests/test_bot.c`, each impossible without its object and checked to be solved through it, and pads and orbs in the mirrored section (9.6): the 800 mirrored runs meet 127 pads and 318 orbs.

### 10.3 Speed portals

`speed x y size 0.5|1|2|3|4`: the names GD's players use, with **GD's own speeds** behind them (6.9). Interactive object (PLAN 5.1): on first touch, it sets the horizontal speed, constant until the next speed portal, then it's `spent`:

| File | GD's name | blocks/s | px/s | px/tick | Ratio |
|---|---|---|---|---|---|
| `0.5` | slow | 8.372 | 837.2 | 3.488333 | 0.806 |
| `1` | normal | 10.386 | 1038.6 | 4.3275 | 1 |
| `2` | fast | 12.914 | 1291.4 | 5.380833 | 1.243 |
| `3` | very fast | 15.6 | 1560 | 6.5 | 1.502 |
| `4` | extremely fast | 19.2 | 1920 | 8 | 1.849 |

```c
st->speed = SPEEDS[portal_index];            /* SPEED_SLOW .. SPEED_XFAST (PLAN 3.2) */
p->vx = PER_TICK(st->speed);
st->speed_mult = st->speed / SPEED_NORMAL;   /* kept for the editor and the HUD */
```

The engine already moves the player by `vx` and accumulates `distance` in double precision (PLAN 3.2), so nothing else in the movement changes. Horizontal speed is only ever changed by speed portals: contacts never slow the player down, and being stopped horizontally is death.

Consequences:

- Percent = `distance / end_shift`, and completion is `distance >= end_shift`.
- The wave stays at 45° automatically: its input sets `vy = ±vx` (8.2).
- Slopes stay slopes: sliding sets `vy` from `vx` and the surface angle (PLAN 4.4), at any speed. At 4× speed the player covers exactly 8 px per tick, which the swept engine handles without tunneling (PLAN 8.2). Vertical speeds don't change with the speed: the jump constants are tied to the **normal** speed (PLAN 3.2), so a cube jumps just as high at 4× and only covers more ground.
- Music stays time-based (`tick / TICK_RATE`), so it's unaffected.
- The editor maps x ↔ time piecewise: walk the speed portals in x order, accumulating `segment_length / speed` (11.10).
- Start positions store the speed (11.8), and snapshots include it (it's in the run state).

### 10.4 Mini portals

`mini x y size on|off`. `on` halves `half`, and scales each mode's constants (a mini cube jumps 1.3583 blocks instead of 2.1333, so 0.64× the height, and a mini UFO 1.2 instead of 1.5666, 0.77×; 6.9); `off` returns to normal size. Corridors don't change: a mini ship flies in the same 1000 px corridor. A **mini wave** isn't at 45°: its vertical speed is double its horizontal one (`vy = ±2 × vx`, 6.9). Implement as a `scale` on the player that every mode function multiplies in. Add it after all modes exist, since it touches all of them.

### 10.5 Robot, swing, spider

| Mode | Behavior | Implementation notes |
|---|---|---|
| Robot | Hold to jump higher: while held after takeoff, gravity is reduced for up to ~0.25 s | `boost_ticks` counter in `player_t`; `integrate` uses `g × 0.3` while `held && boost_ticks > 0` |
| Swing | Ship-like flight; each press flips gravity | Ship integration with `gravity_dir`; ball-style flip on `pressed` |
| Spider | Press teleports to the opposite surface and flips gravity | Needs a vertical raycast among nearby blocks, corridor and ground to find the landing surface; set `pos.y` directly |

### 10.6 Saws

`saw x y size [rot=...]`: a **harm** object (PLAN 3.0) with a **circle** hitbox: center at the rect's center, radius `0.42 × w` (a little inside the sprite's teeth, forgiving like spikes). Any contact kills. Rotation doesn't change a circle, but the sprite can spin as decoration (game layer).

The sweep is written out in PLAN Appendix G.10: the two slabs and the four corner discs, with the quadratic and which root to keep. Any hit in the move's range kills. It's a `SHAPE_CIRCLE` branch in the harm sweep; everything else (broadphase, rotation, containment counting as contact) already applies.

### 10.7 Order

Pads and orbs first (cheap, and they add the most level variety), then saws and speed portals (speed portals are needed for musical sync on longer levels), then robot, swing, spider, mini.

---

## 11. Level editor

### 11.1 Data model

The editor edits an **unsorted** list of objects with **stable ids**:

```c
typedef struct ed_object {
    int id;                     /* never reused within a session */
    object_t obj;               /* the sim's struct (PLAN 3.3): rect, rotation, hitbox */
    char *extra;                /* fields the editor doesn't edit (group=, unknown keys),
                                   written back unchanged on save */
    bool selected;
} ed_object_t;

typedef struct ed_level {
    char file[64];              /* name in levels/, "" for a new level */
    level_header_t hdr;         /* name, author, music, bpm, the start_ fields */
    ed_object_t *objects;
    size_t count;
    size_t cap;
    int next_id;
    bool dirty;
} ed_level_t;
```

After any change to an object (move, rotate, resize, type), recompute its hitbox with PLAN 4.4's `hitbox_build`, so the canvas, the hit test, the validation and the playtest all see the same shape.

Why `extra`: the file format has fields the editor doesn't know how to edit, like `group=` (reserved for triggers) or keys from a newer version. The editor must never drop them: opening and saving a level without touching an object gives back the same line.

Why ids: selection, undo and redo refer to objects that may move in the array when others are deleted. An index would silently point at the wrong object; an id can't.

Why unsorted: the editor inserts and deletes constantly; keeping the array sorted would cost a shift on every edit for no benefit. The sim needs sorting, and gets a sorted copy.

The loader from PLAN 7.3 is split in three:

```c
int level_parse_mem(const char *buf, size_t len, object_t **objs, char ***extras,
    size_t *count, level_header_t *hdr, sim_log_fn log);           /* extras: per object, may be NULL */
int level_write(const char *path, const object_t *objs, char *const *extras,
    size_t count, const level_header_t *hdr);
int sim_init(sim_t *s, const object_t *objs, size_t count);
    /* copy, hitboxes, sort, reach, end_shift, kill_y, spent bitset (PLAN 7.3 step 6) */
```

`sim_load_mem` = `level_parse_mem` + `sim_init`, and `sim_load` reads the file then calls it (PLAN 2.3). The editor uses `level_parse` to open, `level_write` to save, `sim_init` to playtest and verify.

Selection is the `selected` flag in `ed_object_t` (simpler than a separate id set, and fast enough).

### 11.2 Views and layout

```text
y=0    +--------------------------------------------------------------------------+
       | Save  Play  Verify  Undo  Redo | Grid 50 | Zoom 100% | ♪ Title  0.50s     |  toolbar, 80 px
y=80   +---------+----------------------------------------------------------------+
       | palette |                                                                |
       | 220 px  |                     canvas (editor view)                       |
       |         |                                                                |
y=900  |         +----------------------------------------------------------------+
       |         | properties, 180 px                                             |
y=1080 +---------+----------------------------------------------------------------+
```

- **UI view** (1920×1080): toolbar, palette, properties.
- **Editor view**: the canvas. Its viewport is the canvas rectangle, in the window's normalized coordinates, combined with the letterbox (PLAN 9.7): `{220/1920, 80/1080, 1700/1920, 820/1080}` scaled into the letterbox viewport. Its size is `1700 × 820 × zoom` world pixels.

Events go to UI panels first; only if the mouse is over the canvas and no panel consumed the event does the canvas get it.

### 11.3 Camera and zoom

```c
typedef struct ed_camera {
    vec2_t center;       /* world coordinates */
    float zoom;          /* 1 = 1 world px per screen px; 2 = zoomed out x2 */
} ed_camera_t;
```

Zoom around the mouse, so the world point under the cursor stays under the cursor:

```c
void ed_zoom_at(editor_t *ed, gd_t *gd, float factor)
{
    sfVector2i px = sfMouse_getPositionRenderWindow(gd->w);
    sfVector2f before = sfRenderWindow_mapPixelToCoords(gd->w, px, ed->view);
    sfVector2f after;
    float z = ed->cam.zoom * factor;

    ed->cam.zoom = z < 0.25f ? 0.25f : (z > 4.0f ? 4.0f : z);
    ed_apply_camera(ed);                         /* sfView_setSize / setCenter from cam */
    after = sfRenderWindow_mapPixelToCoords(gd->w, px, ed->view);
    ed->cam.center.x += before.x - after.x;
    ed->cam.center.y += before.y - after.y;
    ed_apply_camera(ed);
}
```

Wheel: scroll horizontally by 200 px × zoom. Ctrl+wheel: `ed_zoom_at(ed, gd, delta > 0 ? 0.8f : 1.25f)`. Middle-drag or Space+drag: pan by the mouse movement × zoom.

### 11.4 Grid and snapping

- Grid size 50 (G toggles 25). Objects snap by their **top-left corner** (the file format stores top-left).
- Placing: the object's top-left = `snap(mouse_world)` where `snap(p) = floor(p / grid) × grid`. Using `floor` (not `round`) means the object appears in the cell the mouse is in.
- Moving by drag: snap the **delta**, not the object positions: `delta = snap(mouse_now − mouse_at_drag_start)`, applied to each object's original position. This keeps objects that were off-grid (placed at 25 px, or imported) at the same offset from the grid, instead of all jumping to grid lines.
- Draw only visible grid lines, in one `sfVertexArray` of `sfLines`, rebuilt when the camera changes. Every 4th line brighter (every 200 px = 2 blocks). Draw the ground line (y = 850), the level end (`end_shift + PLAYER_SCREEN_X`) and the kill ceiling (`kill_y`, PLAN 4.7) as distinct colored lines.
- Objects are drawn with the game's renderer (PLAN 9.2: atlas, layers), so the canvas looks exactly like the level. The editor's vertex buffers are rebuilt for the chunks an edit touches, not for the whole level.

**Rotation and size.**

- `R` rotates the selection by +90°, `Shift+R` by −90°: each object's `rotation` changes, and with several objects selected, their positions also rotate around the selection's center (like GD), then snap their top-left to the grid.
- `[` and `]` rotate by −15° and +15° for free angles; the properties panel takes an exact angle.
- The properties panel edits `w` and `h` in grid units (and `size` for square objects). Dragging a handle on a single selected object resizes it in grid steps.
- Every rotation or resize is one `MODIFY` command (11.7).

### 11.5 Tools as a state machine

```text
                 +-----------------------------------------------+
                 v                                               |
[IDLE] --left press on empty canvas, Place tool--> place object, push ADD, stay IDLE
[IDLE] --left press on object, Select tool-------> [DRAG_PENDING]
[IDLE] --left press on empty, Select tool--------> [BOX_SELECT]
[IDLE] --middle press or Space+left press--------> [PANNING]
[IDLE] --right press on object--------------------> delete it, push REMOVE, stay IDLE
[DRAG_PENDING] --mouse moved > 4 px--------------> [DRAGGING] (select object if it wasn't)
[DRAG_PENDING] --release-------------------------> click select (Shift toggles), IDLE
[DRAGGING] --mouse move--------------------------> preview moved positions
[DRAGGING] --release-----------------------------> push one MOVE (total snapped delta), IDLE
[DRAGGING] --Escape------------------------------> cancel (restore positions), IDLE
[BOX_SELECT] --release---------------------------> select objects intersecting the box, IDLE
[PANNING] --release------------------------------> IDLE
```

The 4 px threshold stops a slightly shaky click from becoming a 1-grid move.

Hit test: objects whose **rotated** rect contains the mouse (the point-in-rectangle test in the object's own rotated frame: rotate the mouse point by `-rotation` around the rect's center, then test the unrotated rect). If several, the one drawn **on top** wins, matching what's visible: the game's draw order is by layer (blocks, then hazards, then interactive objects) then by x (PLAN 9.2), and the editor uses the same order, so what you click is what you see in both. Linear search is fine for thousands of objects.

### 11.6 Shortcuts

| Keys | Action |
|---|---|
| 1–9 | Pick palette entry |
| R / Shift+R | Rotate selection +90° / −90° |
| [ / ] | Rotate selection −15° / +15° |
| Tab | Toggle Place / Select tool |
| Delete / Backspace | Delete selection |
| Arrows | Move selection 1 grid step |
| Shift+arrows | Move selection 1 px |
| Ctrl+A | Select all |
| Ctrl+C / Ctrl+V | Copy / paste at mouse |
| Ctrl+D | Duplicate selection one grid step to the right |
| Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) | Undo / redo |
| Ctrl+S | Save |
| Enter / Shift+Enter | Playtest from start / from start position |
| G | Grid 50 / 25 |
| F3 | Hitboxes (PLAN 9.6) |
| Escape | Cancel drag / close dialog / leave editor (asks if dirty) |

Keyboard shortcuts don't fire while a text field has focus (level name, typed coordinates).

### 11.7 Undo and redo

```c
typedef enum cmd_kind { CMD_ADD, CMD_REMOVE, CMD_MOVE, CMD_MODIFY } cmd_kind_t;

typedef struct command {
    cmd_kind_t kind;
    ed_object_t *before;    /* REMOVE, MODIFY: copies before the change */
    ed_object_t *after;     /* ADD, MODIFY: copies after the change      */
    int *ids;               /* MOVE: ids moved                            */
    size_t count;
    float dx;
    float dy;
} command_t;
```

| Command | Do | Undo |
|---|---|---|
| ADD | insert `after[i]` (with their ids) | remove objects with those ids |
| REMOVE | remove objects with those ids | insert `before[i]` back (same ids) |
| MOVE | add (dx, dy) to each id | subtract (dx, dy) |
| MODIFY | replace each id's object with `after[i]` | replace with `before[i]` |

Rules:

1. Every edit goes through `editor_do(ed, cmd)`: apply, push on the undo stack, **free and clear the redo stack**, set `dirty`.
2. `undo`: pop, apply the inverse, push on redo. `redo`: the reverse.
3. One user gesture = one command: a drag, a paste, a multi-object property change.
4. Removed objects keep their ids, so an undo brings back the *same* objects (and a later redo of a MOVE still finds them).
5. Stack capped at 256 commands; when full, free the oldest.
6. Commands own their arrays; freeing a command frees them.

Test (13): 1000 random commands, then undo all → identical to the start; redo all → identical to the end.

### 11.8 Start positions

A level can hold any number of **start positions**: `start x y <mode> [up|down] [speed]` lines in its body. Any gamemode, either gravity, any speed from 10.3's list. They are markers and nothing else: no hitbox, invisible in play, and the run never meets one.

- They are **alternatives** to the level's real start, which is the header's (`start_x`, `start_gamemode` and the others, PLAN 7.2). Placing, moving or deleting a start position never changes the real one.
- They form a list, in the file's order. In the game and in a playtest the player moves through it with **A** (previous) and **E** (next); the real start is the first entry.
- **Only an attempt begun at the real start counts**: one begun at a start position records no best and no completion. It is practice.

An attempt begins at one with:

```c
void sim_reset_at(sim_t *s, double x, double y, gamemode_t mode, int gravity_dir, double speed)
{
    const mode_ops_t *m = &MODES[mode];
    run_state_t *st = &s->st;
    player_t *p = &st->player;

    sim_reset(s);
    st->speed_mult = speed;
    st->tick = (long)((x - PLAYER_SPAWN_X) / (PER_TICK(SCROLL_SPEED) * speed));
    st->distance = (double)st->tick * PER_TICK(SCROLL_SPEED) * speed;
    p->pos = (vec2_t){PLAYER_SPAWN_X + st->distance, y};
    p->prev_pos = p->pos;
    p->mode = mode;
    p->gravity_dir = gravity_dir;
    p->grounded = false;
    p->can_jump = false;
    for (size_t i = 0; i < s->lvl.nb_objects; i++) {
        const object_t *o = &s->lvl.objects[i];

        if (o->hitbox.aabb.x + o->hitbox.aabb.w < p->pos.x - m->half)
            set_spent(st, i);         /* interactive objects behind the start are spent */
    }
    if (m->corridor_height > 0.0)
        corridor_from_center(s, y);   /* same rule as a portal centered at y */
    st->cam.pos.y = camera_rest_y(s);  /* where the camera would settle for this position */
}
```

`tick` is rounded down so `x` stays exactly on the tick grid (music sync uses `tick`). With several speed portals before the start, the tick is an approximation of the real elapsed time (the level's earlier segments ran at other speeds); music uses `ed_time_at_x` (11.11) instead of `tick` for the song position in that case.

Music: `music_play_from(offset + ed_time_at_x(x) + audio_offset)`.

### 11.9 Playtest

1. Build a sorted `sim_t` from the editor objects (`sim_init`), excluding `start` objects.
2. Reset from the level start, or from the start position (Shift+Enter).
3. Run the normal level loop (PLAN 3.6) inside the editor scene, with a banner "Playtest — Esc to stop". Progress is **not** saved.
4. Record the player's position every 4 ticks into a path array.
5. On death: show the explosion for the usual delay, then return to the editor with the camera centered on the death point.
6. On completion or Escape: return to the editor.
7. In the editor, draw the recorded path as a thin line until the next edit. It shows exactly where jumps happen and how close hazards are: most of level design is reading this line.

### 11.10 Verify (bot)

The Verify button:

1. `sim_init` a copy of the level.
2. Run the bot (PLAN 8.3, mode-aware decisions from `bot_decision_ticks`) with a cap of 300 000 simulated attempts.
3. Show one of:
   - "Bot found a path ✓" and draw the bot's path like a playtest path.
   - "Bot found no path — furthest 63%" with a marker at the furthest x reached, and the path of that attempt.
   - "Undecided (search limit reached)" if the cap is hit (rare for hand-made levels; raise the cap or make the level shorter to test sections).

The result is **information only** (PLAN 8.3): the bot's search is coarser than a human and can miss a path. It never blocks saving, playtesting or playing; you check the flagged spot yourself.

On the prototype, the 7 legacy levels and the 3 mode test levels each take well under a second, so it can run synchronously. If a very long level ever takes more than ~200 ms, run it in steps across frames (e.g. 2 000 attempts per frame) with a progress indicator.

Show next to it: level duration (`end_shift / SCROLL_SPEED` s) vs song duration (4.9).

### 11.11 Music tools

**Play from here** (P): plays the song from the time matching the canvas center:

```c
float ed_time_at_x(const ed_level_t *lv, float world_x)
{
    return lv->hdr.music_offset + (world_x - PLAYER_SPAWN_X) / SCROLL_SPEED;
}
```

(With speed portals: sum each segment's length divided by its speed, walking portals in x order.)

A vertical line scrolls with the song while it plays, at `x = PLAYER_SPAWN_X + (song_time − offset) × SCROLL_SPEED`, so you see where the music is in the level.

**Beat lines**: set BPM and first-beat time (a text field each, stored as `bpm 128` and `first_beat 0.35` in the header, PLAN 7.2). Beat `k` is at song time `first_beat + k × 60 / bpm`, so at:

```c
float beat_x(int k, const level_header_t *h)
{
    float song_time = h->first_beat + (float)k * 60.0f / h->bpm;

    return PLAYER_SPAWN_X + (song_time - h->music_offset) * SCROLL_SPEED;
}
```

Draw only beats in view: compute `k_min` and `k_max` from the view's left and right edges by inverting the formula. Every 4th beat (bar) brighter. At the normal speed (1038.6 px/s) and 128 BPM, beats are 487 px apart: about 3.5 blocks.

### 11.12 Saving

1. New level: ask for a name (text field). The file is `levels/<id>.gd`, the id being one past the highest that exists (PLAN 7.2: ids are digits, and progress is keyed by them); the name goes in the `name` header.
2. Write `levels/<file>.tmp` with `level_write`, check `fclose`, `rename` over `levels/<file>`.
3. Content: the header fields that aren't at their default value, then objects **sorted by x** (then y). Sorted files are readable and give small git diffs. Optional fields are written only when they differ from their default (`rot=` when not 0, `w=`/`h=` when not equal to `size`), followed by the object's `extra` fields unchanged.
4. `dirty = false`.

Autosave: every 60 s if dirty, and before every playtest, write to `save/editor_autosave/<file>` (not over the real file). When opening a level whose autosave is newer than the level file, ask "Restore unsaved changes from <time>?". Delete the autosave after a successful normal save.

Leaving with unsaved changes: dialog "Save changes to <name>?" with Save / Discard / Cancel (focused).

### 11.13 Validation shown in the editor

Recomputed after each edit (cheap):

- Identical objects on top of each other (same type, position, size).
- Objects below the ground (`y ≥ 850` for anything but decoration).
- Portals with no following cube portal before the end (not an error, but often a mistake).
- Level longer than the song.
- Spikes that overlap a block (often intended, sometimes not): shown as a hint, not a warning.
- Surfaces facing up that are steeper than 50°: they're walls, not floors (PLAN 4.4), which is rarely intended for a rotated block or slope.

Show counts in the toolbar; clicking cycles the camera through the flagged objects.

### 11.13b As built: groundwork, E1 and E2

- **The loader is three parts** (`src/sim/level_parse.c`, `level_write.c`, `level_build.c`). `level_parse_doc` reads a level as a document: its objects in file order, the fields of each line nothing edits (`group=`, unknown keys) kept as written, and its start positions. `level_write` gives the text back: the header fields that aren't at their default, the start positions, then the objects by x then y with only the fields each needs, written to a `.tmp` file and renamed. `sim_init` builds a simulation from a document. `sim_load_mem` is now parse then build; every level plays tick for tick as before.
- **What a save keeps and loses.** Written and parsed again, every level of `levels/` gives the same text and plays the same. A save does **not** keep comments, blank lines or the order of the lines: the file is rewritten whole.
- **Start positions (11.8)** are parsed, kept in `lvl.starts`, saved, and drawn in the editor as faint blue icons. Choosing one with A/E and attempts that don't count come with E5.
- **The text field** is a toolkit widget (`W_TEXT`): typed characters, Backspace, Enter confirms, a click only focuses it, and no key is a shortcut while it has the focus.
- **The editor's level** (`src/editor/ed_level.c`, pure and tested): objects with ids never given twice, open, save, the cell under the mouse, what a click hits (the object drawn on top, in its own turned frame), and the palette's entries, generated from the object, mode and colour tables. A placed object is made by the loader from its own line, so what is placed is what the game reads back; the same object is never placed twice in a cell.
- **E1, the canvas** (`src/editor_canvas.c`, `editor_draw.c`): the level drawn with the game's own vertices and atlas, pan (middle button or Space and drag), the wheel along the level, Ctrl and the wheel to zoom around the mouse from 25% to 400%, a grid that thins out when zoomed out, the ground, the level's end and the kill ceiling as lines, the real start and the start positions as icons. Objects are rebuilt each frame for what is in view, not kept in the game's chunk buffers: simpler, and fast enough at this size.
- **E2, making a level** (`src/editor_scene.c`, `editor_menu.c`): the editor opens on a list of the levels and a field to name a new one. Left click places the palette's entry (keys 1 to 9, the arrows, or a click on it), dragging paints, right click deletes, G switches the grid between 50 and 25, Ctrl+S saves. Leaving with unsaved changes warns once; the dialog is E7's.
- **The level list** in the game scrolls (wheel, Up, Down), so a thirteenth level is reachable.
- **Tests:** `tests/test_doc.c` (start positions, kept fields, every kind of object line written back as it was, the header, every level of `levels/` round trip and played from a document, the file written whole), `tests/test_editor.c` (ids, open and save, snapping, the hit test, the palette, placing, a placed level played, the next id) and the text field in `tests/test_ui.c`.

### 11.13c As built: E3

- **Undo and redo** (`src/editor/ed_history.c`, pure and tested). Every edit of the objects goes through it. Three kinds of command instead of 11.7's four: `ADD`, `REMOVE` and `CHANGE`, which keeps each object whole before and after. A move is a `CHANGE`, so undoing it puts back the exact numbers and never `x + dx - dx`; E4's turns and resizes will be the same command. A removed object comes back with its id and at its place in the list, so what is drawn on top is the same again.
- **One gesture, one step.** The scene starts a gesture at each mouse press and each key; objects added, or removed, during the same gesture join the same command. A painted stroke, a paste, a deleted selection are each one Ctrl+Z. 256 commands are kept; a new edit forgets what was undone.
- **The selection** (`src/editor/ed_select.c`, pure and tested) is each object's flag: a click selects one, Shift+click toggles one, a box takes everything drawn inside or across it (Shift adds to what was selected), Ctrl+A takes all.
- **Moving.** A press on an object becomes a drag after 4 px. The selection is grabbed as it was, and every position is that plus the mouse's travel **rounded** to the grid (11.4 says `snap`, but `floor` would move a whole cell for one pixel to the left); objects off the grid keep their offset. Dropping it is one command, Escape puts everything back. The arrows move the selection one grid step, with Shift one pixel. Z, Q, S and D move it a whole block, 100 px, whatever the grid. Each move rebuilds the hitbox.
- **Copies.** Ctrl+C keeps copies (the fields nothing edits included), Ctrl+V puts their top left corner in the cell under the mouse, cell to cell so offsets are kept, Ctrl+D puts the selection again one grid step to the right. The copies become the selection.
- **Two tools** (`src/editor_mouse.c`), Tab switches: Place (E2's painting) and Select. Right click deletes in both. Picking a palette entry goes back to Place. Placing drops the selection. A held move key (arrows, Z, Q, S, D) or Ctrl+Z / Ctrl+Y repeats: once at the press, then after 350 ms every 60 ms. The editor does it itself, the window's own key repeat being off for the game's input (1.3). Escape cancels a drag or a box, then drops the selection, then leaves. No key edits while a mouse button is in the middle of something.
- Selected objects are tinted green and outlined along their own turned rect.
- **Tests:** `tests/test_edit.c`: each command undone and redone, strokes as one step, the 256 limit, the selection, a drag (travel not added up, offsets kept, hitbox moved, cancel, a drag back to where it started is no step), copies into another level, and 11.7's random test: six runs of random edits, where every undo and every redo must give exactly the level there was at that depth, ids and order included.

### 11.14 Stages

| Stage | Contents | Done when |
|---|---|---|
| E1 | Canvas, camera, zoom around mouse, grid, draws a loaded level | Every existing level displays correctly at all zooms |
| E2 | Place, right-click delete, save, open, new level | A level made in the editor loads in the game |
| E3 | Select tool, box select, drag with snapped delta, undo/redo, copy/paste/duplicate | The random undo test passes |
| E4 | Palette from `MODES`, properties panel, all object types | Every object type can be created and edited |
| E5 | Playtest, start position, path display | Playtest from a start position inside a ship section works |
| E6 | Song choice, play from here, beat lines | Beat lines match audible beats on a known-BPM song |
| E7 | Verify, validation, autosave, unsaved-changes dialog | A level with an impossible jump shows "Bot found no path" at the right spot, as information |

---

## 12. Level format v2 grammar

```text
file        = [ "version" SP int NL ] { line }
line        = blank | comment | header | object | start
comment     = "#" { char } NL                        ("#" also ends any line early)
header      = "name" SP text NL
            | "music" SP file [ SP float ] NL         (file: no spaces)
            | "bpm" SP float SP float NL              (bpm, first beat seconds)
object      = type SP num SP num SP int [ SP word ] { SP field } NL
start       = "start" SP num SP num SP mode [ SP ( "up" | "down" ) ] [ SP speed ] NL
type        = "block" | "slope" | "spike" | "saw" | "portal" | "gravity" | "speed"
            | "pad" | "orb" | "mini"
field       = key "=" value                           (no spaces around "=")
key         = "rot" | "w" | "h" | "group" | [a-z_]+   (unknown keys: warning, ignored)
num         = [ "-" ] digits [ "." digits ]
SP          = one or more spaces or tabs
NL          = "\n" or "\r\n" or end of file
```

Extra word per type:

| Type | Extra word | Values |
|---|---|---|
| `block`, `slope`, `spike`, `saw` | none | |
| `portal` | mode | any `MODES[].name` |
| `gravity` | direction | `up`, `down` |
| `speed` | multiplier | `0.5`, `1`, `2`, `3`, `4` (or your set) |
| `pad` | kind | `yellow`, `pink`, `red`, `blue` |
| `orb` | kind | `yellow`, `pink`, `red`, `blue`, `green`, `black` |
| `mini` | state | `on`, `off` |

Fields, any object (PLAN 7.2):

| Field | Value | Default |
|---|---|---|
| `rot` | degrees, any number | `0` |
| `w`, `h` | grid units, integer ≥ 1 | `size` |
| `group` | comma list of integers (reserved for triggers) | none |

`start` has no size and its own rule: parse it before the generic object rule. Its mode is any `MODES[].name`, its speed any value from the `speed` list (default `1`).

Rules:

- No `version` line = version 1 (today's files). Version 1 files may start with the legacy progress header (PLAN 7.2).
- Unknown header keywords and unknown object types: warning with file and line, line skipped. Unknown fields: warning, field ignored, line kept. Newer files open in older builds as far as possible.
- A number that doesn't parse completely (`2.5` where an integer is expected, `750x`, `nan`, `inf`, out of range), a size, width or height of **0 or less**, a missing or unexpected extra word: warning, line skipped. There's no maximum.
- Duplicate `name`/`music`/`bpm` lines: last one wins, warning.

---

## 13. Test plan with expected values

All in `tests/`, linking only `src/sim/` (PLAN 8). "Measured" values come from the prototype; your implementation should match them to within ±1 px or ±1 tick.

| Area | Test | Expected |
|---|---|---|
| Input | held for 2 s | one `pressed`, held on every tick |
| Input | a press inside tick k | `pressed` on tick k only |
| Input | press+release inside one tick | that tick `pressed=1, held=0` |
| Input | two presses inside one tick | one `pressed`, nothing on the next tick |
| Settings | round trip | identical struct |
| Settings | `fps_limit=75`, `music_volume=300`, garbage line | defaults for those, warnings, others kept |
| Settings | unknown key | preserved on save |
| Cube | jump from flat ground | apex 213.32 px (GD's 2.1333 blocks), airtime 102 ticks (0.425 s), length 441.4 px |
| UFO | one press from the ground | rise 150.9 px, apex at 0.287 s, lands at 0.583 s |
| UFO | holding after the press | same as above |
| UFO | hop during a yellow pad's launch (2876.9 px/s) | `vy` becomes exactly 1470.7 px/s (set, not added) |
| Orbs | pink orb right after a yellow pad | `vy` becomes the pink orb's speed; the pad's momentum is discarded |
| UFO | press every 140 ticks for 4 s | altitude drift under 60 px |
| Wave | held 40 ticks | dx = dy = 125.00 px |
| Wave | touching corridor floor, then press | leaves the floor on the same tick |
| Ball | flip across the ball's 800 px corridor, from rest | ≈0.42 s (about 100 ticks) |
| Gravity | a pure flip (gravity portal, ball click) | world velocity identical just before and just after; `gravity_dir` negated; `vy` negated; nothing else changes |
| Gravity | blue pad or blue orb on flat ground | `gravity_dir` negated, then `vy = −1422.9` px/s: the player leaves for the ceiling at that speed whatever it was doing |
| Gravity | green orb while falling | flip, then `vy = +1983.7` px/s, a jump away from the new floor; the previous fall speed is discarded |
| Ball | press and release 1 to 20 ticks before landing | no flip |
| Ball | hold from before landing, through the landing | flips on the landing tick; still held, flips again on the next landing |
| Holds | cube jumps off the ground and keeps holding into an orb | the orb doesn't activate; the cube lands and jumps again |
| Holds | hold started in the air before an orb, still down when touching it | the orb activates on the first touching tick, not one tick later |
| Holds | cube on the ground jumps (held) in the same tick it would first touch an orb | the jump used the hold before the contact: the orb doesn't act |
| Pads | cube running onto a yellow pad on the ground, button held | launched at the pad's speed; no jump overwrites it on the next tick (`can_jump` is computed after the pad) |
| Holds | press released one tick before touching an orb | nothing |
| Holds | fresh press while on the ground and touching an orb | only the orb acts, no surface jump that tick |
| Holds | ship holding for 2 s enters an orb | the orb activates (ship holds are never used) |
| Holds | UFO hops, keeps holding into an orb | nothing; a new press on the orb activates it |
| Holds | button held while respawning, orb at the spawn | the orb activates (a hold carried into an attempt is fresh) |
| Contacts | flipped ball under a block with bottom y = 400 | grounded at exactly y = 450 |
| Contacts | UFO hop into a block's underside at 1470.7 px/s | bounces back at −441.2 px/s (30%) |
| Contacts | ship pressed slowly against a corridor ceiling | slides with `vy = 0` (under `BOUNCE_MIN_SPEED`), no vibration |
| Contacts | wave touching a block's top face or a slope | dies; touching the ground or a corridor surface: slides |
| Corridors | ship portal at y = 250, then a ball portal at y = 250 | bounds −150..850 (1000 px), then −50..750 (800 px); the camera keeps each corridor centered on screen |
| Corridors | cube portal while in a ship corridor | the corridor is removed; the camera eases back to the ground view |
| Contacts | wave held into the corridor ceiling; flipped wave released into the corridor floor | stops and slides along it, alive (surfaces never kill) |
| Contacts | wave sliding on the ground enters a cube portal | the cube ends up standing on the ground, alive |
| Portals | ship pressed against its corridor ceiling takes a cube portal | never touches the old ceiling again; its strips fade out over 0.25 s |
| Contacts | UFO at 2× speed, ship and ball at 3×, cube at 4×, riding down a 45° slope | `grounded` every tick, following the slope (faster than the fall cap) |
| Input | ship: press and release inside one tick | exactly one tick of thrust |
| Input | wave: press and release inside one tick | exactly one tick up, then down again |
| Contacts | flipped cube runs along a block's underside, then off its end | stays grounded along it, then falls upward |
| Saws | box passing 1 px outside the circle, near the rounded corner of the inflated shape | survives; 1 px inside: dies; a 20 px saw at 4× speed is still hit |
| Pads | yellow pad in UFO and ship mode | the launch is 2876.9 px/s in both, above the ship's 2326.5 px/s thrust limit; holding the ship doesn't add to it, gravity slows it |
| Pads | pad at `rot=180` on a ceiling, flipped cube | launched away from its floor (the ceiling), i.e. downward on screen |
| Orbs | grounded cube presses on an orb | the orb's velocity, no cube jump on the same tick; next tick `held` is live again |
| Rotation | mirrored level with `rot' = 180 − rot` | same bot result |
| Contacts | every test level, mirrored in its corridor | same bot result as the original |
| Portals | crossing any mode portal | exactly one mode change |
| Portals | second ship (or UFO, ball, wave) portal while in that mode | corridor moves to be centered on the second portal |
| Portals | wave going up enters a cube portal | the cube starts with `vy = +1038.6` px/s and rises about 57 px before falling; going down: `vy = -1038.6`, it lands at once |
| Portals | flipped ball through a cube portal | still flipped (`gravity_dir == -1`), now a cube, boundaries removed |
| Boundaries | ship, ball, wave, UFO portal, with gravity normal and flipped | boundaries present, same positions in both cases |
| Boundaries | gravity portal in cube mode | no boundaries appear |
| Kill ceiling | flipped cube with nothing above | dies when its top crosses `kill_y` (600 px above the highest hitbox), not by completing the level |
| Kill ceiling | normal gravity, yellow pad from the top of the highest object | survives however high it goes (no kill ceiling in normal gravity) |
| Bot | `ufo`, `wave`, `ball` test levels | path found (printed; a "no path" result is a warning, not a failure) |
| Bot | UFO test level played as a cube (portal removed) | no path found (printed) |
| Format | parse → write → parse on every level in `levels/` and `tests/levels/` | identical objects and header |
| Editor | 1000 random commands, undo all | identical to the initial level |
| Editor | then redo all | identical to the final level |
| Editor | play from the start to tick T on the ground in cube mode, then `sim_reset_at` at that player's x, y; run N more ticks with the same inputs on both | identical `sim_state_hash` after each of the N ticks (start positions only guarantee this for a grounded cube: elsewhere `vy` and the corridor legitimately differ) |
| Editor | open and save a level with `group=` fields and an unknown field, no edits | byte-identical object lines |
| Music (math only) | `music_check_sync` with a simulated 0.3 s lag | resyncs; with 0.03 s lag, doesn't |
| Music (math only) | `audio_offset_ms = 120` | every computed song position is 0.12 s later into the song |
| Input | jump bound to `MouseRight` only | a right click produces one `pressed` tick; a left click doesn't, and still clicks UI widgets |

---

## 14. Milestones and acceptance criteria

**M1 — Foundations**
- [ ] Input edges pass the four input tests
- [ ] Key repeat disabled; holding Space never produces a second press
- [ ] Jump bindings (keys, mouse buttons, gamepad) rebindable, defaults Space, Up, left click, gamepad A
- [ ] Settings store passes its tests; unknown keys preserved
- [ ] Widgets: button (activate on release), toggle, slider (mouse capture), cycler, key capture, list; all keyboard-navigable

**M2 — Music**
- [ ] Library scan with `songs.txt`; undecodable files skipped with a log line
- [ ] Song resolution order (override → level → default) and offset order implemented
- [ ] Sync table (4.8) implemented for every event
- [ ] Drift check resyncs after a forced freeze
- [ ] Per-level override from the level list, saved with the new progress format, old format still readable
- [ ] Credits on the end screen and a Credits page; every song has a license entry

**M3 — Options**
- [ ] All rows of 5.2 working with keyboard only and mouse only
- [ ] VSync and FPS limit never both active
- [ ] Display changes revert after 10 s unless confirmed
- [ ] Capture of keys, mouse and gamepad buttons, with swap and last-binding protection
- [ ] Audio offset slider and calibration
- [ ] Reset progress with backup

**M4 — Modes**
- [ ] Mode table used by loader, tick, contact responses, renderer, bot; modes never move the player
- [ ] Contacts gravity-aware (floors, ceilings, slopes with flipped gravity); mirrored tests pass
- [ ] Head hits: ship, UFO, ball bounce; cube and wave die
- [ ] UFO, wave (with trail), ball (a cube that flips) match the measured values
- [ ] Hold rule (PLAN 3.4): buffered clicks, used holds ignored by orbs, orb priority over surface jumps
- [ ] Gravity portals; mode portals never change `gravity_dir`; kill ceiling for players falling upward in flipped gravity
- [ ] Same-mode portals switch to the new corridor; corridors snapped to the grid; camera locked while inside
- [ ] Mode test levels: the bot finds a path (reported, never blocking)

**M5 — Editor**
- [ ] Stages E1–E7 each meet their "done when"
- [ ] Format round trip and undo tests pass

**M6 — Extras**
- [ ] Pads (thin hitbox, speed never capped), orbs (fresh hold, exact tick, priority over surface jumps)
- [ ] Saws (circle harm hitbox, swept)
- [ ] Speed portals with the double distance accumulator; wave slope follows speed; editor time mapping piecewise
- [ ] Robot, swing, spider, mini as wanted