/*
** ALEXNEX PROJECT, 2026
** trail_draw
** File description:
** the wave's trail on screen: one triangle strip (FEATURES 8.5)
*/

#include <math.h>

#include "mygd.h"

#define TRAIL_WIDTH 14.0f     /* px, measured vertically                    */
#define TRAIL_FAINT 60.0f     /* alpha where the trail leaves the screen    */

/* Opaque at the player, fainter the further behind: the left edge is faint. */
static sfColor fade(vec2_t at, vec2_t head)
{
    float behind = (float)((head.x - at.x) / PLAYER_SCREEN_X);

    behind = fminf(fmaxf(behind, 0.0f), 1.0f);
    return (sfColor){255, 255, 255,
        (sfUint8)(255.0f - (255.0f - TRAIL_FAINT) * behind)};
}

/*
** Each corner becomes two vertices, one above and one below it, so the
** thickness is constant vertically: a 45 degree line looks a little thinner
** than a level one. The strip ends at `head`, where the player is drawn.
*/
void render_trail(gd_t *gd, level_t *lv, vec2_t head, float cam_x)
{
    const trail_t *t = &lv->trail;

    trail_drop_left_of(&lv->trail, cam_x);
    if (t->n == 0)
        return;
    sfVertexArray_clear(lv->trail_va);
    for (size_t i = 0; i <= t->n; i++) {
        vec2_t p = i < t->n ? t->pts[i] : head;
        sfColor c = fade(p, head);

        sfVertexArray_append(lv->trail_va, (sfVertex){{(float)p.x,
            (float)p.y - TRAIL_WIDTH / 2.0f}, c, {0.0f, 0.0f}});
        sfVertexArray_append(lv->trail_va, (sfVertex){{(float)p.x,
            (float)p.y + TRAIL_WIDTH / 2.0f}, c, {0.0f, 0.0f}});
    }
    draw_vertex_array(gd, lv->trail_va, NULL);
}
