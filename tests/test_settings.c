/*
** ALEXNEX PROJECT, 2026
** tests/test_settings.c
** File description:
** the settings store: defaults, ranges, bindings, unknown keys (FEATURES 2)
*/

#include <dirent.h>
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
        KEY_Space, 0}));
    CHECK(binding_equal(s.jump_bindings.items[2], (binding_t){BIND_MOUSE,
        MOUSE_Left, 0}));
    CHECK(binding_equal(s.jump_bindings.items[3], (binding_t){BIND_JOY, 0, 0}));
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_KEY, KEY_R, 0}));
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
        MOUSE_Right, 0}));
    CHECK(binding_equal(s.jump_bindings.items[2], (binding_t){BIND_JOY, 15, 0}));
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
        "icon_cube=", "icon_ship=../ship.png", "icon_ufo=.hidden.png",
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
        for (int m = 0; m < MODE_COUNT; m++)
            CHECK(strcmp(s.icons[m], def.icons[m]) == 0);
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
    CHECK(binding_equal(s.checkpoint_key, (binding_t){BIND_KEY, KEY_Z, 0}));
    CHECK(nb_logged == 1 && strstr(logged, "checkpoint_key=Space") != NULL);
    settings_free(&s);
    load(&s, "jump_bindings=R,Space\n");             /* R's default is taken */
    CHECK(s.restart_key.kind == BIND_NONE);
    CHECK(strstr(logged, "unbound") != NULL);
    settings_free(&s);
    load(&s, "restart_key=Z\n");                     /* checkpoint's default */
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_KEY, KEY_Z, 0}));
    CHECK(binding_equal(s.checkpoint_key, (binding_t){BIND_KEY, KEY_Z, 0})
        == false);
    CHECK(s.checkpoint_key.kind == BIND_NONE);
    settings_free(&s);
    load(&s, "restart_key=MouseRight\n");
    CHECK(nb_logged == 0);
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_MOUSE, MOUSE_Right, 0}));
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
    s.jump_bindings = (bindings_t){{{BIND_KEY, KEY_Numpad0, 0},
        {BIND_MOUSE, MOUSE_Middle, 0}}, 2};
    s.remove_checkpoint_key = (binding_t){BIND_NONE, 0, 0};
    s.audio_offset_ms = 120;
    snprintf(s.menu_song, sizeof(s.menu_song), "%s", "x.ogg");
    snprintf(s.icons[MODE_SPIDER], SETTING_FILE_MAX, "%s", "legs 2.png");
    CHECK(settings_save(&s) == 0);
    CHECK(strstr(read_file(), "icon_spider=legs 2.png\n") != NULL);
    CHECK(strstr(read_file(), "icon_cube=player_icon.png\n") != NULL);
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
        MOUSE_Middle, 0}));
    CHECK(back.remove_checkpoint_key.kind == BIND_NONE);
    CHECK(back.audio_offset_ms == 120);
    CHECK(strcmp(back.menu_song, "x.ogg") == 0);
    CHECK(strcmp(back.icons[MODE_SPIDER], "legs 2.png") == 0);
    CHECK(strcmp(back.icons[MODE_SHIP], "ship_icon.png") == 0);
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

/*
** A line is whole however long: a long comment never applies the key at its
** end, and a long unknown value is written back as it was. A newer build's
** key with digits in it is kept too.
*/
static void test_long_lines(void)
{
    settings_t s;
    char *text = calloc(8192, 1);
    char *back = calloc(8192, 1);
    char value[3001];
    FILE *f = NULL;

    memset(value, 'v', sizeof(value) - 1);
    value[sizeof(value) - 1] = '\0';
    memset(text, '#', 1500);
    snprintf(text + 1500, 8192 - 1500, "music_volume=5\nfuture=%s\n"
        "volume2=50\n", value);
    load(&s, text);
    CHECK(s.music_volume == 80 && nb_logged == 0);
    CHECK(s.nb_extra == 2 && strncmp(s.extra[0], "future=", 7) == 0);
    CHECK(s.nb_extra == 2 && strlen(s.extra[0]) == 7 + strlen(value));
    CHECK(s.nb_extra == 2 && strcmp(s.extra[1], "volume2=50") == 0);
    CHECK(settings_save(&s) == 0);
    f = fopen(path, "r");
    if (f != NULL) {
        back[fread(back, 1, 8191, f)] = '\0';
        fclose(f);
    }
    snprintf(text, 8192, "\nfuture=%s\nvolume2=50\n", value);
    CHECK(strstr(back, text) != NULL);
    settings_free(&s);
    free(text);
    free(back);
}

