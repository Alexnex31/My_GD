/*
** ALEXNEX PROJECT, 2026
** sim/mode_wave.c
** File description:
** the wave: 45 degrees up while the button is down, down otherwise (FEATURES 8)
*/

#include <math.h>

#include "sim/internal.h"

/*
** Its rise speed is its horizontal speed, one way or the other, set again
** every tick: no inertia. It is a real velocity all the same, so the mode a
** portal changes it into starts with it (FEATURES 6.6). The hold is never
** used: flying uses nothing (3.4).
*/
void wave_input(player_t *p, input_t in)
{
    bool down = in.held || in.pressed;       /* a tap inside one tick still counts */

    p->vy = down ? p->vx : -p->vx;
}

void wave_forces(player_t *p)
{
    (void)p;                                 /* no gravity: nothing but the input */
}

/*
** Its nose follows what it really did this tick: 45 degrees in the air, flat
** along a surface it slides on, where its speed still points into it.
*/
void wave_rotation(player_t *p)
{
    p->rotation = (float)(-atan2(p->prev_pos.y - p->pos.y,
        p->pos.x - p->prev_pos.x) * 180.0 / M_PI);
}
