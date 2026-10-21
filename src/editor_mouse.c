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

static rect_t box_to(const editor_t *ed, vec2_t at)
{
    return (rect_t){fmin(at.x, ed->press_at.x), fmin(at.y, ed->press_at.y),
        fabs(at.x - ed->press_at.x), fabs(at.y - ed->press_at.y)};
}

rect_t editor_box(editor_t *ed, gd_t *gd)
{
    return box_to(ed, editor_mouse_world(ed, gd));
}

/* The palette is a column of rows under the toolbar. */
static void palette_click(editor_t *ed, sfVector2f at)
{
    int row = (int)((at.y - ED_TOOLBAR_H - 10.0f) / ED_PALETTE_ROW);

    if (at.x < ED_PALETTE_W && at.y >= ED_TOOLBAR_H + 10.0f && row >= 0
        && row < ed->nb_entries) {
        ed->entry = row;
        ed->tool = ED_TOOL_PLACE;
    }
}

/* Painting or erasing at one point of a stroke. */
static void stroke_point(editor_t *ed, vec2_t at)
{
    int hit;

    if (ed->mouse == ED_PAINTING)
        ed_do_place(&ed->lv, &ed->hist, &ed->entries[ed->entry], at,
            ed->grid);
    if (ed->mouse != ED_ERASING)
        return;
    hit = ed_hit(&ed->lv, at);
    if (hit >= 0)
        ed_do_remove(&ed->lv, &ed->hist, hit);
}

/*
** From the stroke's last point to this one, half a cell at a time: a fast
** swipe between two mouse samples still paints, or erases, every cell it
** crossed. Not under the panel: what it would do there can't be seen.
*/
static void stroke(editor_t *ed, gd_t *gd, sfVector2i px, vec2_t at)
{
    sfVector2f ui = sfRenderWindow_mapPixelToCoords(gd->w, px, gd->ui_view);
    vec2_t from = ed->stroke_on ? ed->stroke_at : at;
    double len = hypot(at.x - from.x, at.y - from.y);
    int steps = (int)ceil(len / (ed->grid / 2.0));

    if (!editor_px_on_canvas(gd, px) || editor_panel_holds(ed, ui)) {
        ed->stroke_on = false;
        return;
    }
    for (int i = 1; i <= steps; i++)
        stroke_point(ed, (vec2_t){from.x + (at.x - from.x) * i / steps,
            from.y + (at.y - from.y) * i / steps});
    if (steps == 0)
        stroke_point(ed, at);
    ed->stroke_at = at;
    ed->stroke_on = true;
}

/* What a held button does at the mouse, each time it or the world moves. */
static void follow(editor_t *ed, gd_t *gd, sfVector2i px)
{
    vec2_t at = editor_px_world(ed, gd, px);
    vec2_t d = {ed_snap_delta(at.x - ed->press_at.x, ed->grid),
        ed_snap_delta(at.y - ed->press_at.y, ed->grid)};

    if (ed->mouse == ED_DRAGGING && (d.x != ed->drag_delta.x
        || d.y != ed->drag_delta.y)) {
        ed_grab_move(&ed->lv, &ed->grab, d.x, d.y);
        ed->drag_delta = d;
        ed->measured = false;                /* moved: no edit until dropped */
    }
    if (ed->mouse == ED_PAINTING || ed->mouse == ED_ERASING)
        stroke(ed, gd, px, at);
}

/* The left button, by tool: paint, or start on an object or on nothing. */
static void left_press(editor_t *ed)
{
    if (ed->tool == ED_TOOL_PLACE) {
        ed_select_none(&ed->lv);             /* placing starts from nothing */
        ed->mouse = ED_PAINTING;
        return;
    }
    ed->press_id = ed_hit(&ed->lv, ed->press_at);
    ed->mouse = ed->press_id >= 0 ? ED_DRAG_PENDING : ED_BOXING;
}

void editor_mouse_press(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfVector2i px = {ev->mouseButton.x, ev->mouseButton.y};
    sfMouseButton b = ev->mouseButton.button;
    sfVector2f ui = sfRenderWindow_mapPixelToCoords(gd->w, px, gd->ui_view);

    ed_gesture(&ed->hist);
    if (editor_settings_button_holds(ui)) {  /* on the toolbar */
        if (b == sfMouseLeft)
            editor_settings_open(ed);
        return;
    }
    if (editor_panel_holds(ed, ui)) {        /* it is over the canvas */
        if (b == sfMouseLeft)
            editor_panel_click(ed, gd, ui);
        ed->measured = false;
        return;
    }
    if (editor_status_holds(ui)) {           /* so is the strip */
        if (b == sfMouseLeft)
            editor_next_issue(ed);
        return;
    }
    if (!editor_px_on_canvas(gd, px)) {
        if (b == sfMouseLeft)
            palette_click(ed, ui);
        return;
    }
    ed->press_px = px;
    ed->press_at = editor_px_world(ed, gd, px);
    ed->press_button = b;
    ed->stroke_on = false;
    if (b == sfMouseMiddle
        || (b == sfMouseLeft && sfKeyboard_isKeyPressed(sfKeySpace)))
        ed->mouse = ED_PANNING;
    else if (b == sfMouseLeft)
        left_press(ed);
    else if (b == sfMouseRight)
        ed->mouse = ED_ERASING;
    follow(ed, gd, px);
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
    ed->drag_delta = (vec2_t){0.0, 0.0};
    ed->mouse = ED_DRAGGING;
}

/*
** Each move event at its own pixel: several in one frame are each a point.
** `ev` is NULL when it is the world that moved under a still mouse: a
** stroke then starts again from there, never drawn across the jump.
*/
void editor_mouse_move(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfVector2i px = sfMouse_getPositionRenderWindow(gd->w);

    if (ev != NULL)
        px = (sfVector2i){ev->mouseMove.x, ev->mouseMove.y};
    else
        ed->stroke_on = false;
    if (ed->mouse == ED_PANNING)
        pan(ed, gd, px);
    if (ed->mouse == ED_DRAG_PENDING)
        start_drag(ed, px);
    follow(ed, gd, px);
}

/* Only the button that began it ends it: a stray click changes nothing. */
void editor_mouse_release(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    if (ed->mouse == ED_IDLE || ev->mouseButton.button != ed->press_button)
        return;
    if (ed->mouse == ED_DRAG_PENDING)
        ed_select_click(&ed->lv, ed->press_id, shift_held());
    if (ed->mouse == ED_DRAGGING)
        ed_grab_drop(&ed->lv, &ed->hist, &ed->grab);
    if (ed->mouse == ED_BOXING)
        ed_select_box(&ed->lv, box_to(ed, editor_px_world(ed, gd,
            (sfVector2i){ev->mouseButton.x, ev->mouseButton.y})),
            shift_held());
    ed->mouse = ED_IDLE;
}

/*
** Escape, or the window losing the mouse: the drag never happened. A stroke
** of painting or erasing ends with what it did (one undo step), and a pan
** where it is: the button's release would never come to end them.
*/
bool editor_mouse_cancel(editor_t *ed)
{
    if (ed->mouse == ED_IDLE)
        return false;
    if (ed->mouse == ED_DRAGGING) {
        ed_grab_cancel(&ed->lv, &ed->grab);
        ed->measured = false;                /* back where they were */
    }
    ed->mouse = ED_IDLE;
    ed_gesture(&ed->hist);
    return true;
}