/* What a folder holds, "." and ".." aside. */
static int nb_files(const char *dir)
{
    DIR *d = opendir(dir);
    int n = 0;

    if (d == NULL)
        return -1;
    for (struct dirent *e = readdir(d); e != NULL; e = readdir(d))
        n += e->d_name[0] != '.';
    closedir(d);
    return n;
}

/* The folder is made, and nothing is left behind but the file (2.4). */
static void test_save_makes_its_folder(void)
{
    settings_t s;
    char nested[128];
    char dir[128];

    snprintf(nested, sizeof(nested), "%s/save/settings.txt", tmp_dir);
    snprintf(dir, sizeof(dir), "%s/save", tmp_dir);
    settings_load(&s, nested, capture);
    CHECK(settings_save(&s) == 0);
    CHECK(access(nested, R_OK) == 0 && nb_files(dir) == 1);
    settings_free(&s);
    remove(nested);
    rmdir(dir);
}

/* Every name reads back as itself; any case reads (2.2). */
static void test_binding_names(void)
{
    char name[BINDING_NAME_MAX];
    binding_t b;

    for (int k = 0; k < KEY_COUNT; k++) {
        binding_format((binding_t){BIND_KEY, k, 0}, name, sizeof(name));
        CHECK(binding_parse(name, &b) && b.kind == BIND_KEY && b.code == k);
    }
    for (int m = 0; m < MOUSE_COUNT; m++) {
        binding_format((binding_t){BIND_MOUSE, m, 0}, name, sizeof(name));
        CHECK(binding_parse(name, &b) && b.kind == BIND_MOUSE && b.code == m);
    }
    for (int j = 0; j < JOY_BUTTONS; j++) {
        binding_format((binding_t){BIND_JOY, j, 0}, name, sizeof(name));
        CHECK(binding_parse(name, &b) && b.kind == BIND_JOY && b.code == j);
    }
    CHECK(binding_parse("pAgEuP", &b) && b.code == KEY_PageUp);
    CHECK(!binding_parse("Joy", &b) && !binding_parse("Joy-1", &b));
    CHECK(!binding_parse("Joy1x", &b) && !binding_parse("MouseX1", &b));
    CHECK(!binding_parse("", &b) && !binding_parse("Space ", &b));
}

/* Combinations: modifiers in any order and case, written in one order. */
static void test_combo_names(void)
{
    char name[BINDING_NAME_MAX];
    binding_t b;

    CHECK(binding_parse("shift+CTRL+z", &b) && b.kind == BIND_KEY
        && b.code == KEY_Z && b.mods == (MOD_CTRL | MOD_SHIFT));
    binding_format(b, name, sizeof(name));
    CHECK(strcmp(name, "Ctrl+Shift+Z") == 0);
    binding_format((binding_t){BIND_KEY, KEY_Backspace, MOD_CTRL | MOD_SHIFT
        | MOD_ALT}, name, sizeof(name));
    CHECK(strcmp(name, "Ctrl+Shift+Alt+Backspace") == 0);
    CHECK(binding_parse(name, &b) && b.mods == 7 && b.code == KEY_Backspace);
    CHECK(!binding_parse("Ctrl+Ctrl+S", &b) && !binding_parse("Ctrl+", &b));
    CHECK(!binding_parse("Ctrl+MouseLeft", &b) && !binding_parse("Alt+Joy1", &b));
    CHECK(!binding_equal((binding_t){BIND_KEY, KEY_S, 0},
        (binding_t){BIND_KEY, KEY_S, MOD_CTRL}));
}

