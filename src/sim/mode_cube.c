/*
** ALEXNEX PROJECT, 2026
** sim/mode_cube.c
** File description:
** the cube: a jump off whatever it stands on (3.4, FEATURES 6.1)
*/

#include <math.h>

#include "sim/internal.h"

void cube_input(player_t *p, input_t in)
{
    bool down = in.held || in.pressed;       /* a tap inside one tick still counts */

    if (!down || !p->can_jump)
        return;
    p->vy = PER_TICK(CUBE_JUMP_V);           /* an impulse: it sets the rise speed */
    p->can_jump = false;
    p->grounded = false;
    p->hold = HOLD_USED;                     /* this hold activates no orb now */
}

/* It spins in the air and lies flat on what it lands on, a quarter turn at most. */
void cube_rotation(player_t *p)
{
    float surface = (float)(atan2(p->support_normal.x,
        -p->support_normal.y * p->gravity_dir) * 180.0 / M_PI);

    if (!p->grounded) {
        p->rotation += (float)PER_TICK(CUBE_SPIN) * (float)p->gravity_dir;
        return;
    }
    p->rotation = surface + roundf((p->rotation - surface) / 90.0f) * 90.0f;
}
