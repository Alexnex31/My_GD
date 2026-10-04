/*
** ALEXNEX PROJECT, 2026
** ui/options_rules.h
** File description:
** header file for my_gd project
*/

#ifndef UI_OPTIONS_RULES_H
    #define UI_OPTIONS_RULES_H

    #include "sim/settings.h"

/*
** The options screen's decisions, without a window (FEATURES 5): what a
** captured input does to the bindings, which window sizes are offered.
*/

/* The Controls rows: 6 jump slots, then the action keys (5.2). */
enum {
    SLOT_JUMP = 0,
    SLOT_RESTART = BINDINGS_MAX,
    SLOT_CHECKPOINT,
    SLOT_REMOVE_CHECKPOINT,
    NB_SLOTS
};

typedef enum rebind_result {
    REBIND_SET,                   /* assigned                                */
    REBIND_SWAPPED,               /* it was another row's: the rows swapped  */
    REBIND_CLEARED,               /* Backspace                               */
    REBIND_SAME,                  /* already this row's: nothing to do       */
    REBIND_REFUSED                /* it would leave no jump input (5.5)      */
} rebind_result_t;

/* The rows as the settings have them; empty jump slots are BIND_NONE. */
void slots_from_settings(const settings_t *s, binding_t slots[NB_SLOTS]);

/* Back into the settings: the jump bindings packed, in the rows' order. */
void slots_to_settings(settings_t *s, const binding_t slots[NB_SLOTS]);

/*
** Row `slot` captured `b` (5.5): BIND_NONE clears it; an input another row
** has swaps the two rows (`*other` says which); at least one jump input
** always stays.
*/
rebind_result_t rebind(binding_t slots[NB_SLOTS], int slot, binding_t b,
    int *other);

/* "Jump 2", "Restart", "Place checkpoint", "Remove checkpoint". */
void slot_name(int slot, char *buf, size_t size);

    #define WINDOW_SIZES_MAX 4

typedef struct window_sizes {
    int w[WINDOW_SIZES_MAX];
    int h[WINDOW_SIZES_MAX];
    char label[WINDOW_SIZES_MAX][16];
    const char *labels[WINDOW_SIZES_MAX];   /* for a cycler's choices     */
    int count;
    int current;                  /* the settings' size among them         */
} window_sizes_t;

/*
** 1280x720, 1600x900 and 1920x1080, those that fit the desktop (5.2), and
** the settings' own size first if it's none of them.
*/
void window_sizes_list(window_sizes_t *ws, int desk_w, int desk_h,
    int cur_w, int cur_h);

#endif
