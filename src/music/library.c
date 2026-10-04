/*
** ALEXNEX PROJECT, 2026
** music/library.c
** File description:
** the song library, which song a level plays, and the sync rule (FEATURES 4)
*/

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "music/library.h"
#include "sim/alloc.h"
#include "sim/constants.h"

#define LINE_LEN 512
#define MSG_LEN 1200

static const char *const EXTENSIONS[] = {".ogg", ".wav", ".flac", ".mp3"};

static void say(sim_log_fn log, const char *msg)
{
    if (log != NULL)
        log(msg);
}

static bool known_extension(const char *name)
{
    const char *dot = strrchr(name, '.');

    for (size_t i = 0; dot != NULL && i < 4; i++)
        if (strcasecmp(dot, EXTENSIONS[i]) == 0)
            return true;
    return false;
}

bool song_file_name_ok(const char *name)
{
    size_t n = strlen(name);

    if (n == 0 || n >= SONG_FILE_MAX || name[0] == '.')
        return false;
    for (size_t i = 0; i < n; i++)
        if (!isgraph((unsigned char)name[i]) || strchr("/\\#|=", name[i]))
            return false;
    return known_extension(name);
}

static void add_song(library_t *lib, const char *file, float duration)
{
    song_t *grown = sim_xcalloc(lib->count + 1, sizeof(song_t));
    song_t *s = &grown[lib->count];
    const char *dot = strrchr(file, '.');

    if (lib->songs != NULL)
        memcpy(grown, lib->songs, lib->count * sizeof(song_t));
    free(lib->songs);
    lib->songs = grown;
    lib->count += 1;
    snprintf(s->file, sizeof(s->file), "%.*s", SONG_FILE_MAX - 1, file);
    snprintf(s->title, sizeof(s->title), "%.*s", (int)(dot - file), file);
    snprintf(s->artist, sizeof(s->artist), "%s", "Unknown");
    snprintf(s->license, sizeof(s->license), "%s", "?");
    s->duration = duration;
}

const song_t *library_find(const library_t *lib, const char *file)
{
    for (size_t i = 0; file != NULL && i < lib->count; i++)
        if (strcmp(lib->songs[i].file, file) == 0)
            return &lib->songs[i];
    return NULL;
}

static char *trim(char *s)
{
    char *end = NULL;

    while (isspace((unsigned char)*s))
        s += 1;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        end -= 1;
    *end = '\0';
    return s;
}

/* The fields after the file name, each optional (4.3). */
static void set_meta(song_t *s, char **f, int n, int line, sim_log_fn log)
{
    char msg[MSG_LEN];
    char *end = NULL;
    double off = 0.0;

    if (n > 1 && f[1][0] != '\0')
        snprintf(s->title, sizeof(s->title), "%s", f[1]);
    if (n > 2 && f[2][0] != '\0')
        snprintf(s->artist, sizeof(s->artist), "%s", f[2]);
    if (n > 3 && f[3][0] != '\0')
        snprintf(s->license, sizeof(s->license), "%s", f[3]);
    if (n <= 4 || f[4][0] == '\0')
        return;
    off = strtod(f[4], &end);
    if (*end == '\0' && off >= 0.0 && off < s->duration) {
        s->default_offset = (float)off;
        return;
    }
    snprintf(msg, sizeof(msg), "%s:%d: offset %s isn't within %s's %.2f s,"
        " using 0", SONGS_META, line, f[4], s->file, s->duration);
    say(log, msg);
}

static void merge_line(library_t *lib, char *line, int nb, sim_log_fn log)
{
    char *f[5] = {NULL};
    char msg[MSG_LEN];
    int n = 0;
    song_t *s = NULL;

    for (char *p = line; p != NULL && n < 5; n++) {
        char *bar = strchr(p, '|');

        if (bar != NULL)
            *bar = '\0';
        f[n] = trim(p);
        p = bar == NULL ? NULL : bar + 1;
    }
    s = (song_t *)library_find(lib, f[0]);
    if (s != NULL)
        return set_meta(s, f, n, nb, log);
    snprintf(msg, sizeof(msg), "%s:%d: %s isn't a song of the library,"
        " ignored", SONGS_META, nb, f[0]);
    say(log, msg);
}

void library_merge_meta(library_t *lib, const char *text, sim_log_fn log)
{
    char line[LINE_LEN];
    int nb = 0;

    while (*text != '\0') {
        size_t len = strcspn(text, "\n");
        char *t = NULL;

        snprintf(line, sizeof(line), "%.*s", (int)(len < LINE_LEN ? len
            : LINE_LEN - 1), text);
        text += len + (text[len] == '\n');
        nb += 1;
        t = trim(line);
        if (t[0] != '\0' && t[0] != '#')
            merge_line(lib, t, nb, log);
    }
}

