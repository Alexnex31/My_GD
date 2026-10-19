/*
** ALEXNEX PROJECT, 2026
** music_manager
** File description:
** the one playing sfMusic: levels, menus, previews, sync (FEATURES 4.7, 4.8)
*/

#include "mygd.h"

#define PREVIEW_MS 10000               /* the song picker plays this much (4.6) */

/* The library's decoder: opens the headers, reads the length, closes. */
bool music_probe(const char *path, float *duration)
{
    sfMusic *m = sfMusic_createFromFile(path);

    if (m == NULL)
        return false;
    *duration = sfTime_asSeconds(sfMusic_getDuration(m));
    sfMusic_destroy(m);
    return true;
}

/*
** Where the menu song was, so coming back resumes it (4.10). By who plays,
** not by file: a level may well play the menu's song.
*/
static void remember_menu(music_manager_t *m)
{
    if (m->current != NULL && m->menu_playing)
        m->menu_position = sfTime_asSeconds(
            sfMusic_getPlayingOffset(m->current));
    m->menu_playing = false;
}

/*
** Only one sfMusic at a time, and the old one goes after the new one opened:
** a missing file never leaves silence and a dangling pointer (4.7).
*/
int music_load(music_manager_t *m, const char *file)
{
    char path[256];
    sfMusic *next = NULL;

    m->start_at_ms = 0;
    if (m->current != NULL && strcmp(m->current_file, file) == 0) {
        remember_menu(m);                    /* the same file, a new owner */
        m->preview_until_ms = 0;
        return 0;
    }
    snprintf(path, sizeof(path), "%s/%s", MUSIC_DIR, file);
    next = sfMusic_createFromFile(path);
    if (next == NULL)
        return -1;
    remember_menu(m);
    if (m->current != NULL)
        sfMusic_destroy(m->current);
    m->current = next;
    snprintf(m->current_file, sizeof(m->current_file), "%s", file);
    sfMusic_setVolume(m->current, music_volume_curve(m->volume));
    m->preview_until_ms = 0;
    return 0;
}

/* The song's length when it loops, 0 when it doesn't (music_seek_point). */
static double loop_len(const music_manager_t *m)
{
    if (!sfMusic_getLoop(m->current))
        return 0.0;
    return sfTime_asSeconds(sfMusic_getDuration(m->current));
}

/*
** SFML only seeks a stream that plays or is paused: a stopped one starts
** from 0 whatever it was told, so it is started first. A position before
** the song's start is a song due later: it starts from 0 when it's time.
*/
void music_play_from(music_manager_t *m, double seconds)
{
    if (m->current == NULL)
        return;
    m->last_check_ms = ui_now_ms();
    m->start_at_ms = 0;
    if (seconds < 0.0) {
        sfMusic_stop(m->current);
        m->start_at_ms = m->last_check_ms + (int64_t)(-seconds * 1000.0) + 1;
        return;
    }
    if (sfMusic_getStatus(m->current) == sfStopped)
        sfMusic_play(m->current);
    sfMusic_setPlayingOffset(m->current,
        sfSeconds((float)music_seek_point(seconds, loop_len(m))));
    sfMusic_play(m->current);
}

/* Where the level's song is at that tick, `ahead` seconds past it (4.8). */
void music_play_at_tick(music_manager_t *m, long tick, double ahead)
{
    music_play_from(m, music_expected(m->level_offset, tick, m->audio_offset)
        + ahead);
}

/* A song due later starts now that it's time. */
static void start_when_due(music_manager_t *m, int64_t now)
{
    if (m->start_at_ms == 0 || now < m->start_at_ms || m->current == NULL)
        return;
    m->start_at_ms = 0;
    m->last_check_ms = now;
    sfMusic_play(m->current);                /* stopped: from its start */
}

void music_stop(music_manager_t *m)
{
    m->start_at_ms = 0;
    if (m->current != NULL)
        sfMusic_stop(m->current);
}

/* A level with no song: the menu song stops, and is resumed after (4.10). */
void music_silence(music_manager_t *m)
{
    remember_menu(m);
    music_stop(m);
}

/* The pause menu's (PLAN 10.4): resuming plays from the tick's place again. */
void music_pause(music_manager_t *m)
{
    m->start_at_ms = 0;
    if (m->current != NULL && sfMusic_getStatus(m->current) == sfPlaying)
        sfMusic_pause(m->current);
}

void music_set_loop(music_manager_t *m, bool loop)
{
    if (m->current != NULL)
        sfMusic_setLoop(m->current, loop);
}

void music_set_volume(music_manager_t *m, int volume)
{
    m->volume = volume;
    if (m->current != NULL)
        sfMusic_setVolume(m->current, music_volume_curve(volume));
}

void music_free(music_manager_t *m)
{
    if (m->current != NULL)
        sfMusic_destroy(m->current);
    m->current = NULL;
}

/*
** Once a second while a level plays: after a freeze the sim is behind the
** song, which never waits; past 50 ms, the song is moved back (4.8).
*/
void music_check_sync(music_manager_t *m, long tick)
{
    int64_t now = ui_now_ms();
    double expected = 0.0;

    start_when_due(m, now);
    if (m->current == NULL || now - m->last_check_ms < 1000
        || sfMusic_getStatus(m->current) != sfPlaying)
        return;
    m->last_check_ms = now;
    expected = music_expected(m->level_offset, tick, m->audio_offset);
    if (expected > 0.0 && music_drifted(expected, sfTime_asSeconds(
        sfMusic_getPlayingOffset(m->current)), loop_len(m)))
        sfMusic_setPlayingOffset(m->current,
            sfSeconds((float)music_seek_point(expected, loop_len(m))));
}

/*
** Every menu calls it: the menu song keeps playing across them, and after a
** level it picks up where it was (4.10).
*/
void music_menu(gd_t *gd)
{
    music_manager_t *m = &gd->music;

    if (m->menu_file[0] == '\0')
        return;                              /* an empty library: silence */
    if (m->menu_playing && sfMusic_getStatus(m->current) == sfPlaying)
        return;                              /* already the menu's */
    if (music_load(m, m->menu_file) != 0)
        return;
    sfMusic_setLoop(m->current, sfTrue);
    music_play_from(m, m->menu_position);
    m->menu_playing = true;
}

/* The song picker's 10 s from the song's own start (4.6). */
double music_position(const music_manager_t *m)
{
    if (m->current == NULL)
        return 0.0;
    return (double)sfTime_asSeconds(sfMusic_getPlayingOffset(m->current));
}

/* A song due later counts: it is on, only not heard yet. */
bool music_is_playing(const music_manager_t *m)
{
    return m->current != NULL && (m->start_at_ms != 0
        || sfMusic_getStatus(m->current) == sfPlaying);
}

void music_preview(gd_t *gd, const song_t *s)
{
    music_manager_t *m = &gd->music;

    if (s == NULL || music_load(m, s->file) != 0)
        return music_menu(gd);
    sfMusic_setLoop(m->current, sfFalse);
    music_play_from(m, s->default_offset);
    m->preview_until_ms = ui_now_ms() + PREVIEW_MS;
}

/* Per frame in the menus and the editor: a preview stops after its 10 s. */
void music_update(gd_t *gd)
{
    music_manager_t *m = &gd->music;

    start_when_due(m, ui_now_ms());
    if (m->preview_until_ms != 0 && ui_now_ms() >= m->preview_until_ms) {
        music_stop(m);
        m->preview_until_ms = -1;            /* over, until the next one */
    }
}
