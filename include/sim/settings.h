/*
** ALEXNEX PROJECT, 2026
** sim/settings.h
** File description:
** header file for my_gd project
*/

#ifndef SIM_SETTINGS_H
    #define SIM_SETTINGS_H

    #include "sim/binding.h"
    #include "sim/sim_types.h"

    #define SETTINGS_PATH "save/settings.txt"
    #define BINDINGS_MAX 6

typedef struct bindings {
    binding_t items[BINDINGS_MAX];
    int count;
} bindings_t;

/*
** The player's preferences (FEATURES 2.2). Nothing here may reach the
** simulation: settings differ between players, physics can't (2.5).
*/
typedef struct settings {
    int music_volume;             /* 0-100                                   */
    int sfx_volume;               /* 0-100                                   */
    char menu_song[128];          /* a file name in music/ (4.2)             */
    bool fullscreen;
    int window_width;             /* window.c also caps it to the desktop    */
    int window_height;
    bool vsync;
    int fps_limit;                /* 0 = none; ignored while vsync is on     */
    bool show_percent;
    bool show_progress_bar;
    bool show_attempts;
    bindings_t jump_bindings;
    binding_t restart_key;        /* BIND_NONE when unbound                  */
    binding_t checkpoint_key;
    binding_t remove_checkpoint_key;
    int audio_offset_ms;          /* -300-300 (4.8)                          */
    char **extra;                 /* unknown "key=value" lines, kept as is   */
    size_t nb_extra;
    bool unreadable;              /* the file exists but couldn't be read:
                                     never overwritten                       */
    char path[256];
} settings_t;

void settings_defaults(settings_t *s);

/*
** Defaults, then the file's valid values. A missing file is no warning; an
** invalid value keeps its default and logs its line. 0, or -1 when the file
** exists but can't be read (defaults, one warning).
*/
int settings_load(settings_t *s, const char *path, sim_log_fn log);

/* Through a .tmp and a rename, unknown keys written back (2.4). */
int settings_save(const settings_t *s);

void settings_free(settings_t *s);

bool bindings_contain(const bindings_t *b, binding_t one);

#endif
