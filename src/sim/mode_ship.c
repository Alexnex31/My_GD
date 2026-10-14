/*
** ALEXNEX PROJECT, 2026
** sim/mode_ship.c
** File description:
** the ship: thrust while the button is down (3.4, FEATURES 6.1)
*/

#include <math.h>

#include "sim/internal.h"
#include "sim/modes.h"

/* Thrust adds up to the ship's own limit; speed from elsewhere is never cut. */
void ship_input(player_t *p, input_t in)
{
    const mode_ops_t *m = player_mode(p);
    bool down = in.held || in.pressed;

    if (down && p->vy < m->max_fall)        /* its cap, up as down */
        p->vy = fmin(p->vy + m->thrust, m->max_fall + m->gravity);
}

/* Its nose follows its path. */
void ship_rotation(player_t *p)
{
    p->rotation = (float)(-atan2(p->vy, p->vx) * 180.0 / M_PI
        * p->gravity_dir);
}
