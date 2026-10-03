/*
** ALEXNEX PROJECT, 2026
** tests/test_settings.c
** File description:
** the settings store: defaults, ranges, bindings, unknown keys (FEATURES 2)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sim/settings.h"
#include "test.h"

/* A folder of its own under /tmp: the player's save/ is never touched. */
static char tmp_dir[] = "/tmp/my_gd_settingsXXXXXX";
static char path[96];
static char logged[4096];
static int nb_logged;

static void capture(const char *msg)
{
    size_t used = strlen(logged);

    snprintf(logged + used, sizeof(logged) - used, "%s\n", msg);
    nb_logged += 1;
}

static void write_file(const char *text)
{
    FILE *f = fopen(path, "w");

    if (f == NULL)
        return;
    fputs(text, f);
    fclose(f);
}

static char *read_file(void)
{
    static char buf[4096];
    FILE *f = fopen(path, "r");
    size_t n = 0;

    buf[0] = '\0';
    if (f == NULL)
        return buf;
    n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static int load(settings_t *s, const char *text)
{
    logged[0] = '\0';
    nb_logged = 0;
    if (text != NULL)
        write_file(text);
    else
        remove(path);
    return settings_load(s, path, capture);
}

static void test_missing_file(void)
{
    settings_t s;

    CHECK(load(&s, NULL) == 0);
    CHECK(nb_logged == 0);
    CHECK(s.music_volume == 80 && s.sfx_volume == 100);
    CHECK(strcmp(s.menu_song, "menu_loop.ogg") == 0);
    CHECK(!s.fullscreen && s.vsync && s.fps_limit == 0);
    CHECK(s.window_width == 1280 && s.window_height == 720);
    CHECK(s.show_percent && s.show_progress_bar && s.show_attempts);
    CHECK(s.jump_bindings.count == 4);
    CHECK(binding_equal(s.jump_bindings.items[0], (binding_t){BIND_KEY,
        KEY_Space}));
    CHECK(binding_equal(s.jump_bindings.items[2], (binding_t){BIND_MOUSE,
        MOUSE_Left}));
    CHECK(binding_equal(s.jump_bindings.items[3], (binding_t){BIND_JOY, 0}));
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_KEY, KEY_R}));
    CHECK(s.audio_offset_ms == 0);
    settings_free(&s);
}

static void test_values_and_spaces(void)
{
    settings_t s;

    CHECK(load(&s, "# a comment\n\n  music_volume =  42 \nvsync=0\n"
        "fps_limit=144\njump_bindings= space , mouseright,Joy15\n"
        "audio_offset_ms=-300\nrestart_key=\nmenu_song=My Song.ogg\n"
        "sfx_volume=5\nsfx_volume=6\n") == 0);
    CHECK(nb_logged == 0);
    CHECK(s.music_volume == 42 && !s.vsync && s.fps_limit == 144);
    CHECK(s.jump_bindings.count == 3);
    CHECK(binding_equal(s.jump_bindings.items[1], (binding_t){BIND_MOUSE,
        MOUSE_Right}));
    CHECK(binding_equal(s.jump_bindings.items[2], (binding_t){BIND_JOY, 15}));
    CHECK(s.audio_offset_ms == -300);
    CHECK(s.restart_key.kind == BIND_NONE);          /* "" is unbound */
    CHECK(strcmp(s.menu_song, "My Song.ogg") == 0);
    CHECK(s.sfx_volume == 6);                         /* the last one wins */
    settings_free(&s);
}

