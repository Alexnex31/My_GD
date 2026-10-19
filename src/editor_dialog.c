/*
** ALEXNEX PROJECT, 2026
** editor_dialog
** File description:
** the editor's questions: unsaved changes on leaving, an autosave to restore (FEATURES 11.12)
*/

#include <time.h>

#include "editor.h"

static void first_button(void *ctx, widget_t *w)
{
    (void)w;
    ((editor_t *)ctx)->answer = 1;
}

static void second_button(void *ctx, widget_t *w)
{
    (void)w;
    ((editor_t *)ctx)->answer = 2;
}

static void third_button(void *ctx, widget_t *w)
{
    (void)w;
    ((editor_t *)ctx)->answer = 3;
}

/* Escape is the answer that loses nothing: Cancel, or Restore. */
static void dialog_back(void *ctx)
{
    editor_t *ed = ctx;

    ed->answer = ed->dialog == ED_DIALOG_LEAVE ? 3 : 1;
}

static widget_t button(const char *label, float x, widget_fn fn)
{
    return (widget_t){.kind = W_BUTTON, .enabled = true, .label = label,
        .bounds = {x, 600.0f, 300.0f, 90.0f}, .on_activate = fn};
}

/*
** Leaving: Save, Discard, Cancel, with Cancel focused so Enter loses
** nothing. An autosave found: Restore, focused, or Discard (11.12).
*/
void editor_dialog_open(editor_t *ed, ed_dialog_t which)
{
    bool leave = which == ED_DIALOG_LEAVE;

    ed->dialog = which;
    ed->answer = 0;
    ed->repeating = false;
    ed->dialog_widgets[0] = button(leave ? "Save" : "Restore",
        leave ? 470.0f : 640.0f, first_button);
    ed->dialog_widgets[1] = button("Discard", leave ? 810.0f : 980.0f,
        second_button);
    ed->dialog_widgets[2] = button("Cancel", 1150.0f, third_button);
    ui_init(&ed->dialog_ui, ed->dialog_widgets, leave ? 3 : 2, ed);
    ed->dialog_ui.focused = leave ? 2 : 0;
    ed->dialog_ui.on_back = dialog_back;
    ed->dialog_ui.panel = (ui_rect_t){260.0f, 370.0f, 1400.0f, 370.0f};
}

/* Save leaves only if the save worked; Discard forgets the autosave too. */
static void answer_leave(editor_t *ed, int answer)
{
    if (answer == 1 && editor_save(ed))
        ed->leave = true;
    if (answer == 2) {
        ed_autosave_remove(ED_AUTOSAVE_DIR, ed->lv.id);
        ed->leave = true;
    }
}

/* Restored, the level is the autosave's, with unsaved changes: nothing of
** the file is lost until Ctrl+S. */
static void answer_restore(editor_t *ed, int answer)
{
    if (answer == 2)
        return ed_autosave_remove(ED_AUTOSAVE_DIR, ed->lv.id);
    if (ed_autosave_restore(&ed->lv, ED_AUTOSAVE_DIR) != 0)
        return editor_notice(ed, "The autosave could not be read");
    ed->measured = false;
    editor_notice(ed, "Restored: Ctrl+S saves it, leaving can still discard it");
}

void editor_dialog_events(editor_t *ed, gd_t *gd)
{
    ed_dialog_t which = ed->dialog;
    ui_event_t ev;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return editor_close(ed, gd);
        if (ui_from_sf(gd, gd->event, &ev))
            ui_event(&ed->dialog_ui, &ev, ui_now_ms());
    }
    if (ed->answer == 0)
        return;
    ed->dialog = ED_DIALOG_NONE;
    if (which == ED_DIALOG_LEAVE)
        answer_leave(ed, ed->answer);
    else
        answer_restore(ed, ed->answer);
}

static void centered(editor_t *ed, gd_t *gd, const char *s, float y)
{
    float w = editor_label_width(ed, s, 34);

    editor_label(ed, gd, s, VIEW_W / 2.0f - w / 2.0f, y, 34, sfWhite);
}

void editor_dialog_draw(editor_t *ed, gd_t *gd)
{
    char line[200];
    char when[32];

    if (ed->dialog == ED_DIALOG_NONE)
        return;
    editor_fill(gd, ed, (sfFloatRect){0.0f, 0.0f, VIEW_W, VIEW_H},
        (sfColor){0, 0, 0, 150}, sfTransparent);
    ui_update(&ed->dialog_ui, ui_now_ms());
    ui_draw(gd, &ed->dialog_ui);
    if (ed->dialog == ED_DIALOG_LEAVE) {
        snprintf(line, sizeof(line), "Save changes to %s?", ed->lv.hdr.name);
        return centered(ed, gd, line, 450.0f);
    }
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M",
        localtime(&ed->restore_when));
    snprintf(line, sizeof(line), "Restore unsaved changes from %s?", when);
    centered(ed, gd, line, 420.0f);
    centered(ed, gd, "Discard opens the level as it was last saved.", 490.0f);
}

/*
** Every minute while there are unsaved changes, and when asked (before a
** playtest, when the window is closed): beside the level, never over it.
** Never while the Restore question is up: the autosave is what it offers,
** and a new level, unsaved from the start, would write over it.
*/
void editor_autosave(editor_t *ed, bool now)
{
    if (ed->dialog == ED_DIALOG_RESTORE)
        return;
    if (!now && ui_now_ms() < ed->autosave_at_ms)
        return;
    ed->autosave_at_ms = ui_now_ms() + ED_AUTOSAVE_EVERY_MS;
    if (ed->lv.dirty && ed_autosave_write(&ed->lv, ED_AUTOSAVE_DIR) != 0)
        editor_notice(ed, "The autosave could not be written");
}