/*
** The editor's shortcuts (11.6): defaults with no two alike and none a
** punctuation key; combinations read and written back; what can't be one
** keeps its default; two alike, the later one gives way. The game's keys
** take no modifiers.
*/
static void test_editor_keys(void)
{
    settings_t s;
    settings_t back;
    ed_key_t page[ED_KEY_PAGE_MAX];
    int listed = 0;

    load(&s, NULL);
    for (int k = 0; k < ED_KEY_COUNT; k++) {
        binding_t b = s.editor_keys[k];

        CHECK(ed_key_bindable(b) && b.kind == BIND_KEY);
        CHECK(ed_key_find(s.editor_keys, b) == k);   /* no two alike */
        CHECK(b.code < KEY_LBracket || b.code > KEY_Hyphen);
    }
    for (int p = 0; p < ED_KEY_PAGES; p++)
        listed += ed_key_page_list(p, page);
    CHECK(listed == ED_KEY_COUNT);                   /* every one on a page */
    settings_free(&s);
    load(&s, "editor_save=alt+shift+s\neditor_undo=Escape\n"
        "editor_redo=LControl\neditor_copy=Ctrl+MouseLeft\n"
        "editor_grid=\nrestart_key=Ctrl+R\n");
    CHECK(binding_equal(s.editor_keys[ED_KEY_SAVE],
        (binding_t){BIND_KEY, KEY_S, MOD_ALT | MOD_SHIFT}));
    CHECK(binding_equal(s.editor_keys[ED_KEY_UNDO],
        ed_key_default(ED_KEY_UNDO)));
    CHECK(binding_equal(s.editor_keys[ED_KEY_REDO],
        ed_key_default(ED_KEY_REDO)));
    CHECK(binding_equal(s.editor_keys[ED_KEY_COPY],
        ed_key_default(ED_KEY_COPY)));
    CHECK(s.editor_keys[ED_KEY_GRID].kind == BIND_NONE);
    CHECK(binding_equal(s.restart_key, (binding_t){BIND_KEY, KEY_R, 0}));
    CHECK(nb_logged == 4);
    s.dirty = true;
    CHECK(settings_save(&s) == 0);
    CHECK(strstr(read_file(), "editor_save=Ctrl+S") == NULL);
    CHECK(strstr(read_file(), "editor_save=Shift+Alt+S\n") != NULL);
    CHECK(strstr(read_file(), "editor_grid=\n") != NULL);
    settings_load(&back, path, NULL);
    for (int k = 0; k < ED_KEY_COUNT; k++)
        CHECK(binding_equal(back.editor_keys[k], s.editor_keys[k]));
    settings_free(&back);
    settings_free(&s);
    load(&s, "editor_copy=Ctrl+S\n");               /* Save's: Copy gives way */
    CHECK(binding_equal(s.editor_keys[ED_KEY_SAVE],
        (binding_t){BIND_KEY, KEY_S, MOD_CTRL}));
    CHECK(binding_equal(s.editor_keys[ED_KEY_COPY],
        ed_key_default(ED_KEY_COPY)));
    CHECK(nb_logged == 1 && strstr(logged, "editor_copy=Ctrl+S") != NULL);
    settings_free(&s);
    load(&s, "editor_copy=Ctrl+S\neditor_save=Ctrl+C\n");   /* swapped: fine */
    CHECK(nb_logged == 0);
    CHECK(binding_equal(s.editor_keys[ED_KEY_COPY],
        (binding_t){BIND_KEY, KEY_S, MOD_CTRL}));
    settings_free(&s);
    load(&s, "editor_save=Ctrl+C\neditor_copy=Ctrl+C\n");   /* its default
                                                        taken too: none */
    CHECK(s.editor_keys[ED_KEY_COPY].kind == BIND_NONE);
    settings_free(&s);
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
    test_long_lines();
    test_save_makes_its_folder();
    test_binding_names();
    test_combo_names();
    test_editor_keys();
    remove(path);
    rmdir(tmp_dir);
}
