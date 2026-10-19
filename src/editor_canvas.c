/*
** ALEXNEX PROJECT, 2026
** editor_canvas
** File description:
** the editor's canvas: its view, the mouse in the world, zoom (FEATURES 11.3)
*/

#include "editor.h"

/*
** The canvas is a rectangle of the UI's 1920x1080, so its viewport is that
** rectangle inside the letterboxed one (11.2, PLAN 9.7).
*/
void editor_apply_camera(editor_t *ed, gd_t *gd)
{
    sfFloatRect vp = sfView_getViewport(gd->ui_view);

    sfView_setViewport(ed->view, (sfFloatRect){
        vp.left + vp.width * ED_PALETTE_W / VIEW_W,
        vp.top + vp.height * ED_TOOLBAR_H / VIEW_H,
        vp.width * ED_CANVAS_W / VIEW_W, vp.height * ED_CANVAS_H / VIEW_H});
    sfView_setSize(ed->view, (sfVector2f){ED_CANVAS_W * ed->zoom,
        ED_CANVAS_H * ed->zoom});
    sfView_setCenter(ed->view, (sfVector2f){(float)ed->cam.x,
        (float)ed->cam.y});
}

/* A window pixel, an event's, in the world. */
vec2_t editor_px_world(editor_t *ed, gd_t *gd, sfVector2i px)
{
    sfVector2f at = sfRenderWindow_mapPixelToCoords(gd->w, px, ed->view);

    return (vec2_t){at.x, at.y};
}

vec2_t editor_mouse_world(editor_t *ed, gd_t *gd)
{
    return editor_px_world(ed, gd, sfMouse_getPositionRenderWindow(gd->w));
}

bool editor_px_on_canvas(gd_t *gd, sfVector2i px)
{
    sfVector2f at = sfRenderWindow_mapPixelToCoords(gd->w, px, gd->ui_view);

    return at.x >= ED_PALETTE_W && at.x < VIEW_W && at.y >= ED_TOOLBAR_H
        && at.y < ED_STATUS_Y;
}

bool editor_mouse_on_canvas(gd_t *gd)
{
    return editor_px_on_canvas(gd, sfMouse_getPositionRenderWindow(gd->w));
}

/*
** Around the mouse: the world point under the cursor stays under it (11.3).
** Whole steps, so the limits are steps too and 100% is always reached again.
*/
void editor_zoom_at(editor_t *ed, gd_t *gd, int steps)
{
    vec2_t before = editor_mouse_world(ed, gd);
    vec2_t after;
    int n = ed->zoom_step + steps;

    ed->zoom_step = n < -ED_ZOOM_STEPS ? -ED_ZOOM_STEPS
        : (n > ED_ZOOM_STEPS ? ED_ZOOM_STEPS : n);
    ed->zoom = powf(ED_ZOOM_STEP, (float)ed->zoom_step);
    editor_apply_camera(ed, gd);
    after = editor_mouse_world(ed, gd);
    ed->cam.x += before.x - after.x;
    ed->cam.y += before.y - after.y;
    editor_apply_camera(ed, gd);
}
