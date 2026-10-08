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

/* What the level's own panel shows and changes: its song and its beat. */
typedef enum ed_song_prop {
    ED_SONG_FILE,             /* through the library, and "none"              */
    ED_SONG_OFFSET,           /* seconds: 0.05, or 1 with `big`               */
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
