/*
** ALEXNEX PROJECT, 2026
** sim/editor_keys.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_EDITOR_KEYS_H
    #define SIM_EDITOR_KEYS_H

    #include <stdbool.h>
    #include "sim/binding.h"

/*
** The editor's shortcuts (FEATURES 11.6, 2.2): each action is one whole
** combination, a key with its own Ctrl, Shift and Alt, so S and Ctrl+S are
** two. Rebound in the options' Editor section, saved as editor_<name>.
** The defaults are letters, arrows and named keys: no punctuation, which
** a French layout can't reach the way SFML reads it.
**
** m(ID, name, label, page, key, mods, repeats): `page` is the options'
** page it is listed on, `repeats` whether holding it acts again.
*/
    #define ED_KEY_LIST(m) \
    m(SAVE, "save", "Save", 0, KEY_S, MOD_CTRL, false) \
    m(UNDO, "undo", "Undo", 0, KEY_Z, MOD_CTRL, true) \
    m(REDO, "redo", "Redo", 0, KEY_Y, MOD_CTRL, true) \
    m(SELECT_ALL, "select_all", "Select all", 0, KEY_A, MOD_CTRL, false) \
    m(COPY, "copy", "Copy", 0, KEY_C, MOD_CTRL, false) \
    m(PASTE, "paste", "Paste", 0, KEY_V, MOD_CTRL, false) \
    m(DUPLICATE, "duplicate", "Duplicate", 0, KEY_D, MOD_CTRL, false) \
    m(DELETE, "delete", "Delete", 0, KEY_Delete, 0, false) \
    m(TOOL, "tool", "Place or select", 0, KEY_Tab, 0, false) \
    m(MOVE_LEFT, "move_left", "Move left", 1, KEY_Left, 0, true) \
    m(MOVE_RIGHT, "move_right", "Move right", 1, KEY_Right, 0, true) \
    m(MOVE_UP, "move_up", "Move up", 1, KEY_Up, 0, true) \
    m(MOVE_DOWN, "move_down", "Move down", 1, KEY_Down, 0, true) \
    m(NUDGE_LEFT, "nudge_left", "Move left 1 px", 1, KEY_Left, MOD_SHIFT, \
        true) \
    m(NUDGE_RIGHT, "nudge_right", "Move right 1 px", 1, KEY_Right, \
        MOD_SHIFT, true) \
    m(NUDGE_UP, "nudge_up", "Move up 1 px", 1, KEY_Up, MOD_SHIFT, true) \
    m(NUDGE_DOWN, "nudge_down", "Move down 1 px", 1, KEY_Down, MOD_SHIFT, \
        true) \
    m(BLOCK_LEFT, "block_left", "Move left a block", 2, KEY_Q, 0, true) \
    m(BLOCK_RIGHT, "block_right", "Move right a block", 2, KEY_D, 0, true) \
    m(BLOCK_UP, "block_up", "Move up a block", 2, KEY_Z, 0, true) \
    m(BLOCK_DOWN, "block_down", "Move down a block", 2, KEY_S, 0, true) \
    m(ROTATE, "rotate", "Turn a quarter", 2, KEY_R, 0, false) \
    m(ROTATE_BACK, "rotate_back", "Turn a quarter back", 2, KEY_R, \
        MOD_SHIFT, false) \
    m(TURN, "turn", "Turn 15 degrees", 2, KEY_T, 0, true) \
    m(TURN_BACK, "turn_back", "Turn 15 degrees back", 2, KEY_T, MOD_SHIFT, \
        true) \
    m(PLAYTEST, "playtest", "Playtest", 3, KEY_Enter, 0, false) \
    m(PLAYTEST_START, "playtest_start", "Playtest from a start", 3, \
        KEY_Enter, MOD_SHIFT, false) \
    m(SONG, "song", "Hear the song from here", 3, KEY_P, 0, false) \
    m(VERIFY, "verify", "Verify", 3, KEY_V, 0, false) \
    m(NEXT_ISSUE, "next_issue", "Next warning", 3, KEY_N, 0, false) \
    m(LEVEL_PANEL, "level_panel", "Level settings", 4, KEY_L, 0, false) \
    m(GRID, "grid", "Grid size", 4, KEY_G, 0, false) \
    m(LAYER_DOWN, "layer_down", "Previous layer", 4, KEY_PageDown, 0, \
        false) \
    m(LAYER_UP, "layer_up", "Next layer", 4, KEY_PageUp, 0, false) \
    m(HOME, "home", "Back to the start", 4, KEY_Home, 0, false) \
    m(KEYS, "keys", "Show the keys", 4, KEY_F1, 0, false) \
    m(ENTRY_1, "entry_1", "Palette 1", 5, KEY_Num1, 0, false) \
    m(ENTRY_2, "entry_2", "Palette 2", 5, KEY_Num2, 0, false) \
    m(ENTRY_3, "entry_3", "Palette 3", 5, KEY_Num3, 0, false) \
    m(ENTRY_4, "entry_4", "Palette 4", 5, KEY_Num4, 0, false) \
    m(ENTRY_5, "entry_5", "Palette 5", 5, KEY_Num5, 0, false) \
    m(ENTRY_6, "entry_6", "Palette 6", 5, KEY_Num6, 0, false) \
    m(ENTRY_7, "entry_7", "Palette 7", 5, KEY_Num7, 0, false) \
    m(ENTRY_8, "entry_8", "Palette 8", 5, KEY_Num8, 0, false) \
    m(ENTRY_9, "entry_9", "Palette 9", 5, KEY_Num9, 0, false)

    #define ED_KEY_ENUM(id, name, label, page, key, mods, rep) ED_KEY_##id,

typedef enum ed_key { ED_KEY_LIST(ED_KEY_ENUM) ED_KEY_COUNT } ed_key_t;

    #define ED_KEY_PAGES 6
    #define ED_KEY_PAGE_MAX 9     /* actions on one page: the rows that fit */

const char *ed_key_name(ed_key_t k);         /* "save": editor_save= */
const char *ed_key_label(ed_key_t k);        /* "Save" */
const char *ed_key_page_name(int page);      /* "Edit" */
int ed_key_page(ed_key_t k);
binding_t ed_key_default(ed_key_t k);
bool ed_key_repeats(ed_key_t k);
/* The actions on a page, in order, into out (ED_KEY_PAGE_MAX); how many. */
int ed_key_page_list(int page, ed_key_t *out);

/*
** What an editor shortcut can be: a key that isn't Escape (it cancels and
** leaves) nor a modifier alone (Ctrl, Shift, Alt, System), with any of
** Ctrl, Shift, Alt. BIND_NONE is unbound.
*/
bool ed_key_bindable(binding_t b);

/* The action a pressed combination is, or -1. */
int ed_key_find(const binding_t keys[ED_KEY_COUNT], binding_t pressed);

#endif
