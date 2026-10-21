/*
** ALEXNEX PROJECT, 2026
** editor_scene
** File description:
** the editor: opening it, its keys, saving (FEATURES 11.6)
*/

#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

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
        level_clean_text(ed->lv.hdr.name);   /* as a save reads back (7.2) */
        if (ed->lv.hdr.name[0] == '\0')
            snprintf(ed->lv.hdr.name, sizeof(ed->lv.hdr.name), "LEVEL %s", id);
        ed->lv.dirty = true;                 /* it exists nowhere yet */
        ed->is_new = true;
    } else if (ed_level_open(&ed->lv, ed->path) != 0) {
        free(ed);
        return NULL;
    }
    ed->zoom = 1.0f;
    ed->grid = 50.0;
    ed->cam = (vec2_t){ED_CANVAS_W / 2.0 - 100.0, GROUND_Y - 300.0};
    ed->view = sfView_create();
    ed->nb_entries = ed_entries(ed->entries, ED_MAX_ENTRIES);
    ed->objects = sfVertexArray_create();
    sfVertexArray_setPrimitiveType(ed->objects, sfTriangles);
    ed->lines = sfVertexArray_create();
    sfVertexArray_setPrimitiveType(ed->lines, sfLines);
    ed->box = sfRectangleShape_create();
    ed->text = sfText_create();
    sfText_setFont(ed->text, gd->main_font);
    ed->marker = sfSprite_create();
    ed->issue_at = -1;
    ed->autosave_at_ms = ui_now_ms() + ED_AUTOSAVE_EVERY_MS;
    if (ed_autosave_pending(ED_AUTOSAVE_DIR, ed->lv.id, ed->path,
        &ed->restore_when))
        editor_dialog_open(ed, ED_DIALOG_RESTORE);
    return ed;
}

void editor_free(editor_t *ed)
{
    if (ed == NULL)
        return;
    sfVertexArray_destroy(ed->objects);
    free(ed->drawn);
    sfVertexArray_destroy(ed->lines);
    sfRectangleShape_destroy(ed->box);
    sfText_destroy(ed->text);
    sfSprite_destroy(ed->marker);
    sfView_destroy(ed->view);
    free(ed->trace);
    ed_verify_free(&ed->verify);
    ed_report_free(&ed->report);
    ed_history_free(&ed->hist);
    ed_grab_cancel(&ed->lv, &ed->grab);
    ed_level_free(&ed->lv);
    free(ed);
}

/*
** A saved level needs no autosave: it would only be offered back (11.12).
** A new level's first save never replaces a file that appeared since its
** id was chosen. levels/ is made if it isn't there: a fresh install.
*/
bool editor_save(editor_t *ed)
{
    if (ed->is_new && access(ed->path, F_OK) == 0) {
        editor_notice(ed, "Not saved: a level with this number appeared");
        return false;
    }
    if (mkdir("levels", 0755) != 0 && errno != EEXIST) {
        editor_notice(ed, "Could not make the levels folder");
        return false;
    }
    if (ed_level_save(&ed->lv, ed->path) != 0) {
        editor_notice(ed, "Could not save the level");
        return false;
    }
    ed->is_new = false;
    ed_autosave_remove(ED_AUTOSAVE_DIR, ed->lv.id);
    editor_notice(ed, "Saved");
    return true;
}

/* Leaving with unsaved changes asks first (11.12). */
static void leave(editor_t *ed)
{
    if (ed->lv.dirty)
        return editor_dialog_open(ed, ED_DIALOG_LEAVE);
    ed->leave = true;
}

