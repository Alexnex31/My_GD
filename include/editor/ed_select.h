/*
** ALEXNEX PROJECT, 2026
** editor/ed_select.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_SELECT_H
    #define EDITOR_ED_SELECT_H

    #include "editor/ed_history.h"

/* The selection is each object's own flag (FEATURES 11.1). */
size_t ed_selected(const ed_level_t *lv);
void ed_select_none(ed_level_t *lv);
void ed_select_all(ed_level_t *lv);
/* A click on object `id` (-1: on nothing). With shift it toggles that one. */
void ed_select_click(ed_level_t *lv, int id, bool shift);
/* Everything drawn inside or across the box; `add` keeps what already was. */
void ed_select_box(ed_level_t *lv, rect_t box, bool add);
bool ed_delete_selected(ed_level_t *lv, ed_history_t *h);

/*
** The selection held while it moves: what each object was when it was
** grabbed. Every move is from there, so a drag never adds up rounding, and
** dropping it is one command whatever the path (11.4, 11.7).
*/
typedef struct ed_grab {
    int *ids;
    object_t *before;
    size_t count;
} ed_grab_t;

void ed_grab(ed_level_t *lv, ed_grab_t *g);
void ed_grab_move(ed_level_t *lv, const ed_grab_t *g, double dx, double dy);
bool ed_grab_drop(ed_level_t *lv, ed_history_t *h, ed_grab_t *g);  /* moved? */
void ed_grab_cancel(ed_level_t *lv, ed_grab_t *g);
/* Grab, move, drop: an arrow key. false with nothing selected. */
bool ed_nudge(ed_level_t *lv, ed_history_t *h, double dx, double dy);

/*
** How far a drag moves things: the mouse's travel to the nearest grid step.
** The travel is snapped, not the objects, so one placed off the grid keeps
** its offset (11.4).
*/
double ed_snap_delta(double travel, double grid);

/* Copies of the selection, to paste here or in another level. */
typedef struct ed_clip {
    object_t *objs;
    char **extras;
    size_t count;
    vec2_t corner;            /* the top left of all of them */
} ed_clip_t;

bool ed_copy(const ed_level_t *lv, ed_clip_t *clip);      /* false: nothing */
/* The copies' corner goes to the cell under `at`; they become the selection. */
bool ed_paste(ed_level_t *lv, ed_history_t *h, const ed_clip_t *clip,
    vec2_t at, double grid);
/* The selection again, one grid step to the right, selected instead. */
bool ed_duplicate(ed_level_t *lv, ed_history_t *h, double grid);
void ed_clip_free(ed_clip_t *clip);

#endif