static int by_title(const void *a, const void *b)
{
    const song_t *x = a;
    const song_t *y = b;
    int c = strcasecmp(x->title, y->title);

    return c != 0 ? c : strcmp(x->file, y->file);
}

static void read_meta(library_t *lib, const char *dir, sim_log_fn log)
{
    char path[1024];
    char *text = NULL;
    FILE *f = NULL;
    long size = 0;

    snprintf(path, sizeof(path), "%s/%s", dir, SONGS_META);
    f = fopen(path, "r");
    if (f == NULL)
        return;                              /* optional: defaults for all */
    if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) >= 0
        && fseek(f, 0, SEEK_SET) == 0) {
        text = sim_xcalloc((size_t)size + 1, 1);
        text[fread(text, 1, (size_t)size, f)] = '\0';
        library_merge_meta(lib, text, log);
        free(text);
    }
    fclose(f);
}

/* One file of the folder: kept if its name is usable and it decodes. */
static void consider(library_t *lib, const char *dir, const char *name,
    song_probe_fn probe, sim_log_fn log)
{
    char path[1024];
    char msg[MSG_LEN];
    float duration = 0.0f;

    if (name[0] == '.' || !known_extension(name))
        return;                              /* songs.txt, hidden files... */
    if (!song_file_name_ok(name)) {
        snprintf(msg, sizeof(msg), "%s/%s: a song's name can't have spaces"
            " or any of #|=, skipped", dir, name);
        return say(log, msg);
    }
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (!probe(path, &duration)) {
        snprintf(msg, sizeof(msg), "%s: can't decode, skipped", path);
        return say(log, msg);
    }
    add_song(lib, name, duration);
}

void library_scan(library_t *lib, const char *dir, song_probe_fn probe,
    sim_log_fn log)
{
    DIR *d = opendir(dir);
    char msg[MSG_LEN];

    *lib = (library_t){0};
    if (d == NULL) {
        snprintf(msg, sizeof(msg), "%s/ not found: no songs", dir);
        return say(log, msg);
    }
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d))
        consider(lib, dir, e->d_name, probe, log);
    closedir(d);
    read_meta(lib, dir, log);
    if (lib->count > 0)
        qsort(lib->songs, lib->count, sizeof(song_t), by_title);
}

void library_free(library_t *lib)
{
    free(lib->songs);
    *lib = (library_t){0};
}

/*
** The override, the level's song, the default (4.4). The level's offset
** only goes with the level's own song: an override starts at its own.
*/
song_choice_t music_choose(const library_t *lib, const char *override,
    const level_header_t *hdr, song_source_t from)
{
    const song_t *s = NULL;

    if (from <= SONG_OVERRIDE && override != NULL && override[0] != '\0'
        && (s = library_find(lib, override)) != NULL)
        return (song_choice_t){s, SONG_OVERRIDE, s->default_offset};
    if (from <= SONG_LEVEL && hdr->music[0] != '\0'
        && (s = library_find(lib, hdr->music)) != NULL)
        return (song_choice_t){s, SONG_LEVEL, hdr->music_offset > 0.0
            ? hdr->music_offset : s->default_offset};
    if (from <= SONG_DEFAULT
        && (s = library_find(lib, DEFAULT_LEVEL_SONG)) != NULL)
        return (song_choice_t){s, SONG_DEFAULT, s->default_offset};
    return (song_choice_t){NULL, SONG_NONE, 0.0};
}

const song_t *music_menu_song(const library_t *lib, const char *menu_song)
{
    const song_t *s = library_find(lib, menu_song);

    if (s != NULL || lib->count == 0)
        return s;
    return &lib->songs[0];
}

double music_expected(double level_offset, long tick, double audio_offset)
{
    return level_offset + (double)tick / TICK_RATE + audio_offset;
}

bool music_drifted(double expected, double actual)
{
    return fabs(actual - expected) > RESYNC_S;
}

float music_volume_curve(int volume)
{
    float v = (float)(volume < 0 ? 0 : (volume > 100 ? 100 : volume));

    return 100.0f * powf(v / 100.0f, 2.0f);
}

double level_duration(const level_data_t *lvl)
{
    double mult = lvl->hdr.start.speed_mult > 0.0 ? lvl->hdr.start.speed_mult
        : 1.0;

    return lvl->end_shift / (SCROLL_SPEED * mult);
}
