/*
** ALEXNEX PROJECT, 2026
** fx/trail.h
** File description:
** header file for my_gd project
*/

#ifndef FX_TRAIL_H
    #define FX_TRAIL_H

    #include <stdbool.h>
    #include <stddef.h>
    #include "sim/sim_types.h"

    #define TRAIL_CAP 256     /* corners kept: minutes of play (FEATURES 8.5) */

/*
** The wave's trail: only the corners of its path, in world coordinates,
** oldest first. Between two corners the path is a straight line, and the
** last line runs to wherever the player is drawn. It reads the simulation
** and never changes it.
*/
typedef struct trail {
    vec2_t pts[TRAIL_CAP];
    size_t n;
    int dir;                  /* the last tick's way: up 1, down -1, level 0  */
    bool was_wave;            /* the player was a wave after the last tick    */
} trail_t;

void trail_reset(trail_t *t, const player_t *p);       /* a new attempt */
void trail_after_tick(trail_t *t, const player_t *p);  /* once per tick */
void trail_drop_left_of(trail_t *t, double x);         /* once per frame */

#endif