/* An invalid value keeps the default, and its line is named (2.3). */
static void test_invalid_values(void)
{
    static const char *const bad[] = {
        "fps_limit=75", "music_volume=101", "sfx_volume=-1",
        "window_width=abc", "window_height=359", "vsync=2", "fullscreen=",
        "menu_song=../x.ogg", "menu_song=.hidden", "menu_song=",
        "jump_bindings=Space,,Up", "jump_bindings=Space,", "jump_bindings=",
        "jump_bindings=Space,Space", "jump_bindings=Escape",
        "jump_bindings=A,B,C,D,E,F,G", "jump_bindings=Joy16",
        "jump_bindings=Mouse", "restart_key=Escape", "restart_key=Nope",
        "audio_offset_ms=301", "music_volume=99999999999999999999",
    };
    settings_t s;
    settings_t def;
    char text[64];

    settings_defaults(&def);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        snprintf(text, sizeof(text), "\n%s\n", bad[i]);
        CHECK(load(&s, text) == 0);
        CHECK(nb_logged == 1);
        CHECK(strstr(logged, ":2: ") != NULL);
        CHECK(strstr(logged, "invalid, using") != NULL);
        CHECK(s.music_volume == def.music_volume && s.fps_limit == 0);
        CHECK(s.jump_bindings.count == 4 && s.window_width == 1280);
        CHECK(binding_equal(s.restart_key, def.restart_key));
        CHECK(strcmp(s.menu_song, def.menu_song) == 0);
        settings_free(&s);
    }
    load(&s, "fps_limit=75\n");
    CHECK(strstr(logged, ":1: fps_limit=75 invalid, using 0") != NULL);
    settings_free(&s);
}

/* A key that acts in a level can't also jump (2.2). */
static void test_action_keys(void)
{
    settings_t s;

    load(&s, "checkpoint_key=Space\n");
    CHECK(binding_equal(s.checkpoint_key, (binding_t){BIND_KEY, KEY_Z}));
    CHECK(nb_logged == 1 && strstr(logged, "checkpoint_key=Space") != NULL);
    settings_free(&s);
    load(&s, "jump_bindings=R,Space\n");             /* R's default is taken */
    CHECK(s.restart_key.kind == BIND_NONE);
    CHECK(strstr(logged, "unbound") != NULL);
    settings_free(&s);
    load(&s, "restart_key=Z\n");                     /* checkpoint's default */
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_KEY, KEY_Z}));
    CHECK(binding_equal(s.checkpoint_key, (binding_t){BIND_KEY, KEY_Z})
        == false);
    CHECK(s.checkpoint_key.kind == BIND_NONE);
    settings_free(&s);
    load(&s, "restart_key=MouseRight\n");
    CHECK(nb_logged == 0);
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_MOUSE, MOUSE_Right}));
    settings_free(&s);
}

/* Saved and read back, every value is the same, unknown keys included. */
static void test_round_trip(void)
{
    settings_t s;
    settings_t back;

    load(&s, "future_thing = 3\nnot a pair\nBad_Key=1\nmusic_volume=7\n");
    CHECK(nb_logged == 2);                            /* the two bad lines */
    CHECK(s.nb_extra == 1 && strcmp(s.extra[0], "future_thing=3") == 0);
    s.sfx_volume = 33;
    s.fullscreen = true;
    s.window_width = 1920;
    s.fps_limit = 240;
    s.show_attempts = false;
    s.jump_bindings = (bindings_t){{{BIND_KEY, KEY_Numpad0},
        {BIND_MOUSE, MOUSE_Middle}}, 2};
    s.remove_checkpoint_key = (binding_t){BIND_NONE, 0};
    s.audio_offset_ms = 120;
    snprintf(s.menu_song, sizeof(s.menu_song), "%s", "x.ogg");
    CHECK(settings_save(&s) == 0);
    CHECK(strstr(read_file(), "future_thing=3\n") != NULL);
    CHECK(strstr(read_file(), "remove_checkpoint_key=\n") != NULL);
    CHECK(strstr(read_file(), "jump_bindings=Numpad0,MouseMiddle\n") != NULL);
    CHECK(strstr(read_file(), "not a pair") == NULL);
    CHECK(settings_load(&back, path, capture) == 0);
    CHECK(back.music_volume == 7 && back.sfx_volume == 33);
    CHECK(back.fullscreen && back.window_width == 1920);
    CHECK(back.fps_limit == 240 && !back.show_attempts);
    CHECK(back.jump_bindings.count == 2);
    CHECK(binding_equal(back.jump_bindings.items[1], (binding_t){BIND_MOUSE,
        MOUSE_Middle}));
    CHECK(back.remove_checkpoint_key.kind == BIND_NONE);
    CHECK(back.audio_offset_ms == 120);
    CHECK(strcmp(back.menu_song, "x.ogg") == 0);
    CHECK(back.nb_extra == 1);
    settings_free(&s);
    settings_free(&back);
}

