/*
** ALEXNEX PROJECT, 2026
** sim/mode_spider.c
** File description:
** the spider: a press takes it straight to the surface opposite (FEATURES 10.5)
*/

#include "sim/internal.h"

/*
** A press while it can jump turns its gravity round and asks this tick's
** move to carry it across, straight to the first thing above (move.c): the
** mode never moves the player itself (6.1). A hold flips once, as the ball's
** does: kept down, it does nothing more, on this landing or the next. A hold
** that is still fresh flips on the first surface it can jump from (3.4).
*/
void spider_input(player_t *p, input_t in)
{
    (void)in;                                /* the hold already knows (3.4) */
    if (p->hold != HOLD_FRESH || !p->can_jump)
        return;
    player_flip_gravity(p);
    p->teleport = true;
    p->hold = HOLD_USED;
}
