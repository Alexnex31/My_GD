/*
** ALEXNEX PROJECT, 2026
** editor/ed_music.c
** File description:
** the level's song in the editor: where it is along the level, its beats (FEATURES 11.11)
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor/ed_music.h"
#include "sim/alloc.h"
#include "sim/constants.h"
#include "sim/level.h"
#include "sim/speed.h"

/* The offset the game will start the level's song at (4.4). */
double ed_song_offset(const ed_level_t *lv, const library_t *lib)
{
    return music_choose(lib, NULL, &lv->hdr, SONG_LEVEL).offset;
}

/*
** The level's speed portals, in x order: time along the level isn't one
** division once the speed changes (FEATURES 10.3). Built at each call from
** the objects as they are now; the caller frees it. Added in the list's
** order, the one the saved file keeps and the game adds them in: two at the
** same x then end the same way here as in the game.
*/
static speed_map_t speeds_of(const ed_level_t *lv)
{
    speed_map_t m = {0};

    for (size_t i = 0; i < lv->count; i++)
        if (!lv->objects[i].is_start && lv->objects[i].obj.type == OBJ_SPEED)
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
** or before right. One speed map for both ends. */
static bool beats_between(const ed_level_t *lv, const speed_map_t *m,
    double offset, double left_right[2], long first_last[2])
{
    double per_beat;
    double from;
    double to;

    if (lv->hdr.bpm <= 0.0)
        return false;
    per_beat = 60.0 / lv->hdr.bpm;
    from = offset + speed_map_ticks(m, &lv->hdr.start, left_right[0])
        / TICK_RATE;
    to = offset + speed_map_ticks(m, &lv->hdr.start, left_right[1])
        / TICK_RATE;
    first_last[0] = (long)ceil((from - lv->hdr.first_beat) / per_beat - 1e-9);
    first_last[1] = (long)floor((to - lv->hdr.first_beat) / per_beat + 1e-9);
    return first_last[1] >= first_last[0];
}

bool ed_beats_between(const ed_level_t *lv, const library_t *lib, double left,
    double right, long *first, long *last)
{
    speed_map_t m = speeds_of(lv);
    double ends[2] = {left, right};
    long beats[2] = {0, 0};
    bool any = beats_between(lv, &m, ed_song_offset(lv, lib), ends, beats);

    speed_map_free(&m);
    *first = beats[0];
    *last = beats[1];
    return any;
}

/*
** Every beat between two x, for the canvas each frame: the speed map is
** built once for all of them, not once a beat. 0 when there are none, or
** more than `max`.
*/
size_t ed_beat_xs(const ed_level_t *lv, const library_t *lib, double left,
    double right, double *xs, size_t max, long *first)
{
    speed_map_t m = speeds_of(lv);
    double offset = ed_song_offset(lv, lib);
    double ends[2] = {left, right};
    long beats[2] = {0, 0};
    size_t n = 0;

    if (beats_between(lv, &m, offset, ends, beats)
        && beats[1] - beats[0] < (long)max) {
        for (long k = beats[0]; k <= beats[1]; k++) {
            double t = lv->hdr.first_beat + (double)k * 60.0 / lv->hdr.bpm;

            xs[n] = speed_map_x(&m, &lv->hdr.start, (t - offset) * TICK_RATE);
            n += 1;
        }
        *first = beats[0];
    }
    speed_map_free(&m);
    return n;
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
    if (p == ED_SONG_OFFSET && !lv->hdr.music_offset_set)
        snprintf(out, size, "%.2f s, the song's", ed_song_offset(lv, lib));
    else if (p == ED_SONG_OFFSET)
        snprintf(out, size, "%.2f s", lv->hdr.music_offset);
    if (p == ED_SONG_BPM)
        snprintf(out, size, "%.10g", lv->hdr.bpm);
    if (p == ED_SONG_FIRST_BEAT)
        snprintf(out, size, "%.2f s", lv->hdr.first_beat);
}

/*
** A song a level can name: its file fits the field and reads back the same,
** with no '#' and no blanks around it (7.2).
*/
static bool nameable(const ed_level_t *lv, const char *file)
{
    char clean[sizeof(lv->hdr.music)];

    if (strlen(file) >= sizeof(clean))
        return false;
    snprintf(clean, sizeof(clean), "%s", file);
    level_clean_text(clean);
    return strcmp(clean, file) == 0;
}

/* "none", then each song of the library a level can name, and round again. */
static void step_song(ed_level_t *lv, const library_t *lib, int dir)
{
    long n = (long)lib->count + 1;
    long at = song_index(lv, lib) + 1;

    for (long tries = 0; tries < n; tries++) {
        at = (at + dir + n) % n;
        if (at == 0 || nameable(lv, lib->songs[at - 1].file))
            break;
    }

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

/*
** With no offset of its own the level plays the song from the song's
** default: a step starts from there, so the song never jumps, and the
** level then has its own, which can be 0.
*/
static void step_offset(ed_level_t *lv, const library_t *lib, double by)
{
    double from = lv->hdr.music_offset_set ? lv->hdr.music_offset
        : ed_song_offset(lv, lib);
    double to = stepped(from, by);

    if (to == from)
        return;
    lv->hdr.music_offset = to;
    lv->hdr.music_offset_set = true;
}

bool ed_song_prop_step(ed_level_t *lv, const library_t *lib, ed_song_prop_t p,
    int dir, bool big)
{
    level_header_t before = lv->hdr;

    if (p == ED_SONG_FILE)
        step_song(lv, lib, dir);
    if (p == ED_SONG_OFFSET)
        step_offset(lv, lib, dir * (big ? 1.0 : 0.05));
    if (p == ED_SONG_BPM)
        lv->hdr.bpm = fmin(stepped(lv->hdr.bpm, dir * (big ? 10.0 : 1.0)),
            LEVEL_BPM_MAX);                  /* what a file may say (7.2) */
    if (p == ED_SONG_FIRST_BEAT)
        lv->hdr.first_beat = stepped(lv->hdr.first_beat,
            dir * (big ? 0.1 : 0.01));
    if (strcmp(before.music, lv->hdr.music) == 0
        && before.music_offset == lv->hdr.music_offset
        && before.music_offset_set == lv->hdr.music_offset_set
        && before.bpm == lv->hdr.bpm
        && before.first_beat == lv->hdr.first_beat)
        return false;
    lv->dirty = true;
    return true;
}
