/*
** ALEXNEX PROJECT, 2026
** editor/ed_music.c
** File description:
** the level's song in the editor: where it is along the level, its beats (FEATURES 11.11)
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "editor/ed_music.h"
#include "sim/constants.h"
#include "sim/speed.h"

/* The offset the game will start the level's song at (4.4). */
double ed_song_offset(const ed_level_t *lv, const library_t *lib)
{
    return music_choose(lib, NULL, &lv->hdr, SONG_LEVEL).offset;
}

/*
** The level's speed portals, in x order: time along the level isn't one
** division once the speed changes (FEATURES 10.3). Built at each call from
** the objects as they are now; the caller frees it.
*/
static speed_map_t speeds_of(const ed_level_t *lv)
{
    speed_map_t m = {0};

    for (size_t i = 0; i < lv->count; i++)
        if (!lv->objects[i].is_start)
            speed_map_add(&m, &lv->objects[i].obj);
    speed_map_sort(&m);
    return m;
}

/* Seconds from the level's start to x, taking every speed portal. */
static double seconds_to(const ed_level_t *lv, double x)
{
    speed_map_t m = speeds_of(lv);
    double ticks = speed_map_ticks(&m, &lv->hdr.start, x);

    speed_map_free(&m);
    return ticks / TICK_RATE;
}

double ed_time_at_x(const ed_level_t *lv, const library_t *lib, double x)
{
    return ed_song_offset(lv, lib) + seconds_to(lv, x);
}

double ed_x_at_time(const ed_level_t *lv, const library_t *lib, double t)
{
    speed_map_t m = speeds_of(lv);
    double x = speed_map_x(&m, &lv->hdr.start,
        (t - ed_song_offset(lv, lib)) * TICK_RATE);

    speed_map_free(&m);
    return x;
}

double ed_beat_x(const ed_level_t *lv, const library_t *lib, long k)
{
    return ed_x_at_time(lv, lib, lv->hdr.first_beat
        + (double)k * 60.0 / lv->hdr.bpm);
}

/* The formula turned around: the first beat at or after left, the last at
** or before right. */
bool ed_beats_between(const ed_level_t *lv, const library_t *lib, double left,
    double right, long *first, long *last)
{
    double per_beat;

    if (lv->hdr.bpm <= 0.0)
        return false;
    per_beat = 60.0 / lv->hdr.bpm;
    *first = (long)ceil((ed_time_at_x(lv, lib, left) - lv->hdr.first_beat)
        / per_beat - 1e-9);
    *last = (long)floor((ed_time_at_x(lv, lib, right) - lv->hdr.first_beat)
        / per_beat + 1e-9);
    return *last >= *first;
}

double ed_level_seconds(const ed_level_t *lv, double end_x)
{
    return seconds_to(lv, end_x);
}

double ed_song_short_by(const ed_level_t *lv, const library_t *lib,
    double end_x)
{
    song_choice_t c = music_choose(lib, NULL, &lv->hdr, SONG_LEVEL);
    double over;

    if (c.song == NULL)
        return 0.0;
    over = c.offset + ed_level_seconds(lv, end_x) - c.song->duration;
    return over > 0.0 ? over : 0.0;
}

const char *ed_song_prop_name(ed_song_prop_t p)
{
    static const char *const NAMES[ED_SONG_PROP_COUNT] = {"Song", "Offset",
        "BPM", "Beat 1"};

    return NAMES[p];
}

/* Its place in the library, or -1: no song, or one the library hasn't. */
static long song_index(const ed_level_t *lv, const library_t *lib)
{
    const song_t *s = lv->hdr.music[0] != '\0'
        ? library_find(lib, lv->hdr.music) : NULL;

    return s == NULL ? -1 : (long)(s - lib->songs);
}

void ed_song_prop_text(const ed_level_t *lv, const library_t *lib,
    ed_song_prop_t p, char *out, size_t size)
{
    long at = song_index(lv, lib);

    if (p == ED_SONG_FILE && at >= 0)
        snprintf(out, size, "%s", lib->songs[at].title);
    else if (p == ED_SONG_FILE)
        snprintf(out, size, "%s", lv->hdr.music[0] != '\0' ? "missing"
            : "none");
    if (p == ED_SONG_OFFSET)
        snprintf(out, size, "%.2f s", lv->hdr.music_offset);
    if (p == ED_SONG_BPM)
        snprintf(out, size, "%.10g", lv->hdr.bpm);
    if (p == ED_SONG_FIRST_BEAT)
        snprintf(out, size, "%.2f s", lv->hdr.first_beat);
}

/* "none", then each song of the library, and round again. */
static void step_song(ed_level_t *lv, const library_t *lib, int dir)
{
    long n = (long)lib->count + 1;
    long at = (song_index(lv, lib) + 1 + dir + n) % n;

    if (at == 0)
        lv->hdr.music[0] = '\0';
    else
        snprintf(lv->hdr.music, sizeof(lv->hdr.music), "%s",
            lib->songs[at - 1].file);
}

/* Never under zero, and kept on hundredths: 0.05 forty times is 2. */
static double stepped(double v, double by)
{
    v = round((v + by) * 100.0) / 100.0;
    return v < 0.0 ? 0.0 : v;
}

bool ed_song_prop_step(ed_level_t *lv, const library_t *lib, ed_song_prop_t p,
    int dir, bool big)
{
    level_header_t before = lv->hdr;

    if (p == ED_SONG_FILE)
        step_song(lv, lib, dir);
    if (p == ED_SONG_OFFSET)
        lv->hdr.music_offset = stepped(lv->hdr.music_offset,
            dir * (big ? 1.0 : 0.05));
    if (p == ED_SONG_BPM)
        lv->hdr.bpm = stepped(lv->hdr.bpm, dir * (big ? 10.0 : 1.0));
    if (p == ED_SONG_FIRST_BEAT)
        lv->hdr.first_beat = stepped(lv->hdr.first_beat,
            dir * (big ? 0.1 : 0.01));
    if (strcmp(before.music, lv->hdr.music) == 0
        && before.music_offset == lv->hdr.music_offset
        && before.bpm == lv->hdr.bpm
        && before.first_beat == lv->hdr.first_beat)
        return false;
    lv->dirty = true;
    return true;
}
