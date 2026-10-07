/*
** ALEXNEX PROJECT, 2026
** editor_scene
** File description:
** the editor: opening it, its keys, saving (FEATURES 11.6)
*/

#include "editor.h"

void editor_notice(editor_t *ed, const char *what)
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
    ed_history_free(&ed->hist);
    ed_clip_free(&ed->clip);
    ed_grab_cancel(&ed->lv, &ed->grab);
    ed_level_free(&ed->lv);
    free(ed);
}

static void save(editor_t *ed)
{
    if (ed_level_save(&ed->lv, ed->path) != 0)
        return editor_notice(ed, "Could not save the level");
    ed->warned = false;
    editor_notice(ed, "Saved");
}

/* Escape with unsaved changes warns once; the second one leaves (11.12). */
static void leave(editor_t *ed)
{
    if (ed->lv.dirty && !ed->warned) {
        ed->warned = true;
        return editor_notice(ed, "Unsaved changes: Ctrl+S saves, Escape again leaves");
    }
    ed->leave = true;
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
    editor_apply_camera(ed, gd);
    editor_mouse_move(ed, gd, NULL);         /* the world moved under it */
}

/* Ctrl and a letter: the edits that aren't made with the mouse. */
static void ctrl_key(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfKeyCode k = ev->key.code;
    bool redo = k == sfKeyY || (k == sfKeyZ && ev->key.shift);

    if (k == sfKeyS)
        return save(ed);
    if (redo && !ed_redo(&ed->lv, &ed->hist))
        editor_notice(ed, "Nothing to redo");
    if (k == sfKeyZ && !ev->key.shift && !ed_undo(&ed->lv, &ed->hist))
        editor_notice(ed, "Nothing to undo");
    if (k == sfKeyA) {
        ed_select_all(&ed->lv);
        ed->tool = ED_TOOL_SELECT;
    }
    if (k == sfKeyC && ed_copy(&ed->lv, &ed->clip))
        editor_notice(ed, "Copied");
    if (k == sfKeyV && editor_mouse_on_canvas(gd) && ed_paste(&ed->lv,
        &ed->hist, &ed->clip, editor_mouse_world(ed, gd), ed->grid))
        ed->tool = ED_TOOL_SELECT;
    if (k == sfKeyD)
        ed_duplicate(&ed->lv, &ed->hist, ed->grid);
}

/* With a selection the arrows move it, a grid step or with Shift a pixel;
** with none, Up and Down still go through the palette. */
static void arrow_key(editor_t *ed, const sfEvent *ev)
{
    sfKeyCode k = ev->key.code;
    double step = ev->key.shift ? 1.0 : ed->grid;
    double dx = k == sfKeyRight ? step : (k == sfKeyLeft ? -step : 0.0);
    double dy = k == sfKeyDown ? step : (k == sfKeyUp ? -step : 0.0);

    if (ed_selected(&ed->lv) > 0) {
        ed_nudge(&ed->lv, &ed->hist, dx, dy);
        return;
    }
    if (dy != 0.0) {
        ed->entry = (ed->entry + (dy > 0.0 ? 1 : ed->nb_entries - 1))
            % ed->nb_entries;
        ed->tool = ED_TOOL_PLACE;
    }
}

/* Z, Q, S, D: the selection moves a whole block, whatever the grid. */
static void block_key(editor_t *ed, sfKeyCode k)
{
    double dx = k == sfKeyD ? ED_BIG_STEP : (k == sfKeyQ ? -ED_BIG_STEP : 0.0);
    double dy = k == sfKeyS ? ED_BIG_STEP : (k == sfKeyZ ? -ED_BIG_STEP : 0.0);

    if (dx != 0.0 || dy != 0.0)
        ed_nudge(&ed->lv, &ed->hist, dx, dy);
}

