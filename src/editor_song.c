/*
** ALEXNEX PROJECT, 2026
** editor_song
** File description:
** the level's song over the canvas: hear it from here, beat lines (FEATURES 11.11)
*/

#include "editor.h"

    #define BEATS_MAX 4001    /* more in view is a grey wash: none are drawn */

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
** song yet: it plays from its offset. The audio offset applies, as in a
** level (4.8).
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
    from = (from < c.offset ? c.offset : from) + gd->music.audio_offset;
    if (from >= c.song->duration)
        return editor_notice(ed, "The song is over before this point");
    music_set_loop(&gd->music, false);
    music_play_from(&gd->music, from);
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

    music_update(gd);                        /* a song due later starts */
    if (!ed->song_on || !music_is_playing(&gd->music))
        return;
    x = ed_x_at_time(&ed->lv, &gd->library, music_position(&gd->music)
        - gd->music.audio_offset);
    if (x > ed->cam.x + half * 0.9 && x < ed->cam.x + half * 1.5)
        ed->cam.x = x + half * 0.8;
}

/* Every beat in view, each fourth one brighter: a bar (11.11). */
static void beat_lines(editor_t *ed, gd_t *gd, double left, double right,
    double top, double bottom)
{
    static double xs[BEATS_MAX];
    long first = 0;
    size_t n = ed_beat_xs(&ed->lv, &gd->library, left, right, xs, BEATS_MAX,
        &first);

    for (size_t i = 0; i < n; i++)
        editor_line(ed, xs[i], top, xs[i], bottom,
            (first + (long)i) % 4 == 0 ? BAR : BEAT);
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
    x = ed_x_at_time(&ed->lv, &gd->library, music_position(&gd->music)
        - gd->music.audio_offset);
    editor_line(ed, x, top, x, bottom, PLAYHEAD);
    editor_line(ed, x + 2.0 * ed->zoom, top, x + 2.0 * ed->zoom, bottom,
        PLAYHEAD);
}
