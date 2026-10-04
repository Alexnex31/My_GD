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

void music_play_from(music_manager_t *m, double seconds)
{
    if (m->current == NULL)
        return;
    sfMusic_stop(m->current);
    sfMusic_setPlayingOffset(m->current, sfSeconds((float)seconds));
    sfMusic_play(m->current);
    m->last_check_ms = ui_now_ms();
}

void music_stop(music_manager_t *m)
{
    if (m->current != NULL)
        sfMusic_stop(m->current);
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

    if (m->current == NULL || now - m->last_check_ms < 1000
        || sfMusic_getStatus(m->current) != sfPlaying)
        return;
    m->last_check_ms = now;
    expected = music_expected(m->level_offset, tick, m->audio_offset);
    if (music_drifted(expected, sfTime_asSeconds(
        sfMusic_getPlayingOffset(m->current))))
        sfMusic_setPlayingOffset(m->current, sfSeconds((float)expected));
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
void music_preview(gd_t *gd, const song_t *s)
{
    music_manager_t *m = &gd->music;

    if (s == NULL || music_load(m, s->file) != 0)
        return music_menu(gd);
    sfMusic_setLoop(m->current, sfFalse);
    music_play_from(m, s->default_offset);
    m->preview_until_ms = ui_now_ms() + PREVIEW_MS;
}

/* Per frame in the menus: a preview stops after its 10 s. */
void music_update(gd_t *gd)
{
    music_manager_t *m = &gd->music;

    if (m->preview_until_ms != 0 && ui_now_ms() >= m->preview_until_ms) {
        music_stop(m);
        m->preview_until_ms = -1;            /* over, until the next one */
    }
}
