/*
** ALEXNEX PROJECT, 2026
** editor_play
** File description:
** the playtest: the level as it is now, played inside the editor (FEATURES 11.9)
*/

#include "editor.h"

/*
** Which start position Shift+Enter means, counted as the game counts them
** (1 is the first in the list): the selected one, or else the last one at
** or before the middle of the canvas, or else the first. 0: there is none.
*/
static int wanted_start(const editor_t *ed)
{
    double best = -INFINITY;
    int rank = 0;
    int before = 0;

    for (size_t i = 0; i < ed->lv.count; i++) {
        const ed_object_t *o = &ed->lv.objects[i];

        if (!o->is_start)
            continue;
        rank += 1;
        if (o->selected)
            return rank;
        if (o->start.pos.x <= ed->cam.x && o->start.pos.x >= best) {
            best = o->start.pos.x;
            before = rank;
        }
    }
    return before != 0 ? before : (rank > 0 ? 1 : 0);
}

/*
** Not over a notice of the level's own: "Song missing" says more (4.4). The
** start positions' keys are named as they are bound, when they are.
*/
static void banner(level_t *lv, const settings_t *s)
{
    char prev[BINDING_NAME_MAX];
    char next[BINDING_NAME_MAX];
    char text[160];

    if (lv->notice_left > 0.0f)
        return;
    binding_format(s->previous_start_key, prev, sizeof(prev));
    binding_format(s->next_start_key, next, sizeof(next));
    snprintf(text, sizeof(text), "Playtest - Escape: back to the editor%s%s%s"
        "%s%s", prev[0] || next[0] ? " - " : "", prev,
        prev[0] && next[0] ? " / " : "", next,
        prev[0] || next[0] ? ": start positions" : "");
    sfText_setUnicodeString(lv->notice_text, utf8_to_utf32(text));
    lv->notice_left = 4.0f;
}

/* The level as it is now, saved or not; nothing it does is recorded. */
void editor_playtest(editor_t *ed, gd_t *gd, bool from_start_position)
{
    level_doc_t doc;
    int at = from_start_position ? wanted_start(ed) : 0;

    if (from_start_position && at == 0)
        return editor_notice(ed, "No start position: the palette's last "
            "entry places one");
    editor_autosave(ed, true);               /* before every playtest (11.12) */
    ed->song_on = false;                     /* the playtest has the music */
    ed_level_doc(&ed->lv, &doc);
    ed->play = level_start_doc(gd, &doc, &ed->lv.hdr, ed->lv.id, at);
    ed_doc_free(&doc);
    editor_mouse_cancel(ed);                 /* nothing half done under it */
    ed->repeating = false;
    banner(ed->play, &gd->settings);
}

/* The path it took is kept, and the canvas goes to where it ended. */
void editor_play_stop(editor_t *ed, gd_t *gd)
{
    level_t *lv = ed->play;
    char text[96];

    if (lv == NULL)
        return;
    free(ed->trace);
    ed->trace = lv->path;
    ed->trace_len = lv->path_len;
    ed->trace_edits = ed->hist.edits;
    ed->trace_bot = false;
    ed->trace_stopped = false;
    lv->path = NULL;
    ed->cam = lv->state == LEVEL_DYING ? lv->death_pos : lv->sim.st.player.pos;
    if (lv->state == LEVEL_DYING) {
        snprintf(text, sizeof(text), "Playtest: died at %.0f%%",
            sim_percent(&lv->sim));
        editor_notice(ed, text);
    }
    level_free(lv, gd);
    ed->play = NULL;
    ed->measured = false;
    music_menu(gd);
}

/* Escape stops it; the level's keys act as in the game (6.5, 9.6, 11.8). */
static void play_input(editor_t *ed, gd_t *gd, const sfEvent *ev)
{
    const settings_t *s = &gd->settings;

    if (ev->type == sfEvtKeyPressed && ev->key.code == sfKeyEscape)
        return editor_play_stop(ed, gd);
    if (input_event_is(ev, s->debug_overlay_key))
        gd->debug_overlay = !gd->debug_overlay;
    if (input_event_is(ev, s->previous_start_key))
        level_change_start(ed->play, gd, -1);
    if (input_event_is(ev, s->next_start_key))
        level_change_start(ed->play, gd, 1);
    if (input_event_is(ev, s->restart_key))
        level_restart(ed->play, gd);
}

void editor_play_events(editor_t *ed, gd_t *gd)
{
    while (ed->play != NULL && poll_event(gd)) {
        const sfEvent *ev = gd->event;

        if (ev->type == sfEvtClosed) {
            editor_play_stop(ed, gd);
            return editor_close(ed, gd);
        }
        if (ev->type == sfEvtLostFocus) {    /* the keys are someone else's */
            editor_play_stop(ed, gd);
            return editor_notice(ed, "Playtest stopped: the window lost the "
                "focus");
        }
        play_input(ed, gd, ev);
    }
    if (ed->play != NULL && ed->play->playtest_over)
        editor_play_stop(ed, gd);
}
