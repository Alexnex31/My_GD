/*
** ALEXNEX PROJECT, 2026
** sim/player.c
** File description:
** the forces acting on the player: input, gravity, gravity flips (3.4)
*/

#include <math.h>

#include "sim/internal.h"
#include "sim/modes.h"

/* A hold is fresh from its press and stays fresh until something uses it. */
void player_update_hold(player_t *p, input_t in)
{
    if (in.pressed)
        p->hold = HOLD_FRESH;
    else if (!in.held)
        p->hold = HOLD_NONE;
}

void player_apply_input(player_t *p, input_t in)
{
    bool down = in.held || in.pressed;       /* a sub-frame tap still counts */

    if (p->mode == MODE_CUBE && down && p->can_jump) {
        p->vy = PER_TICK(CUBE_JUMP_V);       /* an impulse: it sets the rise speed */
        p->can_jump = false;
        p->grounded = false;
        p->hold = HOLD_USED;                 /* this hold activates no orb now */
    }
    if (p->mode == MODE_SHIP && down && p->vy < PER_TICK(SHIP_MAX_VY))
        p->vy = fmin(p->vy + PER_TICK2(SHIP_THRUST),
            PER_TICK(SHIP_MAX_VY) + MODES[MODE_SHIP].gravity);
}

/*
** Gravity accelerates toward the floor up to the mode's cap. A player that was
** supported last tick gets one tick of gravity past its surface's own rise
** speed, so a slope descending faster than the cap still holds it (3.4).
*/
void player_apply_gravity(player_t *p)
{
    const mode_ops_t *m = &MODES[p->mode];
    double limit = -m->max_fall;

    if (p->grounded && p->surface_rise - m->gravity < limit)
        limit = p->surface_rise - m->gravity;
    if (p->vy > limit)
        p->vy = fmax(p->vy - m->gravity, limit);
}

/* Flip the constant gravity and nothing else: the motion is unchanged (3.4). */
void player_flip_gravity(player_t *p)
{
    p->gravity_dir = -p->gravity_dir;
    p->vy = -p->vy;
    p->grounded = false;
    p->can_jump = false;
}

void player_update_rotation(player_t *p)
{
    (void)p;                                 /* the icon's spin is drawing (9.4) */
}
