/*
** ALEXNEX PROJECT, 2026
** editor/ed_history.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_HISTORY_H
    #define EDITOR_ED_HISTORY_H

    #include "editor/ed_level.h"

    #define ED_HISTORY_MAX 256

typedef enum ed_cmd_kind {
    ED_ADD,
    ED_REMOVE,
    ED_CHANGE                 /* a move today; a turn or a resize the same way */
} ed_cmd_kind_t;

/* One edit, with what it takes to do it again and to take it back (11.7). */
typedef struct ed_cmd {
    ed_cmd_kind_t kind;
    int gesture;              /* the user's action it belongs to              */
    size_t count;
    size_t cap;
    ed_object_t *objs;        /* ADD, REMOVE: the objects, ids and extras     */
    size_t *at;               /* REMOVE: where each was when it left          */
    int *ids;                 /* CHANGE: whose object changed                 */
    object_t *before;         /* CHANGE: each one whole, so an undo is exact  */
    object_t *after;
} ed_cmd_t;

/*
** Everything that edits the level's objects goes through here. One gesture
** (a painted stroke, a drag, a paste) is one step back: objects added or
** removed during the same gesture join the same command. Pure, tested alone.
*/
typedef struct ed_history {
    ed_cmd_t undo[ED_HISTORY_MAX];
    size_t nb_undo;
    ed_cmd_t redo[ED_HISTORY_MAX];
    size_t nb_redo;
    int gesture;
} ed_history_t;

/* A new action of the user's begins: a press, a key. */
void ed_gesture(ed_history_t *h);

int ed_do_add(ed_level_t *lv, ed_history_t *h, const object_t *obj,
    const char *extra);                                   /* its id */
bool ed_do_remove(ed_level_t *lv, ed_history_t *h, int id);
bool ed_do_place(ed_level_t *lv, ed_history_t *h, const ed_entry_t *e,
    vec2_t at, double grid);
/*
** The objects `ids` were `before` and are what the level holds now: records
** it. The command keeps its own copies.
*/
void ed_do_change(ed_level_t *lv, ed_history_t *h, const int *ids,
    const object_t *before, size_t count);

bool ed_undo(ed_level_t *lv, ed_history_t *h);            /* false: nothing to */
bool ed_redo(ed_level_t *lv, ed_history_t *h);
void ed_history_free(ed_history_t *h);

#endif
