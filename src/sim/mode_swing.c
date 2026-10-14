/*
** ALEXNEX PROJECT, 2026
** sim/mode_swing.c
** File description:
** the swing: each press turns its gravity round (FEATURES 10.5)
*/

#include "sim/internal.h"

/*
** A press flips the gravity, on the ground or in the air, and that is all:
** no push, the motion is kept (3.4), so the swing curves round to its new
** side instead of jumping there. Holding does nothing: only the press counts,
** and it uses the hold, so a held button takes no orb.
*/
void swing_input(player_t *p, input_t in)
{
    if (!in.pressed)
        return;
    player_flip_gravity(p);
    p->hold = HOLD_USED;                     /* this hold activates no orb now */
}

