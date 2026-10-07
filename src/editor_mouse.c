/*
** ALEXNEX PROJECT, 2026
** editor_mouse
** File description:
** the editor's mouse: place, delete, select, drag, pan (FEATURES 11.5)
*/

#include "editor.h"

static bool shift_held(void)
{
    return sfKeyboard_isKeyPressed(sfKeyLShift)
        || sfKeyboard_isKeyPressed(sfKeyRShift);
}

rect_t editor_box(editor_t *ed, gd_t *gd)
{
    vec2_t at = editor_mouse_world(ed, gd);

    return (rect_t){fmin(at.x, ed->press_at.x), fmin(at.y, ed->press_at.y),
        fabs(at.x - ed->press_at.x), fabs(at.y - ed->press_at.y)};
}

/* The palette is a column of rows, 40 px each, under the toolbar. */
static void palette_click(editor_t *ed, sfVector2f at)
{
    int row = (int)((at.y - ED_TOOLBAR_H - 10.0f) / 40.0f);

    if (at.x < ED_PALETTE_W && at.y >= ED_TOOLBAR_H + 10.0f && row >= 0
        && row < ed->nb_entries) {
        ed->entry = row;
        ed->tool = ED_TOOL_PLACE;
    }
}

/* What a held button does at the mouse, each time it or the world moves. */
static void follow(editor_t *ed, gd_t *gd)
{
    vec2_t at = editor_mouse_world(ed, gd);
    int hit;

    if (ed->mouse == ED_DRAGGING)
        ed_grab_move(&ed->lv, &ed->grab,
            ed_snap_delta(at.x - ed->press_at.x, ed->grid),
            ed_snap_delta(at.y - ed->press_at.y, ed->grid));
    if (ed->mouse == ED_DRAGGING)
        ed->measured = false;
    if (!editor_mouse_on_canvas(gd))
        return;
    if (ed->mouse == ED_PAINTING && ed_do_place(&ed->lv, &ed->hist,
        &ed->entries[ed->entry], at, ed->grid))
        ed->measured = false;
    if (ed->mouse != ED_ERASING)
        return;
    hit = ed_hit(&ed->lv, at);
    if (hit >= 0 && ed_do_remove(&ed->lv, &ed->hist, hit))
        ed->measured = false;
}

/* The left button, by tool: paint, or start on an object or on nothing. */
static void left_press(editor_t *ed, gd_t *gd)
{
    ed->press_at = editor_mouse_world(ed, gd);
    ed->press_id = ed_hit(&ed->lv, ed->press_at);
    if (ed->tool == ED_TOOL_PLACE) {
        ed_select_none(&ed->lv);             /* placing starts from nothing */
        ed->mouse = ED_PAINTING;
    } else
        ed->mouse = ed->press_id >= 0 ? ED_DRAG_PENDING : ED_BOXING;
}

void editor_mouse_press(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfVector2i px = {ev->mouseButton.x, ev->mouseButton.y};
    sfMouseButton b = ev->mouseButton.button;

    if (!editor_mouse_on_canvas(gd)) {
        if (b == sfMouseLeft)
            palette_click(ed, sfRenderWindow_mapPixelToCoords(gd->w, px,
                gd->ui_view));
        return;
    }
    ed_gesture(&ed->hist);
    ed->press_px = px;
    if (b == sfMouseMiddle
        || (b == sfMouseLeft && sfKeyboard_isKeyPressed(sfKeySpace)))
        ed->mouse = ED_PANNING;
    else if (b == sfMouseLeft)
        left_press(ed, gd);
    else if (b == sfMouseRight)
        ed->mouse = ED_ERASING;
    follow(ed, gd);
}

/* A pan moves the world by what the mouse moved, at the zoom's scale. */
static void pan(editor_t *ed, gd_t *gd, sfVector2i px)
{
    sfVector2f a = sfRenderWindow_mapPixelToCoords(gd->w, ed->press_px,
        ed->view);
    sfVector2f b = sfRenderWindow_mapPixelToCoords(gd->w, px, ed->view);

    ed->cam.x += a.x - b.x;
    ed->cam.y += a.y - b.y;
    ed->press_px = px;
    editor_apply_camera(ed, gd);
}

/*
** A press on an object becomes a drag once the mouse has really moved. An
** object that wasn't selected is then the only one dragged, or joins the
** selection with Shift.
*/
static void start_drag(editor_t *ed, sfVector2i px)
{
    const ed_object_t *o = ed_find(&ed->lv, ed->press_id);

    if (abs(px.x - ed->press_px.x) <= ED_DRAG_PX
        && abs(px.y - ed->press_px.y) <= ED_DRAG_PX)
        return;
    if (o == NULL) {
        ed->mouse = ED_IDLE;
        return;
    }
    if (!o->selected)
        ed_select_click(&ed->lv, ed->press_id, shift_held());
    ed_grab(&ed->lv, &ed->grab);
    ed->mouse = ED_DRAGGING;
}

/* `ev` is NULL when it is the world that moved under a still mouse. */
void editor_mouse_move(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfVector2i px = sfMouse_getPositionRenderWindow(gd->w);

    if (ev != NULL)
        px = (sfVector2i){ev->mouseMove.x, ev->mouseMove.y};
    if (ed->mouse == ED_PANNING)
        pan(ed, gd, px);
    if (ed->mouse == ED_DRAG_PENDING)
        start_drag(ed, px);
    follow(ed, gd);
}

void editor_mouse_release(editor_t *ed, gd_t *gd)
{
    if (ed->mouse == ED_DRAG_PENDING)
        ed_select_click(&ed->lv, ed->press_id, shift_held());
    if (ed->mouse == ED_DRAGGING)
        ed_grab_drop(&ed->lv, &ed->hist, &ed->grab);
    if (ed->mouse == ED_BOXING)
        ed_select_box(&ed->lv, editor_box(ed, gd), shift_held());
    ed->mouse = ED_IDLE;
    ed->measured = false;
}

/* Escape, or the window losing the mouse: the drag never happened. */
bool editor_mouse_cancel(editor_t *ed)
{
    if (ed->mouse != ED_DRAGGING && ed->mouse != ED_BOXING
        && ed->mouse != ED_DRAG_PENDING)
        return false;
    if (ed->mouse == ED_DRAGGING)
        ed_grab_cancel(&ed->lv, &ed->grab);
    ed->mouse = ED_IDLE;
    ed->measured = false;
    return true;
}
