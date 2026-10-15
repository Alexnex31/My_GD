/*
** ALEXNEX PROJECT, 2026
** editor_verify
** File description:
** the bot on the level, what looks wrong in it, the strip that says both (FEATURES 11.10, 11.13)
*/

#include "editor.h"

    #define ISSUES_W 430.0f       /* the strip's left part: a click goes on  */
    #define TIMES_X (ED_PALETTE_W + ISSUES_W + 20.0f)
    #define VERDICT_X (TIMES_X + 520.0f)

static const sfColor STRIP = {18, 20, 32, 240};
static const sfColor QUIET = {180, 190, 210, 255};
static const sfColor LOUD = {255, 230, 120, 255};

/*
** After each edit: where the level ends, what looks wrong in it, and how
** much of it is left when its song is over (11.13, 4.9).
*/
void editor_measure(editor_t *ed, gd_t *gd)
{
    if (ed->measured)
        return;
    ed_level_measure(&ed->lv, &ed->end_x, &ed->kill_y);
    ed_check(&ed->lv, &ed->report);
    ed->song_short = ed_song_short_by(&ed->lv, &gd->library, ed->end_x);
    ed->level_seconds = ed_level_seconds(&ed->lv, ed->end_x);
    ed->measured = true;
}

void editor_verify_start(editor_t *ed)
{
    ed->repeating = false;
    ed->verdict[0] = '\0';
    ed_verify_begin(&ed->verify, &ed->lv);
}

/*
** The bot's run becomes the line on the canvas, as a playtest's does. With
** no path, the canvas goes to where it got no further: that spot is the
** one to look at, and to try by hand.
*/
static void verified(editor_t *ed)
{
    ed_verify_t *v = &ed->verify;

    ed_verify_text(v, ed->verdict, sizeof(ed->verdict));
    free(ed->trace);
    ed->trace = v->path;
    ed->trace_len = v->path_len;
    ed->trace_edits = ed->hist.edits;
    ed->trace_bot = true;
    ed->trace_stopped = v->verdict != BOT_FOUND;
    v->path = NULL;
    if (ed->trace_stopped)
        ed->cam = v->end;
    editor_notice(ed, ed->verdict);
    ed_verify_free(v);
}

/*
** While it searches, the editor only draws, and Escape stops it. The search
** gets a few milliseconds of each frame (11.10), in slices of ticks small
** enough that one never runs far past them.
*/
void editor_verify_events(editor_t *ed, gd_t *gd)
{
    int64_t until = ui_now_ms() + ED_VERIFY_FRAME_MS;

    while (poll_event(gd)) {
        const sfEvent *ev = gd->event;

        if (ev->type == sfEvtClosed) {
            ed_verify_free(&ed->verify);
            return editor_close(ed, gd);
        }
        if (ev->type == sfEvtKeyPressed && ev->key.code == sfKeyEscape) {
            ed_verify_free(&ed->verify);
            return editor_notice(ed, "Verify stopped");
        }
    }
    while (!ed_verify_step(&ed->verify, ED_VERIFY_SLICE, BOT_MAX_TICKS))
        if (ui_now_ms() >= until)
            return;
    verified(ed);
}

/* N, or a click on the strip: the canvas goes to the next flagged object. */
void editor_next_issue(editor_t *ed)
{
    const ed_report_t *r = &ed->report;
    const ed_issue_t *at;
    const ed_object_t *o;
    char text[96];

    if (r->count == 0)
        return editor_notice(ed, "Nothing looks wrong");
    ed->issue_at = (ed->issue_at + 1) % (int)r->count;
    at = &r->issues[ed->issue_at];
    o = ed_find(&ed->lv, at->id);
    if (o == NULL)
        return;
    ed->cam = (vec2_t){o->obj.rect.x + o->obj.rect.w / 2.0,
        o->obj.rect.y + o->obj.rect.h / 2.0};
    ed_select_none(&ed->lv);
    ed_select_click(&ed->lv, at->id, false);
    ed->tool = ED_TOOL_SELECT;
    snprintf(text, sizeof(text), "%d/%zu  %s: %s", ed->issue_at + 1, r->count,
        ed_issue_is_hint(at->kind) ? "hint" : "warning",
        ed_issue_text(at->kind));
    editor_notice(ed, text);
}

bool editor_status_holds(sfVector2f at)
{
    return at.x >= ED_PALETTE_W && at.y >= ED_STATUS_Y;
}

static void issues_text(const editor_t *ed, char *out, size_t size)
{
    const ed_report_t *r = &ed->report;

    if (r->count == 0)
        snprintf(out, size, "Nothing looks wrong");
    else
        snprintf(out, size, "%zu warning%s, %zu hint%s   (N: next)",
            r->warnings, r->warnings == 1 ? "" : "s", r->hints,
            r->hints == 1 ? "" : "s");
}

/* The level's length against its song's (11.10, 4.9). */
static void times_text(editor_t *ed, gd_t *gd, char *out, size_t size)
{
    song_choice_t c = music_choose(&gd->library, NULL, &ed->lv.hdr,
        SONG_LEVEL);
    double level = ed->level_seconds;

    if (c.song == NULL)
        snprintf(out, size, "Level %.1f s, no song", level);
    else if (ed->song_short > 0.0)
        snprintf(out, size, "Level %.1f s: %.1f s longer than its song",
            level, ed->song_short);
    else
        snprintf(out, size, "Level %.1f s, song %.1f s", level,
            c.song->duration - c.offset);
}

/* What the bot is doing, what it last said, or how to ask it. */
static void verdict_text(editor_t *ed, char *out, size_t size)
{
    if (ed_verify_running(&ed->verify))
        return ed_verify_text(&ed->verify, out, size);
    if (ed->verdict[0] != '\0' && ed->trace_bot
        && ed->trace_edits == ed->hist.edits)
        snprintf(out, size, "%s", ed->verdict);
    else
        snprintf(out, size, "V: verify with the bot");
}

void editor_status_draw(editor_t *ed, gd_t *gd)
{
    char text[200];

    editor_fill(gd, ed, (sfFloatRect){ED_PALETTE_W, ED_STATUS_Y, ED_CANVAS_W,
        ED_STATUS_H}, STRIP, sfTransparent);
    issues_text(ed, text, sizeof(text));
    editor_label(ed, gd, text, ED_PALETTE_W + 20.0f, ED_STATUS_Y + 8.0f, 20,
        ed->report.warnings > 0 ? LOUD : QUIET);
    times_text(ed, gd, text, sizeof(text));
    editor_label(ed, gd, text, TIMES_X, ED_STATUS_Y + 8.0f, 20,
        ed->song_short > 0.0 ? LOUD : QUIET);
    verdict_text(ed, text, sizeof(text));
    editor_label(ed, gd, text, VERDICT_X, ED_STATUS_Y + 8.0f, 20, QUIET);
}
