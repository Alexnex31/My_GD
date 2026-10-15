/*
** ALEXNEX PROJECT, 2026
** editor/ed_music.h
** File description:
** header file for my_gd project
*/

#ifndef EDITOR_ED_MUSIC_H
    #define EDITOR_ED_MUSIC_H

    #include "editor/ed_level.h"
    #include "music/library.h"

/*
** Where the song is along the level (FEATURES 11.11): the run starts the
** song at its offset and moves at the level's speed, so a time in the song
** is an x and an x a time. Pure, tested alone.
*/
double ed_song_offset(const ed_level_t *lv, const library_t *lib);
double ed_time_at_x(const ed_level_t *lv, const library_t *lib, double x);
double ed_x_at_time(const ed_level_t *lv, const library_t *lib, double t);

/* The beats between two x: false when the level has no bpm. */
bool ed_beats_between(const ed_level_t *lv, const library_t *lib, double left,
    double right, long *first, long *last);
double ed_beat_x(const ed_level_t *lv, const library_t *lib, long k);
/*
** The x of every beat between two x, the first one's number in `first`:
** what the canvas draws each frame, one speed map for all of them. 0 when
** there are none, or more than `max`.
*/
size_t ed_beat_xs(const ed_level_t *lv, const library_t *lib, double left,
    double right, double *xs, size_t max, long *first);

/* How long the level lasts, from its start to `end_x`, at its speed. */
double ed_level_seconds(const ed_level_t *lv, double end_x);
/*
** Seconds of level left when its song is over (FEATURES 4.9): the song then
** loops. 0 when the song is long enough, or when the level has none.
*/
double ed_song_short_by(const ed_level_t *lv, const library_t *lib,
    double end_x);

/* What the level's own panel shows and changes: its song and its beat. */
typedef enum ed_song_prop {
    ED_SONG_FILE,             /* through the library, and "none"              */
    ED_SONG_OFFSET,           /* seconds: 0.05, or 1 with `big`; from the     */
                              /* song's own until the level has one           */
    ED_SONG_BPM,              /* 1, or 10                                     */
    ED_SONG_FIRST_BEAT,       /* seconds: 0.01, or 0.1                        */
    ED_SONG_PROP_COUNT
} ed_song_prop_t;

const char *ed_song_prop_name(ed_song_prop_t p);
void ed_song_prop_text(const ed_level_t *lv, const library_t *lib,
    ed_song_prop_t p, char *out, size_t size);
/* false when nothing changed (a value already at 0). Marks the level dirty;
** the header isn't in the undo history. */
bool ed_song_prop_step(ed_level_t *lv, const library_t *lib, ed_song_prop_t p,
    int dir, bool big);

#endif
