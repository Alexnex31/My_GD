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

static void banner(level_t *lv)
{
    sfText_setUnicodeString(lv->notice_text, utf8_to_utf32(
        "Playtest - Escape: back to the editor - A / E: start positions"));
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
    ed_level_doc(&ed->lv, &doc);
    ed->play = level_start_doc(gd, &doc, &ed->lv.hdr, ed->lv.id, at);
    ed_doc_free(&doc);
    ed->mouse = ED_IDLE;
    ed->repeating = false;
    banner(ed->play);
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

static void play_key(editor_t *ed, gd_t *gd, sfKeyCode k)
{
    if (k == sfKeyEscape)
        return editor_play_stop(ed, gd);
    if (k == sfKeyF3)
        gd->debug_overlay = !gd->debug_overlay;
    if (k == sfKeyA || k == sfKeyE)
        level_change_start(ed->play, gd, k == sfKeyE ? 1 : -1);
}

void editor_play_events(editor_t *ed, gd_t *gd)
{
    while (ed->play != NULL && poll_event(gd)) {
        const sfEvent *ev = gd->event;

        if (ev->type == sfEvtClosed) {
            editor_play_stop(ed, gd);
            return close_window(gd->w);
        }
        if (ev->type == sfEvtGainedFocus)
            ed->play->last_frame_us = input_now_us(gd);
        if (ev->type == sfEvtKeyPressed)
            play_key(ed, gd, ev->key.code);
        if (ed->play != NULL && input_event_is(ev, gd->settings.restart_key))
            level_restart(ed->play, gd);
    }
    if (ed->play != NULL && ed->play->playtest_over)
        editor_play_stop(ed, gd);
}
