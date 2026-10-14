/*
** ALEXNEX PROJECT, 2026
** sim/mode_robot.c
** File description:
** the robot: a jump that rises for as long as the button is held (FEATURES 10.5)
*/

#include <math.h>

#include "sim/internal.h"
#include "sim/modes.h"

/*
** From takeoff the robot rises at a steady speed, 45 degrees at the normal
** scroll speed, for as long as the button stays down and for
** its row's rise_ticks at most; then it falls like anything else. A tap is the
** lowest jump, a full hold the highest. Anything else that takes over its
** speed (a pad, an orb, a flip, another mode, a surface it hits its head
** on) ends the rise.
*/
void robot_input(player_t *p, input_t in)
{
    bool down = in.held || in.pressed;       /* a tap inside one tick still counts */

    if (down && p->can_jump) {
        p->boost_ticks = player_mode(p)->rise_ticks;
        p->can_jump = false;
        p->grounded = false;
        p->hold = HOLD_USED;                 /* this hold activates no orb now */
    } else if (!down) {
        p->boost_ticks = 0;
    }
    if (p->boost_ticks > 0)
        p->vy = player_mode(p)->jump_v;
}

/* No gravity on a tick it rises on: that is the whole of its jump. */
void robot_forces(player_t *p)
{
    if (p->boost_ticks > 0) {
        p->boost_ticks -= 1;
        return;
    }
    player_apply_gravity(p);
}

/* It doesn't spin: it stands on what carries it, and stays upright in the air. */
void robot_rotation(player_t *p)
{
    if (p->grounded)
        p->rotation = (float)(atan2(p->support_normal.x,
            -p->support_normal.y * p->gravity_dir) * 180.0 / M_PI);
}
