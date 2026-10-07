/*
** ALEXNEX PROJECT, 2026
** editor_scene
** File description:
** the editor: its events, placing, deleting, saving (FEATURES 11, stages E1 and E2)
*/

#include "editor.h"

static void notice(editor_t *ed, const char *what)
{
    snprintf(ed->notice, sizeof(ed->notice), "%s", what);
    ed->notice_until_ms = ui_now_ms() + 3000;
}

editor_t *editor_open(gd_t *gd, const char *id, const char *new_name)
{
    editor_t *ed = sim_xcalloc(1, sizeof(editor_t));

    snprintf(ed->path, sizeof(ed->path), "levels/%s.gd", id);
    if (new_name != NULL) {
        ed_level_new(&ed->lv, id);
        if (new_name[0] != '\0')
            snprintf(ed->lv.hdr.name, sizeof(ed->lv.hdr.name), "%s", new_name);
        ed->lv.dirty = true;                 /* it exists nowhere yet */
    } else if (ed_level_open(&ed->lv, ed->path) != 0) {
        free(ed);
        return NULL;
    }
    ed->zoom = 1.0f;
    ed->grid = 50.0;
    ed->cam = (vec2_t){ED_CANVAS_W / 2.0 - 100.0, GROUND_Y - 300.0};
    ed->view = sfView_create();
    ed->nb_entries = ed_entries(ed->entries, ED_MAX_ENTRIES);
    for (int l = 0; l < LAYER_COUNT; l++) {
        ed->objects[l] = sfVertexArray_create();
        sfVertexArray_setPrimitiveType(ed->objects[l], sfTriangles);
    }
    ed->lines = sfVertexArray_create();
    sfVertexArray_setPrimitiveType(ed->lines, sfLines);
    ed->box = sfRectangleShape_create();
    ed->text = sfText_create();
    sfText_setFont(ed->text, gd->main_font);
    ed->marker = sfSprite_create();
    return ed;
}

void editor_free(editor_t *ed)
{
    if (ed == NULL)
        return;
    for (int l = 0; l < LAYER_COUNT; l++)
        sfVertexArray_destroy(ed->objects[l]);
    sfVertexArray_destroy(ed->lines);
    sfRectangleShape_destroy(ed->box);
    sfText_destroy(ed->text);
    sfSprite_destroy(ed->marker);
    sfView_destroy(ed->view);
    ed_level_free(&ed->lv);
    free(ed);
}

static void save(editor_t *ed)
{
    if (ed_level_save(&ed->lv, ed->path) != 0)
        return notice(ed, "Could not save the level");
    ed->warned = false;
    notice(ed, "Saved");
}

/* Escape with unsaved changes warns once; the second one leaves (11.12). */
static void leave(editor_t *ed)
{
    if (ed->lv.dirty && !ed->warned) {
        ed->warned = true;
        return notice(ed, "Unsaved changes: Ctrl+S saves, Escape again leaves");
    }
    ed->leave = true;
}

/* What the buttons held on the canvas do, at the mouse, every event. */
static void paint(editor_t *ed, gd_t *gd)
{
    vec2_t at = editor_mouse_world(ed, gd);
    int hit;

    if (!editor_mouse_on_canvas(gd) || ed->panning)
        return;
    if (ed->placing && ed_place(&ed->lv, &ed->entries[ed->entry], at,
        ed->grid))
        ed->measured = false;
    if (!ed->erasing)
        return;
    hit = ed_hit(&ed->lv, at);
    if (hit >= 0 && ed_remove(&ed->lv, hit))
        ed->measured = false;
}

/* The palette is a column of rows, 40 px each, under the toolbar. */
static void palette_click(editor_t *ed, sfVector2f at)
{
    int row = (int)((at.y - ED_TOOLBAR_H - 10.0f) / 40.0f);

    if (at.x < ED_PALETTE_W && at.y >= ED_TOOLBAR_H + 10.0f && row >= 0
        && row < ed->nb_entries)
        ed->entry = row;
}

