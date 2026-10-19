/*
** ALEXNEX PROJECT, 2026
** fx/draw_order.h
** File description:
** header file for my_gd project
*/

#ifndef FX_DRAW_ORDER_H
    #define FX_DRAW_ORDER_H

    #include <stddef.h>
    #include <stdint.h>
    #include "sim/sim_types.h"

    #define DRAW_CHUNK_W 1024.0   /* the slices the view is culled by (9.2) */

/*
** A slice of the level along x, for what is in view (9.2): the objects
** whose drawn left edge is in it, by their place in the draw order.
*/
typedef struct draw_chunk {
    double left;                  /* the leftmost drawn x of its objects  */
    double right;                 /* the rightmost: a sprite can stick out */
    size_t *ranks;
    size_t count;
} draw_chunk_t;

/*
** The draw order (FEATURES 11.15): by Z layer, then Z order, then the order
** objects were placed in (their line), the later on top, whatever the array
** order. Built once per level; each frame gathers the objects in view in
** that order, those behind the player first. Pure: the game layer draws.
*/
typedef struct draw_index {
    size_t *by_rank;              /* the object drawn k-th, of all of them */
    draw_chunk_t *chunks;
    size_t nb_chunks;
    size_t *visible;              /* this frame's: ranks, sorted           */
    size_t nb_visible;
    size_t nb_behind;             /* the first ones: B5 to B1              */
} draw_index_t;

void draw_index_build(draw_index_t *d, const object_t *objs, size_t n);
/* The objects whose chunk meets [left, right], in the draw order. */
void draw_index_gather(draw_index_t *d, const object_t *objs, double left,
    double right);
void draw_index_free(draw_index_t *d);

/*
** The spent flash (PLAN 9.2): which objects were used since the frame that
** last looked. `seen` is that frame's copy of the spent bits.
** flash_forget: what is spent now is no news (a reset, a checkpoint: the
** objects behind a start position are spent and never fired).
** flash_fresh: the newly spent ones into out (max of them), seen updated.
*/
void flash_forget(uint64_t *seen, const run_state_t *st);
size_t flash_fresh(uint64_t *seen, const run_state_t *st, size_t *out,
    size_t max);

#endif
