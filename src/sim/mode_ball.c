/*
** ALEXNEX PROJECT, 2026
** sim/mode_ball.c
** File description:
** the ball: a cube that flips its gravity instead of jumping (FEATURES 9)
*/

#include <math.h>

#include "sim/internal.h"

/*
** A click flips the gravity, where the cube has its jump, with a small push
** off the surface it leaves: the ball jumps away from it, and the flipped
** gravity takes it to the other one. The push sets a speed, it never slows
** a ball already going faster that way. The flip uses the hold up: kept down, it does nothing more, on
** this landing or the next. A hold that is still fresh (pressed in the air,
** or carried into the attempt) flips on the first surface it reaches (3.4).
*/
void ball_input(player_t *p, input_t in)
{
    (void)in;                                /* the hold already knows (3.4) */
    if (p->hold != HOLD_FRESH || !p->can_jump)
        return;
    player_flip_gravity(p);
    p->vy = fmin(p->vy, -PER_TICK(BALL_FLIP_V));   /* toward its new floor */
    p->hold = HOLD_USED;
}

/*
** It rolls as it goes, faster at higher speeds, the other way round on a
** ceiling. Slower than a circle that never slides would: see BALL_SPIN.
*/
void ball_rotation(player_t *p)
{
    double turn = PER_TICK(BALL_SPIN) * p->vx / PER_TICK(SCROLL_SPEED)
        * p->gravity_dir;

    p->rotation = (float)fmod(p->rotation + turn, 360.0);
}