static void mouse_press(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfVector2i px = {ev->mouseButton.x, ev->mouseButton.y};
    bool space = sfKeyboard_isKeyPressed(sfKeySpace);

    if (!editor_mouse_on_canvas(gd)) {
        if (ev->mouseButton.button == sfMouseLeft)
            palette_click(ed, sfRenderWindow_mapPixelToCoords(gd->w, px,
                gd->ui_view));
        return;
    }
    if (ev->mouseButton.button == sfMouseMiddle
        || (ev->mouseButton.button == sfMouseLeft && space)) {
        ed->panning = true;
        ed->pan_from = px;
    } else if (ev->mouseButton.button == sfMouseLeft)
        ed->placing = true;
    else if (ev->mouseButton.button == sfMouseRight)
        ed->erasing = true;
    paint(ed, gd);
}

/* A pan moves the world by what the mouse moved, at the zoom's scale. */
static void mouse_move(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfVector2i px = {ev->mouseMove.x, ev->mouseMove.y};
    sfVector2f a;
    sfVector2f b;

    if (ed->panning) {
        a = sfRenderWindow_mapPixelToCoords(gd->w, ed->pan_from, ed->view);
        b = sfRenderWindow_mapPixelToCoords(gd->w, px, ed->view);
        ed->cam.x += a.x - b.x;
        ed->cam.y += a.y - b.y;
        ed->pan_from = px;
        editor_apply_camera(ed, gd);
    }
    paint(ed, gd);
}

/* The wheel scrolls along the level; with Ctrl it zooms (11.3). */
static void wheel(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    bool ctrl = sfKeyboard_isKeyPressed(sfKeyLControl)
        || sfKeyboard_isKeyPressed(sfKeyRControl);

    if (!editor_mouse_on_canvas(gd))
        return;
    if (ctrl)
        return editor_zoom_at(ed, gd,
            ev->mouseWheelScroll.delta > 0.0f ? 0.8f : 1.25f);
    if (sfKeyboard_isKeyPressed(sfKeyLShift))
        ed->cam.y -= ev->mouseWheelScroll.delta * 200.0 * ed->zoom;
    else
        ed->cam.x -= ev->mouseWheelScroll.delta * 200.0 * ed->zoom;
}

static void key_press(editor_t *ed, const sfEvent *ev)
{
    sfKeyCode k = ev->key.code;

    if (k == sfKeyEscape)
        return leave(ed);
    if (k == sfKeyS && ev->key.control)
        return save(ed);
    if (k >= sfKeyNum1 && k <= sfKeyNum9 && k - sfKeyNum1 < ed->nb_entries)
        ed->entry = k - sfKeyNum1;
    if (k == sfKeyDown || k == sfKeyUp)
        ed->entry = (ed->entry + (k == sfKeyDown ? 1 : ed->nb_entries - 1))
            % ed->nb_entries;
    if (k == sfKeyG)
        ed->grid = ed->grid == 50.0 ? 25.0 : 50.0;
    if (k == sfKeyHome)
        ed->cam = (vec2_t){ED_CANVAS_W / 2.0 * ed->zoom - 100.0,
            GROUND_Y - 300.0};
}

void editor_events(editor_t *ed, gd_t *gd)
{
    editor_apply_camera(ed, gd);             /* the mouse is read through it */
    while (poll_event(gd)) {
        const sfEvent *ev = gd->event;

        if (ev->type == sfEvtClosed)
            return close_window(gd->w);
        if (ev->type == sfEvtKeyPressed)
            key_press(ed, ev);
        if (ev->type == sfEvtMouseButtonPressed)
            mouse_press(ed, gd, ev);
        if (ev->type == sfEvtMouseMoved)
            mouse_move(ed, gd, ev);
        if (ev->type == sfEvtMouseWheelScrolled)
            wheel(ed, gd, ev);
        if (ev->type == sfEvtMouseButtonReleased
            || ev->type == sfEvtLostFocus) {
            ed->panning = false;
            ed->placing = false;
            ed->erasing = false;
        }
    }
}
