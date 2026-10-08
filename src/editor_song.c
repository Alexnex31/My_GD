/*
** ALEXNEX PROJECT, 2026
** editor_song
** File description:
** the level's song over the canvas: hear it from here, beat lines (FEATURES 11.11)
*/

#include "editor.h"

static const sfColor BEAT = {255, 200, 80, 70};
static const sfColor BAR = {255, 200, 80, 170};
static const sfColor PLAYHEAD = {255, 90, 200, 255};

void editor_song_stop(editor_t *ed, gd_t *gd)
{
    if (!ed->song_on)
        return;
    ed->song_on = false;
    music_menu(gd);                          /* the menus' song, where it was */
}

/*
** P: the song the level will play, from the time the middle of the canvas
** is reached at; P again stops it. Before the level's start there is no
** song yet: it plays from its offset.
*/
void editor_song_toggle(editor_t *ed, gd_t *gd)
{
    song_choice_t c = music_choose(&gd->library, NULL, &ed->lv.hdr,
        SONG_LEVEL);
    double from = ed_time_at_x(&ed->lv, &gd->library, ed->cam.x);

    if (ed->song_on)
        return editor_song_stop(ed, gd);
    if (c.song == NULL || music_load(&gd->music, c.song->file) != 0)
        return editor_notice(ed, "No song to play: L chooses the level's");
    if (from >= c.song->duration)
        return editor_notice(ed, "The song is over before this point");
    music_set_loop(&gd->music, false);
    music_play_from(&gd->music, from < c.offset ? c.offset : from);
    ed->song_on = true;
}

/*
** The canvas turns the page when the line reaches its right edge, so the
** song can be followed along the whole level without touching anything.
*/
void editor_song_follow(editor_t *ed, gd_t *gd)
{
    double half = ED_CANVAS_W * ed->zoom / 2.0;
    double x;

    if (!ed->song_on || !music_is_playing(&gd->music))
        return;
    x = ed_x_at_time(&ed->lv, &gd->library, music_position(&gd->music));
    if (x > ed->cam.x + half * 0.9 && x < ed->cam.x + half * 1.5)
        ed->cam.x = x + half * 0.8;
}

/* Every beat in view, each fourth one brighter: a bar (11.11). */
static void beat_lines(editor_t *ed, gd_t *gd, double left, double right,
    double top, double bottom)
{
    long first = 0;
    long last = 0;

    if (!ed_beats_between(&ed->lv, &gd->library, left, right, &first, &last)
        || last - first > 4000)
        return;
    for (long k = first; k <= last; k++) {
        double x = ed_beat_x(&ed->lv, &gd->library, k);

        editor_line(ed, x, top, x, bottom, k % 4 == 0 ? BAR : BEAT);
    }
}

/* Where the song is in the level while it plays: a line that moves. */
void editor_song_lines(editor_t *ed, gd_t *gd, double left, double right,
    double top, double bottom)
{
    double x;

    beat_lines(ed, gd, left, right, top, bottom);
    if (!ed->song_on)
        return;
    if (!music_is_playing(&gd->music))
        return editor_song_stop(ed, gd);     /* it reached its end */
    x = ed_x_at_time(&ed->lv, &gd->library, music_position(&gd->music));
    editor_line(ed, x, top, x, bottom, PLAYHEAD);
    editor_line(ed, x + 2.0 * ed->zoom, top, x + 2.0 * ed->zoom, bottom,
        PLAYHEAD);
}