/* The window's cross asks nothing: what wasn't saved is in the autosave,
** and is offered back the next time the level is opened. */
void editor_close(editor_t *ed, gd_t *gd)
{
    editor_autosave(ed, true);
    close_window(gd->w);
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
            ev->mouseWheelScroll.delta > 0.0f ? -1 : 1);
    if (sfKeyboard_isKeyPressed(sfKeyLShift)
        || sfKeyboard_isKeyPressed(sfKeyRShift))
        ed->cam.y -= ev->mouseWheelScroll.delta * 200.0 * ed->zoom;
    else
        ed->cam.x -= ev->mouseWheelScroll.delta * 200.0 * ed->zoom;
    editor_apply_camera(ed, gd);
    editor_mouse_move(ed, gd, NULL);         /* the world moved under it */
}

/* The key with the modifiers held, as the options capture one (11.6). */
static binding_t pressed(const sfKeyEvent *key)
{
    return (binding_t){BIND_KEY, (int)key->code,
        (key->control ? MOD_CTRL : 0) | (key->shift ? MOD_SHIFT : 0)
        | (key->alt ? MOD_ALT : 0)};
}

/*
** Escape first closes the list of keys, cancels what the mouse is doing,
** then drops the selection, then leaves. No key edits while the mouse is in
** the middle of something: an undo under a drag would pull the objects
** from under it. Every other key is the action it is bound to, if any.
*/
static void key_press(editor_t *ed, gd_t *gd, const sfKeyEvent *key)
{
    int k = ed_key_find(gd->settings.editor_keys, pressed(key));

    if (key->code == sfKeyEscape) {
        if (ed->show_keys) {
            ed->show_keys = false;
            return;
        }
        if (editor_mouse_cancel(ed))
            return;
        if (ed_selected(&ed->lv) > 0)
            return ed_select_none(&ed->lv);
        return leave(ed);
    }
    if (ed->mouse != ED_IDLE || k < 0)
        return;
    ed_gesture(&ed->hist);
    editor_key_action(ed, gd, (ed_key_t)k);
    ed->repeating = ed_key_repeats((ed_key_t)k);
    ed->repeat = *key;
    ed->repeat_at_ms = ui_now_ms() + ED_REPEAT_DELAY_MS;
}

/*
** The window has the system's key repeat off, for the game's input
** (FEATURES 1.3), so the editor repeats a held key itself: once at the
** press, then after a wait, at a steady pace until it is released. The
** moves and the history repeat (ed_key_repeats).
*/
static void key_repeat(editor_t *ed, gd_t *gd)
{
    int k = ed_key_find(gd->settings.editor_keys, pressed(&ed->repeat));

    if (!ed->repeating)
        return;
    if (!sfKeyboard_isKeyPressed(ed->repeat.code) || k < 0
        || ed->mouse != ED_IDLE) {
        ed->repeating = false;
        return;
    }
    if (ui_now_ms() < ed->repeat_at_ms)
        return;
    ed->repeat_at_ms = ui_now_ms() + ED_REPEAT_EVERY_MS;
    ed_gesture(&ed->hist);
    editor_key_action(ed, gd, (ed_key_t)k);
}

void editor_events(editor_t *ed, gd_t *gd)
{
    if (ed->play != NULL)
        return editor_play_events(ed, gd);
    if (ed->dialog != ED_DIALOG_NONE)
        return editor_dialog_events(ed, gd);
    if (ed_verify_running(&ed->verify))
        return editor_verify_events(ed, gd);
    editor_autosave(ed, false);
    editor_song_follow(ed, gd);
    if (ed->settings_open)
        return editor_settings_events(ed, gd);
    editor_apply_camera(ed, gd);             /* the mouse is read through it */
    while (poll_event(gd)) {
        const sfEvent *ev = gd->event;

        if (ev->type == sfEvtClosed)
            return editor_close(ed, gd);
        if (ev->type == sfEvtKeyPressed) {
            ed->repeating = false;           /* a new key: the old one stops */
            key_press(ed, gd, &ev->key);
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
            editor_mouse_release(ed, gd, ev);
        if (ev->type == sfEvtLostFocus)
            editor_mouse_cancel(ed);
    }
    key_repeat(ed, gd);
}