static void plain_key(editor_t *ed, sfKeyCode k)
{
    block_key(ed, k);
    if (k == sfKeyTab)
        ed->tool = ed->tool == ED_TOOL_PLACE ? ED_TOOL_SELECT : ED_TOOL_PLACE;
    if (k == sfKeyDelete || k == sfKeyBackspace)
        ed_delete_selected(&ed->lv, &ed->hist);
    if (k >= sfKeyNum1 && k <= sfKeyNum9 && k - sfKeyNum1 < ed->nb_entries) {
        ed->entry = k - sfKeyNum1;
        ed->tool = ED_TOOL_PLACE;
    }
    if (k == sfKeyG)
        ed->grid = ed->grid == 50.0 ? 25.0 : 50.0;
    if (k == sfKeyHome)
        ed->cam = (vec2_t){ED_CANVAS_W / 2.0 * ed->zoom - 100.0,
            GROUND_Y - 300.0};
}

/*
** Escape first cancels what the mouse is doing, then drops the selection,
** then leaves. No key edits while the mouse is in the middle of something:
** an undo under a drag would pull the objects from under it.
*/
static void key_press(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    sfKeyCode k = ev->key.code;

    if (k == sfKeyEscape) {
        if (editor_mouse_cancel(ed))
            return;
        if (ed_selected(&ed->lv) > 0)
            return ed_select_none(&ed->lv);
        return leave(ed);
    }
    if (ed->mouse != ED_IDLE)
        return;
    ed_gesture(&ed->hist);
    ed->measured = false;
    if (ev->key.control)
        return ctrl_key(ed, gd, ev);
    if (k == sfKeyLeft || k == sfKeyRight || k == sfKeyUp || k == sfKeyDown)
        return arrow_key(ed, ev);
    plain_key(ed, k);
}

/* The keys worth holding: the moves, and going through the history. */
static bool repeats(const sfKeyEvent *k)
{
    if (k->control)
        return k->code == sfKeyZ || k->code == sfKeyY;
    return k->code == sfKeyLeft || k->code == sfKeyRight || k->code == sfKeyUp
        || k->code == sfKeyDown || k->code == sfKeyZ || k->code == sfKeyQ
        || k->code == sfKeyS || k->code == sfKeyD;
}

/*
** The window has the system's key repeat off, for the game's input
** (FEATURES 1.3), so the editor repeats a held key itself: once at the
** press, then after a wait, at a steady pace until it is released.
*/
static void key_repeat(editor_t *ed, gd_t *gd)
{
    sfEvent again = {.key = ed->repeat};

    if (!ed->repeating)
        return;
    if (!sfKeyboard_isKeyPressed(ed->repeat.code)) {
        ed->repeating = false;
        return;
    }
    if (ui_now_ms() < ed->repeat_at_ms)
        return;
    ed->repeat_at_ms = ui_now_ms() + ED_REPEAT_EVERY_MS;
    key_press(ed, gd, &again);
}

void editor_events(editor_t *ed, gd_t *gd)
{
    editor_apply_camera(ed, gd);             /* the mouse is read through it */
    while (poll_event(gd)) {
        const sfEvent *ev = gd->event;

        if (ev->type == sfEvtClosed)
            return close_window(gd->w);
        if (ev->type == sfEvtKeyPressed) {
            key_press(ed, gd, ev);
            ed->repeat = ev->key;
            ed->repeating = repeats(&ev->key);
            ed->repeat_at_ms = ui_now_ms() + ED_REPEAT_DELAY_MS;
        }
        if (ev->type == sfEvtLostFocus)
            ed->repeating = false;
        if (ev->type == sfEvtMouseButtonPressed && ed->mouse == ED_IDLE)
            editor_mouse_press(ed, gd, ev);
        if (ev->type == sfEvtMouseMoved)
            editor_mouse_move(ed, gd, ev);
        if (ev->type == sfEvtMouseWheelScrolled)
            wheel(ed, gd, ev);
        if (ev->type == sfEvtMouseButtonReleased)
            editor_mouse_release(ed, gd);
        if (ev->type == sfEvtLostFocus && !editor_mouse_cancel(ed))
            ed->mouse = ED_IDLE;
    }
    key_repeat(ed, gd);
}