/* A file that can't be read is never replaced by the defaults (2.3). */
static void test_unreadable(void)
{
    settings_t s;
    char dir_path[128];

    snprintf(dir_path, sizeof(dir_path), "%s/folder.txt", tmp_dir);
    mkdir(dir_path, 0755);
    logged[0] = '\0';
    nb_logged = 0;
    CHECK(settings_load(&s, dir_path, capture) == -1);
    CHECK(nb_logged == 1 && s.unreadable && s.music_volume == 80);
    CHECK(settings_save(&s) == -1);
    settings_free(&s);
    rmdir(dir_path);
    CHECK(settings_load(&s, "/dev/null", capture) == -1);   /* not a file */
    CHECK(s.unreadable);                     /* and never saved: see below */
    settings_free(&s);
    if (geteuid() == 0)
        return;                              /* root reads a mode 000 file */
    write_file("music_volume=3\n");
    chmod(path, 0);
    CHECK(settings_load(&s, path, capture) == -1 && s.music_volume == 80);
    CHECK(settings_save(&s) == -1);
    settings_free(&s);
    chmod(path, 0644);
    CHECK(strcmp(read_file(), "music_volume=3\n") == 0);   /* untouched */
}

/* The folder is made, and nothing is left behind but the file (2.4). */
static void test_save_makes_its_folder(void)
{
    settings_t s;
    char nested[128];
    char tmp[140];

    snprintf(nested, sizeof(nested), "%s/save/settings.txt", tmp_dir);
    snprintf(tmp, sizeof(tmp), "%s.tmp", nested);
    settings_load(&s, nested, capture);
    CHECK(settings_save(&s) == 0);
    CHECK(access(nested, R_OK) == 0 && access(tmp, F_OK) != 0);
    settings_free(&s);
    remove(nested);
    snprintf(nested, sizeof(nested), "%s/save", tmp_dir);
    rmdir(nested);
}

/* Every name reads back as itself; any case reads (2.2). */
static void test_binding_names(void)
{
    char name[BINDING_NAME_MAX];
    binding_t b;

    for (int k = 0; k < KEY_COUNT; k++) {
        binding_format((binding_t){BIND_KEY, k}, name, sizeof(name));
        CHECK(binding_parse(name, &b) && b.kind == BIND_KEY && b.code == k);
    }
    for (int m = 0; m < MOUSE_COUNT; m++) {
        binding_format((binding_t){BIND_MOUSE, m}, name, sizeof(name));
        CHECK(binding_parse(name, &b) && b.kind == BIND_MOUSE && b.code == m);
    }
    for (int j = 0; j < JOY_BUTTONS; j++) {
        binding_format((binding_t){BIND_JOY, j}, name, sizeof(name));
        CHECK(binding_parse(name, &b) && b.kind == BIND_JOY && b.code == j);
    }
    CHECK(binding_parse("pAgEuP", &b) && b.code == KEY_PageUp);
    CHECK(!binding_parse("Joy", &b) && !binding_parse("Joy-1", &b));
    CHECK(!binding_parse("Joy1x", &b) && !binding_parse("MouseX1", &b));
    CHECK(!binding_parse("", &b) && !binding_parse("Space ", &b));
}

void test_settings(void)
{
    if (mkdtemp(tmp_dir) == NULL) {
        CHECK(!"mkdtemp");
        return;
    }
    snprintf(path, sizeof(path), "%s/settings.txt", tmp_dir);
    test_missing_file();
    test_values_and_spaces();
    test_invalid_values();
    test_action_keys();
    test_round_trip();
    test_unreadable();
    test_save_makes_its_folder();
    test_binding_names();
    remove(path);
    rmdir(tmp_dir);
}
