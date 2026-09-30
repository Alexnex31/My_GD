/*
** ALEXNEX PROJECT, 2026
** sim/camera.c
** File description:
** the camera: a smoothed follow, locked on a corridor while inside one (3.5)
*/

#include <math.h>

#include "sim/internal.h"

/* Where the camera would already have settled for this player (3.5, 7.2). */
/*
** Where the follow below converges to from the ground view: unchanged while
** the player is inside the zone or under it (the clamp holds it), pulled up
** when the player is above it (3.5, 7.2).
*/
double camera_rest_y(const player_t *p, const ship_bounds_t *b)
{
    if (b->active)
        return b->top - (VIEW_HEIGHT - (b->bottom - b->top)) / 2.0;
    if (p->pos.y < CAM_ZONE_TOP)
        return p->pos.y - CAM_ZONE_TOP;
    return 0.0;
}

/* Close enough to the corridor that the rest of the move isn't visible. */
static bool at_rest(double from, double to)
{
    return fabs(to - from) < CAM_SNAP_EPSILON;
}

/*
** Where the camera wants to be: nowhere new while the player is inside the
** zone, otherwise just enough to bring it back to the edge it left (3.5).
*/
static double follow_target(const player_t *p, const camera_t *c)
{
    double screen_y = p->pos.y - c->pos.y;

    if (screen_y < CAM_ZONE_TOP)
        return p->pos.y - CAM_ZONE_TOP;      /* it went above: the camera rises */
    if (screen_y > CAM_ZONE_BOTTOM)
        return p->pos.y - CAM_ZONE_BOTTOM;   /* it went below: the camera drops */
    return c->pos.y;                         /* inside: nothing to follow */
}

/*
** Outside a corridor there is nothing to snap to: the camera follows the
** player, strictly on x (the game layer) and eased on y, and only once it
** leaves the zone. No return to a "ground view": that rule is what made the
** camera drop on its own once the player landed.
*/
void camera_follow(camera_t *c, const player_t *p, const ship_bounds_t *b)
{
    double target = 0.0;

    if (b->active) {                         /* the corridor, centered on screen */
        target = b->top - (VIEW_HEIGHT - (b->bottom - b->top)) / 2.0;
        c->pos.y = at_rest(c->pos.y, target) ? target
            : c->pos.y + (target - c->pos.y) * CAM_LERP;
        return;                              /* eased onto it, never cut (3.5) */
    }
    target = follow_target(p, c);
    if (target > 0.0)
        target = 0.0;                        /* never shows below the ground */
    c->pos.y += (target - c->pos.y) * CAM_LERP;
}
