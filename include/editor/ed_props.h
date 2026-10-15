/*
** ALEXNEX PROJECT, 2026
** editor/ed_props.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_PROPS_H
    #define EDITOR_ED_PROPS_H

    #include "editor/ed_select.h"

/*
** What the properties panel shows and changes on the selection (FEATURES
** 11.4, 11.8). An object has a property or not: a block has no kind, a start
** position no size. Pure, tested alone.
*/
typedef enum ed_prop {
    ED_PROP_TURN,             /* degrees, 15 at a time                        */
    ED_PROP_W,                /* grid units of 50 px, one at a time           */
    ED_PROP_H,
    ED_PROP_KIND,             /* a portal's mode, a colour, up or down, on or off */
    ED_PROP_GRAVITY,          /* a start position's                           */
    ED_PROP_SPEED,
    ED_PROP_SIZE,             /* a start position's: normal or mini (10.4)    */
    ED_PROP_COUNT
} ed_prop_t;

const char *ed_prop_name(ed_prop_t p);
/* Something selected has it. */
bool ed_prop_applies(const ed_level_t *lv, ed_prop_t p);
/* Its value, or "mixed" when the selected objects that have it differ. */
void ed_prop_text(const ed_level_t *lv, ed_prop_t p, char *out, size_t size);
/* The next (dir 1) or previous (-1) value on each one that has it: one
** command. false when nothing changed. */
bool ed_prop_step(ed_level_t *lv, ed_history_t *h, ed_prop_t p, int dir);

/*
** Turns the selection by `deg`. Several objects also turn like one piece,
** around a grid corner or a cell's center by their middle: a piece on the
** grid stays on it, and keeps its shape. The next turn of the same piece,
** with nothing changed between, goes around the same point, so the turn
** back, or four quarters, put it back exactly. Start positions move with
** the piece but have nothing to turn.
*/
bool ed_rotate(ed_level_t *lv, ed_history_t *h, double deg, double grid);

#endif
