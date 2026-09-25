/*
** ALEXNEX PROJECT, 2026
** sim/camera.c
** File description:
** the camera: a smoothed follow, locked on a corridor while inside one (3.5)
*/

#include "sim/internal.h"

void camera_follow(camera_t *c, const player_t *p, const ship_bounds_t *b)
{
    double sy = p->pos.y - c->pos.y;         /* the player's screen y */
    double target = c->pos.y;

    if (b->active) {
        c->pos.y = b->top - (VIEW_HEIGHT - (b->bottom - b->top)) / 2.0;
        return;                              /* the corridor, centered on screen */
    }
    if (sy < CAM_TOP_MARGIN)
        target = p->pos.y - CAM_TOP_MARGIN;
    else if (sy > CAM_BOTTOM_MARGIN)
        target = p->pos.y - CAM_BOTTOM_MARGIN;
    else if (p->grounded && c->pos.y < 0.0)
        target = 0.0;                        /* back to the ground view */
    if (target > 0.0)
        target = 0.0;                        /* never below the ground view */
    c->pos.y += (target - c->pos.y) * CAM_LERP;
}
