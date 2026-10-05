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
    bool down = in.held || in.pressed;

    if (down && p->vy < PER_TICK(SHIP_MAX_VY))
        p->vy = fmin(p->vy + PER_TICK2(SHIP_THRUST),
            PER_TICK(SHIP_MAX_VY) + MODES[MODE_SHIP].gravity);
}

/* Its nose follows its path. */
void ship_rotation(player_t *p)
{
    p->rotation = (float)(-atan2(p->vy, p->vx) * 180.0 / M_PI
        * p->gravity_dir);
}
