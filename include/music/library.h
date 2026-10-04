/*
** ALEXNEX PROJECT, 2026
** music/library.h
** File description:
** header file for my_gd project
*/

#ifndef MUSIC_LIBRARY_H
    #define MUSIC_LIBRARY_H

    #include "sim/sim_types.h"

    #define MUSIC_DIR "music"
    #define SONGS_META "songs.txt"
    #define DEFAULT_LEVEL_SONG "back_mus.ogg"
    #define SONG_FILE_MAX 64          /* a level's `music` field's size (7.2) */
    #define RESYNC_S 0.05             /* drift worth a seek (FEATURES 4.8)    */

/*
** The songs in music/ (FEATURES 4.2). Pure C: what decodes a file is the
** caller's (SFML in the game, a fake in the tests), so is playing them.
*/
typedef struct song {
    char file[SONG_FILE_MAX];
    char title[64];
    char artist[64];
    char license[32];
    float default_offset;         /* seconds, from songs.txt               */
    float duration;               /* seconds, from the decoder             */
} song_t;

typedef struct library {
    song_t *songs;                /* sorted by title, case-insensitive     */
    size_t count;
} library_t;

/* Opens the file far enough to know it decodes, and its length. */
typedef bool (*song_probe_fn)(const char *path, float *duration);

/*
** Lists `dir`, keeps the .ogg, .wav, .flac and .mp3 files that decode, then
** merges dir/songs.txt (4.3). Logs what it skips. A missing folder is an
** empty library.
*/
void library_scan(library_t *lib, const char *dir, song_probe_fn probe,
    sim_log_fn log);

/* songs.txt's text, merged into the songs already found (4.3). */
void library_merge_meta(library_t *lib, const char *text, sim_log_fn log);

const song_t *library_find(const library_t *lib, const char *file);
void library_free(library_t *lib);

/*
** A name the level header, songs.txt and the progress file can all hold: no
** folder, no space, none of `#|=`, a known extension.
*/
bool song_file_name_ok(const char *name);

typedef enum song_source {
    SONG_OVERRIDE,                /* the player's, for this level (4.6)    */
    SONG_LEVEL,                   /* the level's `music`                   */
    SONG_DEFAULT,                 /* DEFAULT_LEVEL_SONG                    */
    SONG_NONE                     /* nothing to play                       */
} song_source_t;

typedef struct song_choice {
    const song_t *song;           /* NULL with SONG_NONE                   */
    song_source_t source;
    double offset;                /* where the song starts, seconds        */
} song_choice_t;

/*
** Which song a level plays and from where (4.4), trying the rules from
** `from` on: a song that fails to open is retried with `from` = its source
** plus one.
*/
song_choice_t music_choose(const library_t *lib, const char *override,
    const level_header_t *hdr, song_source_t from);

/* The menu's song: the setting's, else the library's first, else NULL. */
const song_t *music_menu_song(const library_t *lib, const char *menu_song);

/* Where the song should be at this tick (4.8). */
double music_expected(double level_offset, long tick, double audio_offset);

bool music_drifted(double expected, double actual);

/* The slider's 0-100 as sfMusic's volume: perceived loudness (4.7). */
float music_volume_curve(int volume);

/* Seconds from the start to the completion, at the level's own speed. */
double level_duration(const level_data_t *lvl);

#endif
