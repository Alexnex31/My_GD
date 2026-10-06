/*
** ALEXNEX PROJECT, 2026
** sim/mode_ufo.c
** File description:
** the UFO: one hop per press, on the ground or in the air (FEATURES 7)
*/

#include <math.h>

#include "sim/internal.h"

/*
** A hop sets the rise speed and never adds to it, so every hop is the same
** whatever the UFO was doing. Holding does nothing: only the press counts.
*/
void ufo_input(player_t *p, input_t in)
{
    if (!in.pressed)
        return;
    p->vy = PER_TICK(UFO_JUMP_V);
    p->can_jump = false;
    p->grounded = false;
    p->hold = HOLD_USED;                     /* this hold activates no orb now */
}

/* It leans the way it goes, a third of what the ship does. */
void ufo_rotation(player_t *p)
{
    p->rotation = (float)(-atan2(p->vy, p->vx) * 180.0 / M_PI * UFO_TILT
        * p->gravity_dir);
}
